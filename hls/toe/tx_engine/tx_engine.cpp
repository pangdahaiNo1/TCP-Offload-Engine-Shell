#include "tx_engine.hpp"
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;
/** @ingroup tx_engine
 *  @name TxEngTcpFsm
 *  The TxEngTcpFsm reads the Events from the EventEngine then it loads all
 * the necessary MetaData from the data structures (RX & TX Sar Table).
 * Depending on the Event type it generates the necessary MetaData for the
 *  txEng_ipHeader_Const and the txEng_pseudoHeader_Const.
 *
 *  Additionally it requests the IP Tuples from the Session. In some special
 * cases the IP Tuple is delivered directly from @ref rx_engine and does not
 * have to be loaded from the Session Table. The isLookUpFifo indicates this
 * special cases. Lookup Table for the current session. Depending on the Event
 * Type the retransmit or/and probe Timer is set.
 */
void TxEngTcpFsm(
    // from ack delay to tx engine req
    stream<EventWithTuple> &ack_delay_to_tx_eng_event,
    stream<ap_uint<1> > &   tx_eng_read_count_fifo,
    // rx sar read req
    stream<TcpSessionID> &  tx_eng_to_rx_sar_lup_req,
    stream<RxSarLookupRsp> &rx_sar_to_tx_eng_lup_rsp,
    // tx sar r/w req
    stream<TxEngToTxSarReq> &tx_eng_to_tx_sar_req,
    stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
    // timer
    stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
    stream<TcpSessionID> &          tx_eng_to_timer_set_ptimer,
    // to session lookup req
    stream<ap_uint<16> > &tx_eng_to_slookup_rev_table_req,
    // inner connect
    // to construct ipv4 header
    stream<ap_uint<16> > &tx_tcp_payload_length_out,
    // to construct the tcp pseudo full header
    stream<TxEngFsmMetaData> &tx_eng_fsm_meta_data_out,
    // to read mem
    stream<MemBufferRWCmd> &tx_eng_to_mem_cmd,
#if (TCP_NODELAY)
    stream<bool> &tx_eng_is_ddr_bypass,
#endif
    // fourtuple source is from tx enigne  FSM or slookup, true = slookup, false = FSM
    stream<bool> &tx_four_tuple_source,
    // four tuple from tx enigne FSM
    stream<FourTuple> &tx_eng_fsm_four_tuple_out) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum TXEngTcpFsmState { LOAD, TRANSITION };
  static TXEngTcpFsmState fsm_state = LOAD;

  static bool           tx_rx_sar_loaded = false;
  static EventWithTuple tx_eng_event_reg;
  RstEvent              rst_event;

  static ap_uint<2>       retrans_seg_cnt = 0;
  static RxSarLookupRsp   rx_sar_rsp_reg;
  TxSarToTxEngRsp         tx_sar_rsp;
  static TxSarToTxEngRsp  tx_sar_rsp_reg;
  static TxEngFsmMetaData tcp_tx_meta_reg;

  static TcpSessionBuffer curr_length;
  static TcpSessionBuffer used_length;
  static TcpSessionBuffer usable_window;
  static bool             ackd_eq_not_ackd;
  TcpSessionBuffer        usableWindow_w;

  ap_uint<16>                     next_curr_length;
  ap_uint<16>                     next_used_length;
  ap_uint<16>                     next_usable_window;
  ap_uint<32>                     txSar_not_ackd_w;
  ap_uint<32>                     txSar_ackd;
  static ap_uint<WINDOW_BITS + 1> remaining_window = 0;

  // temp output variables
  TxEngFsmMetaData       tcp_tx_meta;
  TxEngToRetransTimerReq to_rtimer_req;
  TxEngToTxSarReq        to_tx_sar_req;
  MemBufferRWCmd         to_mem_cmd;
  ap_uint<16>            slowstart_threshold;
  ap_uint<32>            payload_mem_addr;

  switch (fsm_state) {
    case LOAD:
      if (!ack_delay_to_tx_eng_event.empty()) {
        ack_delay_to_tx_eng_event.read(tx_eng_event_reg);
        logger.Info(ACK_DELAY, TX_ENGINE, "Event", tx_eng_event_reg.to_string());
        tx_eng_read_count_fifo.write(1);
        tx_rx_sar_loaded = false;
        // NOT necessary for SYN/SYN_ACK only needs one
        // to tx sar read req
        to_tx_sar_req = TxEngToTxSarReq(tx_eng_event_reg.session_id);
        switch (tx_eng_event_reg.type) {
          case RT:
          case TX:
          case SYN_ACK:
          case FIN:
          case ACK_NODELAY:
          case ACK:
            tx_eng_to_rx_sar_lup_req.write(tx_eng_event_reg.session_id);
            tx_eng_to_tx_sar_req.write(to_tx_sar_req);
            logger.Info(
                TX_ENGINE, RX_SAR_TB, "Lup Rx Req", tx_eng_event_reg.session_id.to_string(16));
            logger.Info(TX_ENGINE, TX_SAR_TB, "Lup Tx Req", to_tx_sar_req.to_string());
            break;
          case RST:
            // Get tx_sar_rsp for SEQ numb
            rst_event = tx_eng_event_reg;
            if (rst_event.has_session_id()) {
              tx_eng_to_tx_sar_req.write(to_tx_sar_req);
              logger.Info(TX_ENGINE, TX_SAR_TB, "RST-Lup Tx Req", to_tx_sar_req.to_string());
            }
            break;
          case SYN:
            if (tx_eng_event_reg.retrans_cnt != 0) {
              tx_eng_to_tx_sar_req.write(to_tx_sar_req);
              logger.Info(TX_ENGINE, TX_SAR_TB, "SYN-Lup Tx Req", to_tx_sar_req.to_string());
            }
            break;
          default:
            break;
        }
        fsm_state = TRANSITION;
      }  // if not empty
      retrans_seg_cnt = 0;
      break;
    case TRANSITION:
      switch (tx_eng_event_reg.type) {
        case TX:
          // Sends everything between tx_sar_rsp.not_ackd and tx_sar_rsp.app_written
          if ((!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) ||
              tx_rx_sar_loaded) {
            logger.Info(TX_ENGINE, "TX Seg");
            if (!tx_rx_sar_loaded) {
              rx_sar_rsp_reg = rx_sar_to_tx_eng_lup_rsp.read();
              tx_sar_rsp     = tx_sar_to_tx_eng_rsp.read();
              logger.Info(RX_SAR_TB, TX_ENGINE, "SAR Rsp", rx_sar_rsp_reg.to_string());
              logger.Info(TX_SAR_TB, TX_ENGINE, "SAR Rsp", tx_sar_rsp.to_string());

              curr_length      = tx_sar_rsp.curr_length;
              used_length      = tx_sar_rsp.used_length;
              usableWindow_w   = tx_sar_rsp.usable_window;
              ackd_eq_not_ackd = tx_sar_rsp.ackd_eq_not_ackd;
              // Get our space, Advertise at least a  quarter/half, otherwise 0
              tcp_tx_meta_reg.win_size   = rx_sar_rsp_reg.win_size;
              tcp_tx_meta_reg.rst        = 0;
              tcp_tx_meta_reg.syn        = 0;
              tcp_tx_meta_reg.fin        = 0;
              tcp_tx_meta_reg.length     = 0;
              tcp_tx_meta_reg.ack        = 1;  // ACK is always set when established
              tcp_tx_meta_reg.ack_number = rx_sar_rsp_reg.recvd;
            } else {
              tx_sar_rsp = tx_sar_rsp_reg;

              if (!remaining_window.bit(WINDOW_BITS)) {
                usableWindow_w = remaining_window(WINDOW_BITS - 1, 0);
              } else {
                usableWindow_w = 0;
              }
              logger.Info(TX_ENGINE, "Useable Win", usableWindow_w.to_string(16));
            }

            tcp_tx_meta_reg.seq_number = tx_sar_rsp.not_ackd;

            // Construct paylaod address by tx_sar_rsp.not_ackd
            GetSessionMemAddr(
                tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd, false, payload_mem_addr);

            // Check length, if bigger than Usable Window or MMS
            if (curr_length <= usableWindow_w) {
              if (curr_length >= TCP_MSS) {  // TODO change to >= MSS, use maxSegmentCount
                // We stay in this state and sent immediately another packet
                txSar_not_ackd_w       = tx_sar_rsp.not_ackd + TCP_MSS;
                tcp_tx_meta_reg.length = TCP_MSS;
                // Update next current length in case the module is in a loop
                next_curr_length = curr_length - TCP_MSS;
                next_used_length = used_length + TCP_MSS;
              } else {
                // If we sent all data, there might be a fin we have to sent too
                if (tx_sar_rsp.fin_is_ready && (tx_sar_rsp.ackd_eq_not_ackd || curr_length == 0)) {
                  tx_eng_event_reg.type = FIN;
                } else {
                  logger.Info(TX_ENGINE, "Change state to LOAD, curren len < win");
                  fsm_state = LOAD;  // MR TODO: this not should be conditional
                }
                // Check if small segment and if unacknowledged data in pipe (Nagle)
                if (tx_sar_rsp.ackd_eq_not_ackd) {
                  // todo maybe precompute
                  next_curr_length = tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0) - curr_length;
                  next_used_length = tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0) + curr_length;
                  txSar_not_ackd_w = tx_sar_rsp.not_ackd_short;
                } else {
                  txSar_not_ackd_w = tx_sar_rsp.not_ackd + curr_length;
                  tx_eng_to_timer_set_ptimer.write(tx_eng_event_reg.session_id);
                  logger.Info(TX_ENGINE,
                              PROBE_TMR,
                              "Set PTimer",
                              tx_eng_event_reg.session_id.to_string(16));
                }
                tcp_tx_meta_reg.length = curr_length;
                // Write back tx_sar_rsp not_ackd pointer
                to_tx_sar_req = TxEngToTxSarReq(tx_eng_event_reg.session_id, txSar_not_ackd_w, 1);
                tx_eng_to_tx_sar_req.write(to_tx_sar_req);
                logger.Info(TX_ENGINE, TX_SAR_TB, "SAR Upd", to_tx_sar_req.to_string());
              }
            } else {
              // code duplication, but better timing..
              if (usableWindow_w >= TCP_MSS) {
                txSar_not_ackd_w       = tx_sar_rsp.not_ackd + TCP_MSS;
                tcp_tx_meta_reg.length = TCP_MSS;
                next_curr_length       = curr_length - TCP_MSS;
                next_used_length       = used_length + TCP_MSS;
              } else {
                // Check if we sent >= TCP_MSS data
                // if (tx_sar_rsp.ackd_eq_not_ackd) {
                txSar_not_ackd_w = tx_sar_rsp.not_ackd + usableWindow_w;
                next_curr_length = curr_length - usableWindow_w;
                next_used_length = used_length + usableWindow_w;
                //} else
                //  txSar_not_ackd_w = tx_sar_rsp.not_ackd;

                tcp_tx_meta_reg.length = usableWindow_w;
                // Set probe Timer to try again later
                tx_eng_to_timer_set_ptimer.write(tx_eng_event_reg.session_id);
                to_tx_sar_req = TxEngToTxSarReq(tx_eng_event_reg.session_id, txSar_not_ackd_w, 1);
                tx_eng_to_tx_sar_req.write(to_tx_sar_req);
                logger.Info(
                    TX_ENGINE, PROBE_TMR, "Set PTimer", tx_eng_event_reg.session_id.to_string(16));
                logger.Info(TX_ENGINE, TX_SAR_TB, "SAR Upd", to_tx_sar_req.to_string());
                logger.Info(TX_ENGINE, "Change state to LOAD");

                fsm_state = LOAD;
              }
            }

            if (tcp_tx_meta_reg.length != 0) {
              to_mem_cmd = MemBufferRWCmd(payload_mem_addr, tcp_tx_meta_reg.length);
              tx_eng_to_mem_cmd.write(to_mem_cmd);
              tx_tcp_payload_length_out.write(tcp_tx_meta_reg.length);
              tx_eng_fsm_meta_data_out.write(tcp_tx_meta_reg);
              tx_four_tuple_source.write(true);
              tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
              // Only set RT timer if we actually send sth
              to_rtimer_req = TxEngToRetransTimerReq(tx_eng_event_reg.session_id);
              tx_eng_to_timer_set_rtimer.write(to_rtimer_req);

              logger.Info(TX_ENGINE, "ReadMem Cmd", to_mem_cmd.to_string());
              logger.Info(TX_ENGINE, TOE_TOP, "TX Meta", tcp_tx_meta_reg.to_string(), true);
              logger.Info(TX_ENGINE,
                          SLUP_CTRL,
                          "Four Tuple Lup",
                          tx_eng_event_reg.session_id.to_string(16));
              logger.Info(TX_ENGINE, RTRMT_TMR, "Set RTimer", to_rtimer_req.to_string());

            }  // TODO if probe send msg length 1

            remaining_window = tx_sar_rsp.min_window - next_used_length;

            curr_length         = next_curr_length;  // Update next iteration variables
            used_length         = next_used_length;
            tx_sar_rsp.not_ackd = txSar_not_ackd_w;
            ackd_eq_not_ackd    = (tx_sar_rsp.ackd == txSar_not_ackd_w) ? true : false;
            tx_sar_rsp_reg      = tx_sar_rsp;
            logger.Info(TX_ENGINE, "Remain Win", remaining_window.to_string(16));
            logger.Info(TX_ENGINE, "Used len", curr_length.to_string(16));
            logger.Info(TX_ENGINE, "Not Acked", used_length.to_string(16));
            logger.Info(TX_ENGINE, "Current TX SAR", tx_sar_rsp_reg.to_string());
            tx_rx_sar_loaded = true;
          }
          break;
        case RT:
          if ((!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty())) {
            rx_sar_rsp_reg = rx_sar_to_tx_eng_lup_rsp.read();
            tx_sar_rsp     = tx_sar_to_tx_eng_rsp.read();
            logger.Info(TX_ENGINE, "RT Seg");
            logger.Info(RX_SAR_TB, TX_ENGINE, "SAR Rsp", rx_sar_rsp_reg.to_string());
            logger.Info(TX_SAR_TB, TX_ENGINE, "SAR Rsp", tx_sar_rsp.to_string());

            // Compute how many bytes have to be retransmitted, If the fin was sent,
            // subtract 1 byte
            curr_length = tx_sar_rsp.used_length_rst;

            tcp_tx_meta_reg.ack_number = rx_sar_rsp_reg.recvd;
            tcp_tx_meta_reg.seq_number = tx_sar_rsp.ackd;
            // Get our space, Advertise at least a  quarter/half, otherwise 0
            tcp_tx_meta_reg.win_size = rx_sar_rsp_reg.win_size;
            tcp_tx_meta_reg.ack      = 1;  // ACK is always set when session is established
            tcp_tx_meta_reg.rst      = 0;
            tcp_tx_meta_reg.syn      = 0;
            tcp_tx_meta_reg.fin      = 0;

            // Construct address before modifying tx_sar_rsp.ackd
            // payload_mem_addr(31, 30) = (!TCP_RX_DDR_BYPASS);  // If DDR is not used in the RX
            // start
            //                                                   // from the beginning of the memory
            // payload_mem_addr(30, WINDOW_BITS) = tx_eng_event_reg.session_id(13, 0);
            // payload_mem_addr(WINDOW_BITS - 1, 0) =
            //     tx_sar_rsp.ackd(WINDOW_BITS - 1, 0);  // tx_eng_event_reg.address;
            GetSessionMemAddr(
                tx_eng_event_reg.session_id, tx_sar_rsp.ackd, false, payload_mem_addr);
            // Decrease Slow Start Threshold, only on first RT from retransmitTimer
            if (!tx_rx_sar_loaded && (tx_eng_event_reg.retrans_cnt == 1)) {
              if (curr_length > (4 * TCP_MSS)) {  // max( FlightSize/2, 2*TCP_MSS) RFC:5681
                slowstart_threshold = curr_length / 2;
              } else {
                slowstart_threshold = (2 * TCP_MSS);
              }
              to_tx_sar_req =
                  TxEngToTxSarRetransReq(tx_eng_event_reg.session_id, slowstart_threshold);
              tx_eng_to_tx_sar_req.write(to_tx_sar_req);
              logger.Info(TX_ENGINE, TX_SAR_TB, "SAR RT Upd", to_tx_sar_req.to_string());
            }

            // Since we are retransmitting from tx_sar_rsp.ackd to tx_sar_rsp.not_ackd, this
            // data is already inside the usable_window
            // => no check is required
            // Only check if length is bigger than MMS
            if (curr_length > TCP_MSS) {
              // We stay in this state and sent immediately another packet
              tcp_tx_meta_reg.length = TCP_MSS;
              tx_sar_rsp.ackd += TCP_MSS;
              tx_sar_rsp.used_length_rst -= TCP_MSS;
              // TODO replace with dynamic count, remove this
              if (retrans_seg_cnt == 3) {
                // Should set a probe or sth??
                // tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id,
                // tx_sar_rsp.not_ackd, 1));
                fsm_state = LOAD;
              }
              tx_eng_event_reg.type = RT_CONT;
              retrans_seg_cnt++;
            } else {
              tcp_tx_meta_reg.length = curr_length;
              if (tx_sar_rsp.fin_is_sent) {  // In case a FIN was sent and not ack, has to be
                                             // sent again
                tx_eng_event_reg.type = FIN;
              } else {
                // set RT here???
                fsm_state = LOAD;
              }
            }

            // Only send a packet if there is data
            if (tcp_tx_meta_reg.length != 0) {
              to_mem_cmd = MemBufferRWCmd(payload_mem_addr, tcp_tx_meta_reg.length);

              tx_eng_to_mem_cmd.write(to_mem_cmd);
              tx_tcp_payload_length_out.write(tcp_tx_meta_reg.length);
              tx_eng_fsm_meta_data_out.write(tcp_tx_meta_reg);
              tx_four_tuple_source.write(true);
              tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
              // Only set RT timer if we actually send sth
              to_rtimer_req = TxEngToRetransTimerReq(tx_eng_event_reg.session_id);
              tx_eng_to_timer_set_rtimer.write(to_rtimer_req);

              logger.Info(TX_ENGINE, "ReadMem Cmd", to_mem_cmd.to_string());
              logger.Info(TX_ENGINE, TOE_TOP, "RT Meta", tcp_tx_meta_reg.to_string(), true);
              logger.Info(TX_ENGINE,
                          SLUP_CTRL,
                          "Four Tuple Lup",
                          tx_eng_event_reg.session_id.to_string(16));
              logger.Info(TX_ENGINE, RTRMT_TMR, "Set RTimer", to_rtimer_req.to_string());
            }
            tx_rx_sar_loaded = true;
            tx_sar_rsp_reg   = tx_sar_rsp;
          }
          break;

        case RT_CONT:
          logger.Info(TX_ENGINE, "RT-CONT Seg");

          // Compute how many bytes have to be retransmitted, If the fin was sent,
          // subtract 1 byte
          curr_length = tx_sar_rsp_reg.used_length_rst;

          tcp_tx_meta_reg.ack_number = rx_sar_rsp_reg.recvd;
          tcp_tx_meta_reg.seq_number = tx_sar_rsp_reg.ackd;
          tcp_tx_meta_reg.win_size = rx_sar_rsp_reg.win_size;  // Get our space, Advertise at least
                                                               // a quarter/half, otherwise 0
          tcp_tx_meta_reg.ack = 1;  // ACK is always set when session is established
          tcp_tx_meta_reg.rst = 0;
          tcp_tx_meta_reg.syn = 0;
          tcp_tx_meta_reg.fin = 0;

          // Construct address before modifying tx_sar_rsp_reg.ackd
          // payload_mem_addr(31, 30) = (!TCP_RX_DDR_BYPASS);  // If DDR is not used in the RX start
          //                                                   // from the beginning of the memory
          // payload_mem_addr(30, WINDOW_BITS) = tx_eng_event_reg.session_id(13, 0);
          // payload_mem_addr(WINDOW_BITS - 1, 0) =
          //     tx_sar_rsp_reg.ackd(WINDOW_BITS - 1, 0);  // tx_eng_event_reg.address;
          GetSessionMemAddr(tx_eng_event_reg.session_id, tx_sar_rsp.ackd, false, payload_mem_addr);

          // Decrease Slow Start Threshold, only on first RT from retransmitTimer
          if (!tx_rx_sar_loaded && (tx_eng_event_reg.retrans_cnt == 1)) {
            if (curr_length > (4 * TCP_MSS)) {  // max( FlightSize/2, 2*TCP_MSS) RFC:5681
              slowstart_threshold = curr_length / 2;
            } else {
              slowstart_threshold = (2 * TCP_MSS);
            }
            to_tx_sar_req =
                TxEngToTxSarRetransReq(tx_eng_event_reg.session_id, slowstart_threshold);
            tx_eng_to_tx_sar_req.write(to_tx_sar_req);
            logger.Info(TX_ENGINE, TX_SAR_TB, "SAR RT Upd", to_tx_sar_req.to_string());
          }

          // Since we are retransmitting from tx_sar_rsp_reg.ackd to tx_sar_rsp_reg.not_ackd, this
          // data is already inside the usable_window
          // => no check is required
          // Only check if length is bigger than MMS
          if (curr_length > TCP_MSS) {
            // We stay in this state and sent immediately another packet
            tcp_tx_meta_reg.length = TCP_MSS;
            tx_sar_rsp_reg.ackd += TCP_MSS;
            tx_sar_rsp_reg.used_length_rst -= TCP_MSS;
            // TODO replace with dynamic count, remove this
            if (retrans_seg_cnt == 3) {
              // Should set a probe or sth??
              // tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id,
              // tx_sar_rsp_reg.not_ackd, 1));
              fsm_state = LOAD;
            }
            retrans_seg_cnt++;
          } else {
            tcp_tx_meta_reg.length = curr_length;
            if (tx_sar_rsp_reg.fin_is_sent) {  // In case a FIN was sent and not ack, has to be
                                               // sent again
              tx_eng_event_reg.type = FIN;
            } else {
              // set RT here???
              fsm_state = LOAD;
            }
          }

          // Only send a packet if there is data
          if (tcp_tx_meta_reg.length != 0) {
            to_mem_cmd = MemBufferRWCmd(payload_mem_addr, tcp_tx_meta_reg.length);
            tx_eng_to_mem_cmd.write(to_mem_cmd);
            tx_tcp_payload_length_out.write(tcp_tx_meta_reg.length);
            tx_eng_fsm_meta_data_out.write(tcp_tx_meta_reg);
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
            // Only set RT timer if we actually send sth
            to_rtimer_req = TxEngToRetransTimerReq(tx_eng_event_reg.session_id);
            tx_eng_to_timer_set_rtimer.write(to_rtimer_req);

            logger.Info(TX_ENGINE, "ReadMem Cmd", to_mem_cmd.to_string());
            logger.Info(TX_ENGINE, TOE_TOP, "RT-CONT Meta", tcp_tx_meta_reg.to_string(), true);
            logger.Info(
                TX_ENGINE, SLUP_CTRL, "Four Tuple Lup", tx_eng_event_reg.session_id.to_string(16));
            logger.Info(TX_ENGINE, RTRMT_TMR, "Set RTimer", to_rtimer_req.to_string());
          }

          break;

        case ACK:
        case ACK_NODELAY:
          if (!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) {
            rx_sar_rsp_reg = rx_sar_to_tx_eng_lup_rsp.read();
            tx_sar_rsp     = tx_sar_to_tx_eng_rsp.read();
            logger.Info(TX_ENGINE, "ACK Seg");
            logger.Info(RX_SAR_TB, TX_ENGINE, "SAR Rsp", rx_sar_rsp_reg.to_string());
            logger.Info(TX_SAR_TB, TX_ENGINE, "SAR Rsp", tx_sar_rsp.to_string());

            tcp_tx_meta_reg.ack_number = rx_sar_rsp_reg.recvd;
            tcp_tx_meta_reg.seq_number = tx_sar_rsp.not_ackd;  // Always send SEQ
            tcp_tx_meta_reg.win_size   = rx_sar_rsp_reg.win_size;
            tcp_tx_meta_reg.length     = 0;
            tcp_tx_meta_reg.ack        = 1;
            tcp_tx_meta_reg.rst        = 0;
            tcp_tx_meta_reg.syn        = 0;
            tcp_tx_meta_reg.fin        = 0;
            tx_tcp_payload_length_out.write(tcp_tx_meta_reg.length);
            tx_eng_fsm_meta_data_out.write(tcp_tx_meta_reg);
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);

            logger.Info(TX_ENGINE, TOE_TOP, "ACK Meta", tcp_tx_meta_reg.to_string(), true);
            logger.Info(
                TX_ENGINE, SLUP_CTRL, "Four Tuple Lup", tx_eng_event_reg.session_id.to_string(16));
            logger.Info(TX_ENGINE, "Change state to LOAD from ACK event");

            fsm_state = LOAD;
          }
          break;
        case SYN:
          if (((tx_eng_event_reg.retrans_cnt != 0) && !tx_sar_to_tx_eng_rsp.empty()) ||
              (tx_eng_event_reg.retrans_cnt == 0)) {
            logger.Info(TX_ENGINE, "SYN Seg");
            if (tx_eng_event_reg.retrans_cnt != 0) {
              tx_sar_rsp = tx_sar_to_tx_eng_rsp.read();
              logger.Info(TX_SAR_TB, TX_ENGINE, "SAR Rsp", tx_sar_rsp.to_string());

              tcp_tx_meta_reg.seq_number = tx_sar_rsp.ackd;
            } else {
              tx_sar_rsp.not_ackd        = RandomVal();
              tcp_tx_meta_reg.seq_number = tx_sar_rsp.not_ackd;
              to_tx_sar_req =
                  TxEngToTxSarReq(tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd + 1, 1, 1);
              tx_eng_to_tx_sar_req.write(to_tx_sar_req);
              logger.Info(TX_ENGINE, TX_SAR_TB, "SAR Upd", to_tx_sar_req.to_string());
            }
            tcp_tx_meta_reg.ack_number = 0;
            tcp_tx_meta_reg.win_size   = 0xFFFF;
            tcp_tx_meta_reg.ack        = 0;
            tcp_tx_meta_reg.rst        = 0;
            tcp_tx_meta_reg.syn        = 1;
            tcp_tx_meta_reg.fin        = 0;
            // For TCP_MSS Option, 4 bytes, WIN-Scale 4B
            tcp_tx_meta_reg.length    = 8;
            tcp_tx_meta_reg.win_scale = WINDOW_SCALE_BITS;

            tx_tcp_payload_length_out.write(tcp_tx_meta_reg.length);
            tx_eng_fsm_meta_data_out.write(tcp_tx_meta_reg);
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
            // set retransmit timer
            tx_eng_to_timer_set_rtimer.write(
                TxEngToRetransTimerReq(tx_eng_event_reg.session_id, SYN));

            logger.Info(TX_ENGINE, TOE_TOP, "SYN Meta", tcp_tx_meta_reg.to_string(), true);
            logger.Info(
                TX_ENGINE, SLUP_CTRL, "Four Tuple Lup", tx_eng_event_reg.session_id.to_string(16));
            logger.Info(TX_ENGINE, RTRMT_TMR, "Set RTimer", to_rtimer_req.to_string());
            fsm_state = LOAD;
          }
          break;
        case SYN_ACK:
          if (!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) {
            logger.Info(TX_ENGINE, "SYN_ACK Seg");

            rx_sar_rsp_reg = rx_sar_to_tx_eng_lup_rsp.read();
            tx_sar_rsp     = tx_sar_to_tx_eng_rsp.read();
            logger.Info(RX_SAR_TB, TX_ENGINE, "SAR Rsp", rx_sar_rsp_reg.to_string());
            logger.Info(TX_SAR_TB, TX_ENGINE, "SAR Rsp", tx_sar_rsp.to_string());
            // construct SYN_ACK message
            tcp_tx_meta_reg.ack_number = rx_sar_rsp_reg.recvd;
            tcp_tx_meta_reg.win_size   = 0xFFFF;
            tcp_tx_meta_reg.ack        = 1;
            tcp_tx_meta_reg.rst        = 0;
            tcp_tx_meta_reg.syn        = 1;
            tcp_tx_meta_reg.fin        = 0;
            if (tx_eng_event_reg.retrans_cnt != 0) {
              tcp_tx_meta_reg.seq_number = tx_sar_rsp.ackd;
            } else {
              tx_sar_rsp.not_ackd        = RandomVal();
              tcp_tx_meta_reg.seq_number = tx_sar_rsp.not_ackd;
              to_tx_sar_req =
                  TxEngToTxSarReq(tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd + 1, 1, 1);
              tx_eng_to_tx_sar_req.write(to_tx_sar_req);
              logger.Info(TX_ENGINE, TX_SAR_TB, "SAR Upd", to_tx_sar_req.to_string());
            }
            // TCP_MSS Option 4B
            // WScale Option 4B, if receiver support win-scale, out side use this option
            tcp_tx_meta_reg.length    = 4 + 4 * (rx_sar_rsp_reg.win_shift != 0);
            tcp_tx_meta_reg.win_scale = rx_sar_rsp_reg.win_shift;

            tx_eng_fsm_meta_data_out.write(tcp_tx_meta_reg);
            tx_tcp_payload_length_out.write(tcp_tx_meta_reg.length);
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
            // set retransmit timer
            to_rtimer_req = TxEngToRetransTimerReq(tx_eng_event_reg.session_id, SYN_ACK);
            tx_eng_to_timer_set_rtimer.write(to_rtimer_req);

            logger.Info(TX_ENGINE, TOE_TOP, "SYN-ACK Meta", tcp_tx_meta_reg.to_string(), true);
            logger.Info(
                TX_ENGINE, SLUP_CTRL, "Four Tuple Lup", tx_eng_event_reg.session_id.to_string(16));
            logger.Info(TX_ENGINE, RTRMT_TMR, "Set RTimer", to_rtimer_req.to_string());

            fsm_state = LOAD;
          }
          break;
        case FIN:
          // The term tx_rx_sar_loaded is used for retransmission proposes
          if ((!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) ||
              tx_rx_sar_loaded) {
            logger.Info(TX_ENGINE, "FIN Seg");

            if (!tx_rx_sar_loaded) {
              rx_sar_rsp_reg = rx_sar_to_tx_eng_lup_rsp.read();
              tx_sar_rsp     = tx_sar_to_tx_eng_rsp.read();
              logger.Info(RX_SAR_TB, TX_ENGINE, "SAR Rsp", rx_sar_rsp_reg.to_string());
              logger.Info(TX_SAR_TB, TX_ENGINE, "SAR Rsp", tx_sar_rsp.to_string());
            } else {
              tx_sar_rsp = tx_sar_rsp_reg;
            }
            // construct FIN message
            tcp_tx_meta_reg.ack_number = rx_sar_rsp_reg.recvd;
            tcp_tx_meta_reg.win_size   = rx_sar_rsp_reg.win_size;  // Get our space
            tcp_tx_meta_reg.length     = 0;
            tcp_tx_meta_reg.ack        = 1;  // has to be set for FIN message as well
            tcp_tx_meta_reg.rst        = 0;
            tcp_tx_meta_reg.syn        = 0;
            tcp_tx_meta_reg.fin        = 1;

            // Check if retransmission, in case of RT, we have to reuse not_ackd number
            tcp_tx_meta_reg.seq_number = tx_sar_rsp.not_ackd - (tx_eng_event_reg.retrans_cnt != 0);
            if (tx_eng_event_reg.retrans_cnt == 0) {
              // Check if all data is sent, otherwise we have to delay FIN message
              // Set fin flag, such that probeTimer is informed
              if (tx_sar_rsp.app_written == tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0)) {
                to_tx_sar_req = TxEngToTxSarReq(
                    tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd + 1, 1, 0, true, true);
              } else {
                to_tx_sar_req = TxEngToTxSarReq(
                    tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd, 1, 0, true, false);
              }
              tx_eng_to_tx_sar_req.write(to_tx_sar_req);
              logger.Info(TX_ENGINE, TX_SAR_TB, "SAR Upd", to_tx_sar_req.to_string());
            }

            // Check if there is a FIN to be sent
            if (tcp_tx_meta_reg.seq_number(WINDOW_BITS - 1, 0) == tx_sar_rsp.app_written) {
              tx_eng_fsm_meta_data_out.write(tcp_tx_meta_reg);
              tx_tcp_payload_length_out.write(tcp_tx_meta_reg.length);
              tx_four_tuple_source.write(true);
              tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
              // set retransmit timer
              to_rtimer_req = TxEngToRetransTimerReq(tx_eng_event_reg.session_id);

              tx_eng_to_timer_set_rtimer.write(to_rtimer_req);
              logger.Info(TX_ENGINE, TOE_TOP, "FIN Meta", tcp_tx_meta_reg.to_string(), true);
              logger.Info(TX_ENGINE,
                          SLUP_CTRL,
                          "Four Tuple Lup",
                          tx_eng_event_reg.session_id.to_string(16));
              logger.Info(TX_ENGINE, RTRMT_TMR, "Set RTimer", to_rtimer_req.to_string());
            }

            fsm_state = LOAD;
          }
          break;
        case RST:
          // Assumption RST length == 0
          rst_event = tx_eng_event_reg;
          logger.Info(TX_ENGINE, "Rst Seg");
          if (!rst_event.has_session_id()) {
            tx_tcp_payload_length_out.write(0);
            tcp_tx_meta = TxEngFsmMetaData(0, rst_event.get_ack_number(), 1, 1, 0, 0);
            tx_eng_fsm_meta_data_out.write(tcp_tx_meta);
            // fourtuple is from tx fsm
            tx_four_tuple_source.write(false);

            tx_eng_fsm_four_tuple_out.write(tx_eng_event_reg.four_tuple);
            logger.Info(TX_ENGINE, "Four Tuple in FSM", tx_eng_event_reg.four_tuple.to_string());
            logger.Info(TX_ENGINE, TOE_TOP, "Rst Meta", tcp_tx_meta.to_string(), true);

          } else if (!tx_sar_to_tx_eng_rsp.empty()) {
            tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);
            logger.Info(TX_SAR_TB, TX_ENGINE, "TX SAR Lup Rsp", tx_sar_rsp.to_string());
            tx_tcp_payload_length_out.write(0);
            tcp_tx_meta =
                TxEngFsmMetaData(tx_sar_rsp.not_ackd, rst_event.get_ack_number(), 1, 1, 0, 0);
            tx_eng_fsm_meta_data_out.write(tcp_tx_meta);
            // fourtuple is from slup_ctrls
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(rst_event.session_id);
            logger.Info(TX_ENGINE, SLUP_CTRL, "Four Tuple Lup", rst_event.session_id.to_string(16));
            logger.Info(TX_ENGINE, TOE_TOP, "Rst Meta", tcp_tx_meta.to_string(), true);
          }
          fsm_state = LOAD;
          break;
      }  // switch
      break;
  }  // switch
}

/** @ingroup tx_engine
 *  Forwards the incoming tuple from the reverse table in session lookup controller or Tx Engine FSM
 *  to the 2 header construction modules
 */
void TxEngFourTupleHandler(
    // fourtuple is from session lookup controller
    stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
    // fourtuple source is from tx enigne  FSM or slookup, true = slookup, false = FSM
    stream<bool> &tx_four_tuple_source,
    // fourtuple from tx enigne FSM
    stream<FourTuple> &tx_eng_fsm_four_tuple,
    // four tuple to tcp header constructor
    stream<FourTuple> &tx_four_tuple_for_tcp_header,
    // two tuple to ipv4 header constructor
    stream<IpAddrPair> &tx_ip_pair_for_ip_header) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum TxEngTupleFsm { LOAD_TUPLE_SRC, READ_TUPLE_FROM_TX_ENG, READ_TUPLE_FROM_SLUP_CTRL };
  static TxEngTupleFsm fsm_state = LOAD_TUPLE_SRC;

  static bool            tuple_is_from_slookup;
  ReverseTableToTxEngRsp slookup_rsp;
  FourTuple              four_tuple;
  IpAddrPair             ip_pair;

  switch (fsm_state) {
    case LOAD_TUPLE_SRC:
      if (!tx_four_tuple_source.empty()) {
        tuple_is_from_slookup = tx_four_tuple_source.read();
        logger.Info(TX_ENGINE,  "Tx Four Tuple from SLUP ? or FSM", tuple_is_from_slookup ? "1" : "0");
        fsm_state = tuple_is_from_slookup ? READ_TUPLE_FROM_SLUP_CTRL : READ_TUPLE_FROM_TX_ENG;
      }
      break;
    case READ_TUPLE_FROM_SLUP_CTRL:
      if (!slookup_rev_table_to_tx_eng_rsp.empty()) {
        slookup_rsp = slookup_rev_table_to_tx_eng_rsp.read();
        logger.Info(SLUP_CTRL, TX_ENGINE, "RevTable Rsp Tuple", slookup_rsp.to_string());
        ip_pair =
            IpAddrPair(slookup_rsp.four_tuple.src_ip_addr, slookup_rsp.four_tuple.dst_ip_addr);
        tx_ip_pair_for_ip_header.write(ip_pair);
        tx_four_tuple_for_tcp_header.write(slookup_rsp.four_tuple);
        fsm_state = LOAD_TUPLE_SRC;
      }
      break;
    case READ_TUPLE_FROM_TX_ENG:
      if (!tx_eng_fsm_four_tuple.empty()) {
        // should in big endian
        four_tuple = tx_eng_fsm_four_tuple.read();

        ip_pair = IpAddrPair(four_tuple.src_ip_addr, four_tuple.dst_ip_addr);
        tx_ip_pair_for_ip_header.write(ip_pair);
        tx_four_tuple_for_tcp_header.write(four_tuple);
        fsm_state = LOAD_TUPLE_SRC;
        logger.Info(TX_ENGINE, "Tx FSM Tuple", four_tuple.to_string());
        logger.Info(TX_ENGINE, "Tx FSM IpPair", ip_pair.to_string());
      }
      break;
  }
}

/** @ingroup tx_engine
 * 	Reads the TCP header metadata and the IP tuples. From this data it
 * generates the TCP pseudo header and streams it out.
 *  @param[in]		tx_eng_fsm_meta_data
 *  @param[in]		tx_four_tuple_for_tcp_header
 *  @param[out]		tx_tcp_pseudo_full_header_out
 */

void                 TxEngPseudoFullHeaderConstruct(stream<TxEngFsmMetaData> &tx_eng_fsm_meta_data,
                                                    stream<FourTuple> &       tx_four_tuple_for_tcp_header,
                                                    stream<bool> &            tx_tcp_packet_contains_payload,
                                                    stream<NetAXISWord> &     tx_tcp_pseudo_full_header_out) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  static TxEngFsmMetaData tx_fsm_meta_data_reg;
  static FourTuple        four_tuple;

  // If length is 0 the packet or it is a SYN packet the payload is not needed
  bool packet_contains_payload;

  ap_uint<16> window_size;
  NetAXISWord send_word;

  if (!tx_four_tuple_for_tcp_header.empty() && !tx_eng_fsm_meta_data.empty()) {
    tx_four_tuple_for_tcp_header.read(four_tuple);
    tx_eng_fsm_meta_data.read(tx_fsm_meta_data_reg);
    // tcp header without options length = 20B
    // tx_fsm_meta_data_reg.length = payload length + option length
    ap_uint<16> tcp_length_in_pseduo_header = tx_fsm_meta_data_reg.length + 0x14;

    if (tx_fsm_meta_data_reg.length == 0 || tx_fsm_meta_data_reg.syn) {
      packet_contains_payload = false;
    } else {
      packet_contains_payload = true;
    }
    // Generate pseudoheader
    // IP:port in four tuple is in big-endian
    send_word.data(31, 0)  = four_tuple.src_ip_addr;
    send_word.data(63, 32) = four_tuple.dst_ip_addr;

    send_word.data(79, 64) = 0x0600;  // TCP

    send_word.data(95, 80) = SwapByte<16>(tcp_length_in_pseduo_header);
    // Generate TCP header
    send_word.data(111, 96)  = four_tuple.src_tcp_port;
    send_word.data(127, 112) = four_tuple.dst_tcp_port;

    // Insert SEQ number
    send_word.data(159, 128) = SwapByte<32>(tx_fsm_meta_data_reg.seq_number);
    // Insert ACK number
    send_word.data(191, 160) = SwapByte<32>(tx_fsm_meta_data_reg.ack_number);

    send_word.data.bit(192)  = 0;  // NS bit
    send_word.data(195, 193) = 0;  // reserved
    /* Control bits:
     * [200] == FIN
     * [201] == SYN
     * [202] == RST
     * [203] == PSH
     * [204] == ACK
     * [205] == URG
     */
    send_word.data.bit(200)  = tx_fsm_meta_data_reg.fin;  // control bits
    send_word.data.bit(201)  = tx_fsm_meta_data_reg.syn;
    send_word.data.bit(202)  = tx_fsm_meta_data_reg.rst;
    send_word.data.bit(203)  = 0;
    send_word.data.bit(204)  = tx_fsm_meta_data_reg.ack;
    send_word.data(207, 205) = 0;  // some other bits
    send_word.data(223, 208) = SwapByte<16>(tx_fsm_meta_data_reg.win_size);
    send_word.data(255, 224) = 0;  // urgPointer & checksum

    if (tx_fsm_meta_data_reg.syn) {
      send_word.data(263, 256) = 0x02;                   // Option Kind
      send_word.data(271, 264) = 0x04;                   // Option length
      send_word.data(287, 272) = SwapByte<16>(TCP_MSS);  // Set Maximum MSS

      // Only send WINDOW SCALE in SYN and SYN-ACK, in the latter only send if
      // WSopt was received RFC 7323 1.3
      if (tx_fsm_meta_data_reg.win_scale != 0) {
        send_word.data(199, 196) = 0x7;   // data offset, tcp header = 28B
        send_word.data(295, 288) = 0x03;  // Option Kind
        send_word.data(303, 296) = 0x03;  // Option length
        send_word.data(311, 304) = tx_fsm_meta_data_reg.win_scale;
        send_word.data(319, 312) = 0X0;  // End of Option List
        // if SYN/SYN-ACK packet, tcp pseudo header + tcp header = 40B
        send_word.keep = 0xFFFFFFFFFF;
      } else {
        send_word.data(199, 196) = 0x6;  // data offset
        send_word.keep           = 0xFFFFFFFFF;
      }
    } else {
      send_word.data(199, 196) = 0x5;  // data offset
      // normal packet, tcp pseudo header + tcp header = 32B
      send_word.keep = 0xFFFFFFFF;
    }

    send_word.last = 1;
    tx_tcp_pseudo_full_header_out.write(send_word);
    tx_tcp_packet_contains_payload.write(packet_contains_payload);
    logger.Info(TX_ENGINE, "TCP Pseudo header", send_word.to_string());
    logger.Info(TX_ENGINE, "TCP Seg contains payload?", packet_contains_payload ? "1" : "0");
  }
}

/** @ingroup tx_engine
 *  It appends the pseudo TCP header with the corresponding payload stream.
 * @param[in] tx_tcp_pseudo_full_header = tcp pseudo header + tcp header
 */
void                 TxEngConstructPseudoPacket(stream<NetAXISWord> &tx_tcp_pseudo_full_header,
                                                stream<bool> &       tx_tcp_packet_contains_payload,
                                                stream<NetAXISWord> &tx_tcp_packet_payload,
                                                stream<NetAXISWord> &tx_tcp_pseudo_packet_for_tx_eng,
                                                stream<NetAXISWord> &tx_tcp_pseudo_packet_for_checksum) {
#pragma HLS INLINE   off
#pragma HLS LATENCY  max = 1
#pragma HLS pipeline II  = 1

  static NetAXISWord prev_word;
  NetAXISWord        cur_word;
  NetAXISWord        send_word;
  // Payload is not needed because length==0 or is a SYN packet
  bool packet_contains_payload;

  enum TxPseudoPktFsmState { READ_PSEUDO, READ_PAYLOAD, EXTRA_WORD };
  static TxPseudoPktFsmState fsm_state = READ_PSEUDO;

  switch (fsm_state) {
    case READ_PSEUDO:
      if (!tx_tcp_pseudo_full_header.empty() && !tx_tcp_packet_contains_payload.empty()) {
        tx_tcp_pseudo_full_header.read(cur_word);
        tx_tcp_packet_contains_payload.read(packet_contains_payload);
        // no payload in this segment, send it immediately
        if (!packet_contains_payload) {
          logger.Info(TX_ENGINE, "PseudoPacket Single", cur_word.to_string());

          tx_tcp_pseudo_packet_for_tx_eng.write(cur_word);
          tx_tcp_pseudo_packet_for_checksum.write(cur_word);
        } else {
          fsm_state = READ_PAYLOAD;
          prev_word = cur_word;
        }
      }
      break;
    case READ_PAYLOAD:
      if (!tx_tcp_packet_payload.empty()) {
        tx_tcp_packet_payload.read(cur_word);
        // TODO this is only valid with no TCP options
        MergeTwoWordsHead(cur_word, prev_word, 32, send_word);
        send_word.last = cur_word.last;

        if (cur_word.last) {
          if (cur_word.keep.bit(32)) {
            send_word.last = 0;
            fsm_state      = EXTRA_WORD;
          } else {
            fsm_state = READ_PSEUDO;
          }
        }
        logger.Info(TX_ENGINE, "PseudoPacket", send_word.to_string());
        tx_tcp_pseudo_packet_for_tx_eng.write(send_word);
        tx_tcp_pseudo_packet_for_checksum.write(send_word);
        prev_word = cur_word;
        // ConcatTwoWords(NetAXISWord(), cur_word, 32, prev_word);
        // prev_word.last = cur_word.last;
      }
      break;
    case EXTRA_WORD:
      // the last word in payload has more than 32B and less than 64B
      ConcatTwoWords(NetAXISWord(), prev_word, 32, send_word);
      send_word.last = 1;
      logger.Info(TX_ENGINE, "PseudoPacket EXTRA", send_word.to_string());
      tx_tcp_pseudo_packet_for_tx_eng.write(send_word);
      tx_tcp_pseudo_packet_for_checksum.write(send_word);
      fsm_state = READ_PSEUDO;
      break;
  }
}

/** @ingroup tx_engine
 *  It remove the tcp pseudo header in tcp pseduo packet, the output stream is TCP header + TCP
 * payload
 */
void                 TxEngRemovePseudoHeader(stream<NetAXISWord> &tx_tcp_pseudo_packet_for_tx_eng,
                                             stream<NetAXISWord> &tx_tcp_packet) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off
  const ap_uint<6>   pseudo_header_byte_offset = 12;
  static NetAXISWord prev_word;
  static bool        first_word = true;
  NetAXISWord        cur_word;
  NetAXISWord        send_word = NetAXISWord();
  enum TxEngPseduoHdrRemoverFsmState { READ, READ_MORE_PAYLOAD, EXTRA_WORD };
  static TxEngPseduoHdrRemoverFsmState fsm_state = READ;

  switch (fsm_state) {
    case READ:
      if (!tx_tcp_pseudo_packet_for_tx_eng.empty()) {
        cur_word = tx_tcp_pseudo_packet_for_tx_eng.read();
        if (cur_word.last) {
          ConcatTwoWords(NetAXISWord(), cur_word, pseudo_header_byte_offset, send_word);
          send_word.last = 1;
          logger.Info(TX_ENGINE, "TcpPacket for TxEng Single", send_word.to_string());

          tx_tcp_packet.write(send_word);
        } else {
          prev_word = cur_word;
          fsm_state = READ_MORE_PAYLOAD;
        }
      }
      break;
    case READ_MORE_PAYLOAD:
      if (!tx_tcp_pseudo_packet_for_tx_eng.empty()) {
        cur_word = tx_tcp_pseudo_packet_for_tx_eng.read();
        ConcatTwoWords(cur_word, prev_word, pseudo_header_byte_offset, send_word);
        send_word.last = 0;

        if (cur_word.last) {
          if (cur_word.keep.bit(12)) {
            fsm_state = EXTRA_WORD;
          } else {
            send_word.last = 1;
            fsm_state      = READ;
          }
        }
        tx_tcp_packet.write(send_word);
        logger.Info(TX_ENGINE, "TcpPacket for TxEng", send_word.to_string());
        prev_word = cur_word;
      }
      break;
    case EXTRA_WORD:
      ConcatTwoWords(NetAXISWord(), prev_word, pseudo_header_byte_offset, send_word);
      send_word.last = 1;
      tx_tcp_packet.write(send_word);
      logger.Info(TX_ENGINE, "TcpPacket for TxEng EXTRA", send_word.to_string());
      fsm_state = READ;
      break;
  }
}

/** @ingroup tx_engine
 * 	Reads the IP header metadata and the IP addresses. From this data it
 * generates the IP header and streams it out.
 *  NOTE: NO checksum calculation after the SYNTHESIS, checksum only for testbench
 *  @param[in]		tx_tcp_payload_length
 *  @param[in]		tx_tcp_ip_pair
 *  @param[out]		tx_ipv4_header
 */
void                 TxEngConstructIpv4Header(stream<ap_uint<16> > &tx_tcp_payload_length,
                                              stream<IpAddrPair> &  tx_tcp_ip_pair,
                                              stream<NetAXISWord> & tx_ipv4_header) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  IpAddrPair  ip_addr_pair;
  NetAXISWord send_word = NetAXISWord(0, 0, 0, 1);
  // total length in ipv4 header
  ap_uint<16> ip_length = 0;
  // tcp header options length + tcp payload length
  ap_uint<16> tcp_payload_length = 0;

  if (!tx_tcp_payload_length.empty() && !tx_tcp_ip_pair.empty()) {
    tx_tcp_payload_length.read(tcp_payload_length);
    tx_tcp_ip_pair.read(ip_addr_pair);
    ip_length = tcp_payload_length + 40;

    // Compose the IP header
    send_word.data(7, 0)     = 0x45;
    send_word.data(15, 8)    = 0;
    send_word.data(31, 16)   = SwapByte<16>(ip_length);
    send_word.data(47, 32)   = 0;
    send_word.data(50, 48)   = 0;    // Flags
    send_word.data(63, 51)   = 0x0;  // Fragment Offset
    send_word.data(71, 64)   = 0x40;
    send_word.data(79, 72)   = 0x06;                      // TCP
    send_word.data(95, 80)   = 0;                         // IP header checksum
    send_word.data(127, 96)  = ip_addr_pair.src_ip_addr;  // bigendian
    send_word.data(159, 128) = ip_addr_pair.dst_ip_addr;

    send_word.last = 1;
    send_word.keep = 0xFFFFF;

#ifndef __SYNTHESIS__
    ap_uint<16> checksum;
    ap_uint<21> first_sum = 0;

    ap_uint<16> tmp;

    for (int i = 0; i < 160; i += 16) {
      tmp(7, 0)  = send_word.data(i + 15, i + 8);
      tmp(15, 8) = send_word.data(i + 7, i);
      first_sum += tmp;
    }
    first_sum = first_sum(15, 0) + first_sum(20, 16);
    first_sum = first_sum(15, 0) + first_sum(16, 16);
    checksum  = ~(first_sum(15, 0));

    send_word.data(95, 80) = (checksum(7, 0), checksum(15, 8));

#endif
    tx_ipv4_header.write(send_word);
    logger.Info(TX_ENGINE, "Tcp TX Ipv4 header", send_word.to_string());
  }
}

/** @ingroup tx_engine
 *  Reads the IP header stream and the payload stream. It also inserts TCP
 * checksum The complete packet is then streamed out of the TCP engine. The IP
 * checksum must be computed and inserted after
 *
 * insert TCP checksum, no insert ip checksum!
 */
void                 TxEngConstructIpv4Packet(stream<NetAXISWord> & tx_ipv4_header,
                                              stream<ap_uint<16> > &tx_tcp_checksum,
                                              stream<NetAXISWord> & tx_tcp_packet,
                                              stream<NetAXIS> &     tx_ip_pkt_out) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  const ap_uint<6> tcp_seg_byte_offset = 44;  // 64B - 20B ip_header length

  NetAXISWord        ipv4_header;
  NetAXISWord        cur_word;
  NetAXISWord        send_word;
  static NetAXISWord prev_word;

  ap_uint<16> tcp_checksum;
  enum teips_states { READ_FIRST, READ_PAYLOAD, EXTRA_WORD };
  static teips_states fsm_state = READ_FIRST;

  switch (fsm_state) {
    case READ_FIRST:
      if (!tx_ipv4_header.empty() && !tx_tcp_packet.empty() && !tx_tcp_checksum.empty()) {
        tx_ipv4_header.read(ipv4_header);
        tx_tcp_packet.read(cur_word);
        tx_tcp_checksum.read(tcp_checksum);
        // no IP options supported
        // ip header is always 20B
        send_word.data(159, 0)   = ipv4_header.data(159, 0);
        send_word.keep(19, 0)    = 0xFFFFF;
        send_word.data(511, 160) = cur_word.data(351, 0);
        send_word.data(304, 288) = SwapByte<16>(tcp_checksum);
        send_word.keep(63, 20)   = cur_word.keep(43, 0);
        send_word.last           = 0;

        if (cur_word.last) {
          if (cur_word.keep.bit(tcp_seg_byte_offset)) {
            fsm_state = EXTRA_WORD;
          } else {
            send_word.last = 1;
          }
        } else {
          fsm_state = READ_PAYLOAD;
        }
        prev_word = cur_word;
        logger.Info(TX_ENGINE, TOE_TOP, "IpSeg Out First", send_word.to_string());
        tx_ip_pkt_out.write(send_word.to_net_axis());
      }
      break;
    case READ_PAYLOAD:
      if (!tx_tcp_packet.empty()) {
        tx_tcp_packet.read(cur_word);

        // send_word.data(159, 0)   = prev_word.data(511, 352);
        // send_word.keep(19, 0)    = prev_word.keep(63, 44);
        // send_word.data(511, 160) = cur_word.data(351, 0);
        // send_word.keep(63, 20)   = cur_word.keep(43, 0);
        ConcatTwoWords(cur_word, prev_word, tcp_seg_byte_offset, send_word);
        send_word.last = 0;

        if (cur_word.last) {
          if (cur_word.keep.bit(tcp_seg_byte_offset)) {
            fsm_state = EXTRA_WORD;
          } else
            send_word.last = 1;
          fsm_state = READ_FIRST;
        }

        prev_word = cur_word;
        tx_ip_pkt_out.write(send_word.to_net_axis());
        logger.Info(TX_ENGINE, TOE_TOP, "Tx Eng IpSeg Out", send_word.to_string());
      }
      break;
    case EXTRA_WORD:
      // send_word.data(159, 0) = prev_word.data(511, 352);
      // send_word.keep(19, 0)  = prev_word.keep(63, 44);
      ConcatTwoWords(NetAXISWord(), prev_word, tcp_seg_byte_offset, send_word);
      send_word.last = 1;
      tx_ip_pkt_out.write(send_word.to_net_axis());
      logger.Info(TX_ENGINE, TOE_TOP, "Tx Eng IpSeg Out Extra", send_word.to_string());
      fsm_state = READ_FIRST;
      break;
  }
}

void tx_engine(
    // from ack delay to tx engine req
    stream<EventWithTuple> &ack_delay_to_tx_eng_event,
    stream<ap_uint<1> > &   tx_eng_read_count_fifo,
    // rx sar
    stream<TcpSessionID> &  tx_eng_to_rx_sar_lup_req,
    stream<RxSarLookupRsp> &rx_sar_to_tx_eng_lup_rsp,
    // tx sar
    stream<TxEngToTxSarReq> &tx_eng_to_tx_sar_req,
    stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
    // timer
    stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
    stream<TcpSessionID> &          tx_eng_to_timer_set_ptimer,
    // to session lookup req/rsp
    stream<ap_uint<16> > &          tx_eng_to_slookup_rev_table_req,
    stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
    // to datamover cmd
    stream<DataMoverCmd> &mover_read_mem_cmd_out,
    // read data from data mem
    stream<NetAXIS> &mover_read_mem_data_in,
#if (TCP_NODELAY)
    stream<NetAXIS> &tx_app_to_tx_eng_data,
#endif
    // to outer
    stream<NetAXIS> &tx_ip_pkt_out

) {
#pragma HLS DATAFLOW
#pragma HLS INLINE

#pragma HLS INTERFACE axis port = mover_read_mem_cmd_out
#pragma HLS aggregate variable = mover_read_mem_cmd_out compact = bit

#pragma HLS INTERFACE axis port = mover_read_mem_data_in
#if (TCP_NODELAY)
#pragma HLS INTERFACE axis port = tx_app_to_tx_eng_data
#endif
#pragma HLS INTERFACE axis port = tx_ip_pkt_out

  // Memory Read delay around 76 cycles, 10 cycles/packet, so keep meta of at
  // least 8 packets
  static stream<TxEngFsmMetaData> tx_eng_fsm_meta_data_fifo("tx_eng_fsm_meta_data_fifo");
#pragma HLS stream variable = tx_eng_fsm_meta_data_fifo depth = 16
#pragma HLS aggregate variable = tx_eng_fsm_meta_data_fifo compact = bit

  static stream<ap_uint<16> > tx_tcp_payload_length_fifo("tx_tcp_payload_length_fifo");
#pragma HLS stream variable = tx_tcp_payload_length_fifo depth = 16
#pragma HLS aggregate variable = tx_tcp_payload_length_fifo compact = bit

  static stream<MemBufferRWCmd> tx_eng_to_mem_cmd_fifo("tx_eng_to_mem_cmd_fifo");
#pragma HLS stream variable = tx_eng_to_mem_cmd_fifo depth = 16
#pragma HLS aggregate variable = tx_eng_to_mem_cmd_fifo compact = bit

  static stream<bool> tx_eng_is_ddr_bypass_fifo("tx_eng_is_ddr_bypass_fifo");
#pragma HLS stream variable = tx_eng_is_ddr_bypass_fifo depth = 16

  static stream<bool> tx_four_tuple_source_fifo("tx_four_tuple_source_fifo");
#pragma HLS stream variable = tx_four_tuple_source_fifo depth = 32

  static stream<FourTuple> tx_eng_fsm_four_tuple_fifo("tx_eng_fsm_four_tuple_fifo");
#pragma HLS stream variable = tx_eng_fsm_four_tuple_fifo depth = 16
#pragma HLS aggregate variable = tx_eng_fsm_four_tuple_fifo compact = bit

  static stream<FourTuple> tx_four_tuple_for_tcp_header_fifo("tx_four_tuple_for_tcp_header_fifo");
#pragma HLS stream variable = tx_four_tuple_for_tcp_header_fifo depth = 16
#pragma HLS aggregate variable = tx_four_tuple_for_tcp_header_fifo

  static stream<IpAddrPair> tx_ip_pair_for_ip_header_fifo("tx_ip_pair_for_ip_header_fifo");
#pragma HLS stream variable = tx_ip_pair_for_ip_header_fifo depth = 16
#pragma HLS aggregate variable = tx_ip_pair_for_ip_header_fifo compact = bit

  static stream<MemBufferRWCmdDoubleAccess> mem_buffer_double_access_fifo(
      "mem_buffer_double_access_fifo");
#pragma HLS stream variable = mem_buffer_double_access_fifo depth = 16
#pragma HLS aggregate variable = mem_buffer_double_access_fifo compact = bit

  // read from memory
  static stream<NetAXISWord> tx_eng_read_tcp_payload_fifo("tx_eng_read_tcp_payload_fifo");
#pragma HLS stream variable = tx_eng_read_tcp_payload_fifo depth = 512
#pragma HLS aggregate variable = tx_eng_read_tcp_payload_fifo compact = bit

  static stream<bool> tx_tcp_packet_contains_payload_fifo("tx_tcp_packet_contains_payload_fifo");
#pragma HLS stream variable = tx_tcp_packet_contains_payload_fifo depth = 32

  static stream<NetAXISWord> tx_tcp_pseudo_full_header_fifo("tx_tcp_pseudo_full_header_fifo");
#pragma HLS stream variable = tx_tcp_pseudo_full_header_fifo depth = 512
#pragma HLS aggregate variable = tx_tcp_pseudo_full_header_fifo compact = bit

  static stream<NetAXISWord> tx_tcp_pseudo_packet_for_tx_eng_fifo(
      "tx_tcp_pseudo_packet_for_tx_eng_fifo");
#pragma HLS stream variable = tx_tcp_pseudo_packet_for_tx_eng_fifo depth = 512
#pragma HLS aggregate variable = tx_tcp_pseudo_packet_for_tx_eng_fifo compact = bit

  static stream<NetAXISWord> tx_tcp_pseudo_packet_for_checksum_fifo(
      "tx_tcp_pseudo_packet_for_checksum_fifo");
#pragma HLS stream variable = tx_tcp_pseudo_packet_for_checksum_fifo depth = 512
#pragma HLS aggregate variable = tx_tcp_pseudo_packet_for_checksum_fifo compact = bit

  static stream<NetAXISWord> tx_tcp_packet_fifo("tx_tcp_packet_fifo");
#pragma HLS stream variable = tx_tcp_packet_fifo depth = 512
#pragma HLS aggregate variable = tx_tcp_packet_fifo compact = bit
  static stream<NetAXISWord> tx_ipv4_header_fifo("tx_ipv4_header_fifo");
#pragma HLS stream variable = tx_ipv4_header_fifo depth = 512
#pragma HLS aggregate variable = tx_ipv4_header_fifo compact = bit

  static stream<SubChecksum> tcp_pseudo_packet_subchecksum_fifo(
      "tcp_pseudo_packet_subchecksum_fifo");
#pragma HLS STREAM variable = tcp_pseudo_packet_subchecksum_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_packet_subchecksum_fifo compact = bit

  static stream<ap_uint<16> > tcp_pseudo_packet_checksum_fifo("tcp_pseudo_packet_checksum_fifo");
#pragma HLS STREAM variable = tcp_pseudo_packet_checksum_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_packet_checksum_fifo compact = bit

  TxEngTcpFsm(ack_delay_to_tx_eng_event,
              tx_eng_read_count_fifo,
              tx_eng_to_rx_sar_lup_req,
              rx_sar_to_tx_eng_lup_rsp,
              tx_eng_to_tx_sar_req,
              tx_sar_to_tx_eng_rsp,
              tx_eng_to_timer_set_rtimer,
              tx_eng_to_timer_set_ptimer,
              tx_eng_to_slookup_rev_table_req,
              tx_tcp_payload_length_fifo,
              tx_eng_fsm_meta_data_fifo,
              tx_eng_to_mem_cmd_fifo,
#if (TCP_NODELAY)
              tx_eng_is_ddr_bypass_fifo,
#endif
              tx_four_tuple_source_fifo,
              tx_eng_fsm_four_tuple_fifo);

  TxEngReadDataSendCmd(
      tx_eng_to_mem_cmd_fifo, mover_read_mem_cmd_out, mem_buffer_double_access_fifo);

  TxEngReadDataFromMem(mover_read_mem_data_in,
                       mem_buffer_double_access_fifo,
#if (TCP_NODELAY)
                       tx_eng_is_ddr_bypass_fifo,
                       tx_app_to_tx_eng_data,
#endif
                       tx_eng_read_tcp_payload_fifo);

  TxEngFourTupleHandler(slookup_rev_table_to_tx_eng_rsp,
                        tx_four_tuple_source_fifo,
                        tx_eng_fsm_four_tuple_fifo,
                        tx_four_tuple_for_tcp_header_fifo,
                        tx_ip_pair_for_ip_header_fifo);

  TxEngPseudoFullHeaderConstruct(tx_eng_fsm_meta_data_fifo,
                                 tx_four_tuple_for_tcp_header_fifo,
                                 tx_tcp_packet_contains_payload_fifo,
                                 tx_tcp_pseudo_full_header_fifo);

  TxEngConstructPseudoPacket(tx_tcp_pseudo_full_header_fifo,
                             tx_tcp_packet_contains_payload_fifo,
                             tx_eng_read_tcp_payload_fifo,
                             tx_tcp_pseudo_packet_for_tx_eng_fifo,
                             tx_tcp_pseudo_packet_for_checksum_fifo);

  ComputeTxSubChecksum(tx_tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_subchecksum_fifo);

  CheckChecksum(tcp_pseudo_packet_subchecksum_fifo, tcp_pseudo_packet_checksum_fifo);

  TxEngRemovePseudoHeader(tx_tcp_pseudo_packet_for_tx_eng_fifo, tx_tcp_packet_fifo);

  TxEngConstructIpv4Header(
      tx_tcp_payload_length_fifo, tx_ip_pair_for_ip_header_fifo, tx_ipv4_header_fifo);

  TxEngConstructIpv4Packet(
      tx_ipv4_header_fifo, tcp_pseudo_packet_checksum_fifo, tx_tcp_packet_fifo, tx_ip_pkt_out);
}