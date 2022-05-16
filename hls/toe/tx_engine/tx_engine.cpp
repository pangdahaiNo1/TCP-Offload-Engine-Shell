#include "tx_engine.hpp"

/** @ingroup tx_engine
 *  @name txEng_metaLoader
 *  The txEng_metaLoader reads the Events from the EventEngine then it loads all
 * the necessary MetaData from the data structures (RX & TX Sar Table).
 * Depending on the Event type it generates the necessary MetaData for the
 *  txEng_ipHeader_Const and the txEng_pseudoHeader_Const.
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
    stream<ap_uint<16> > &tx_tcp_payload_length,
    // to construct the tcp pseudo full header
    stream<TxEngFsmMetaData> &tx_eng_fsm_meta_data,
    // to read mem
    stream<MemBufferRWCmd> &tx_eng_to_mem_cmd,
#if (TCP_NODELAY)
    stream<bool> &tx_eng_is_ddr_bypass,
#endif
    // fourtuple source is from tx enigne  FSM or slookup, true = slookup, false = FSM
    stream<bool> &tx_four_tuple_source,
    // four tuple from tx enigne FSM
    stream<FourTuple> &tx_eng_fsm_four_tuple) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum RxEngTcpFsmState { LOAD, TRANSITION };
  static RxEngTcpFsmState fsm_state = LOAD;

  static bool           tx_eng_fsm_sar_is_loaded = false;
  static EventWithTuple tx_eng_event_reg;
  static ap_uint<32>    ml_randomValue = 0x562301af;  // Random seed initialization

  static ap_uint<2>       ml_segmentCount = 0;
  static RxSarLookupRsp   rx_sar_rsp_reg;
  TxSarToTxEngRsp         tx_sar_rsp;
  static TxSarToTxEngRsp  tx_sar_rsp_reg;
  static TxEngFsmMetaData tx_eng_meta_data_reg;

  ap_uint<16> slowstart_threshold;
  ap_uint<32> payload_mem_addr;
  RstEvent    rst_event;

  static TcpSessionBuffer curr_length;
  static TcpSessionBuffer used_length;
  static TcpSessionBuffer usable_window;
  static bool             ackd_eq_not_ackd;
  TcpSessionBuffer        usableWindow_w;

  ap_uint<16>        next_curr_length;
  ap_uint<16>        next_used_length;
  ap_uint<16>        next_usable_window;
  ap_uint<32>        txSar_not_ackd_w;
  ap_uint<32>        txSar_ackd;
  static ap_uint<17> remaining_window;

  switch (fsm_state) {
    case LOAD:
      if (!ack_delay_to_tx_eng_event.empty()) {
        ack_delay_to_tx_eng_event.read(tx_eng_event_reg);
        tx_eng_read_count_fifo.write(1);
        tx_eng_fsm_sar_is_loaded = false;
        // NOT necessary for SYN/SYN_ACK only needs one
        switch (tx_eng_event_reg.type) {
          case RT:
            tx_eng_to_rx_sar_lup_req.write(tx_eng_event_reg.session_id);
            tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id));
            break;
          case TX:
            tx_eng_to_rx_sar_lup_req.write(tx_eng_event_reg.session_id);
            tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id));
            break;
          case SYN_ACK:
            tx_eng_to_rx_sar_lup_req.write(tx_eng_event_reg.session_id);
            tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id));
            break;
          case FIN:
            tx_eng_to_rx_sar_lup_req.write(tx_eng_event_reg.session_id);
            tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id));
            break;
          case RST:
            // Get tx_sar_rsp for SEQ numb
            rst_event = tx_eng_event_reg;
            if (rst_event.has_session_id()) {
              tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id));
            }
            break;
          case ACK_NODELAY:
          case ACK:
            tx_eng_to_rx_sar_lup_req.write(tx_eng_event_reg.session_id);
            tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id));
            break;
          case SYN:
            if (tx_eng_event_reg.retrans_cnt != 0) {
              tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id));
            }
            break;
          default:
            break;
        }
        fsm_state = TRANSITION;
        ml_randomValue++;  // make sure it doesn't become zero TODO move this out
                           // of if, but breaks my testsuite
      }                    // if not empty
      ml_segmentCount = 0;
      break;
    case TRANSITION:
      switch (tx_eng_event_reg.type) {
        // When Nagle's algorithm disabled
        // Can bypass DDR
#if (TCP_NODELAY)
        case TX:
          if ((!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) ||
              tx_eng_fsm_sar_is_loaded) {
            if (!tx_eng_fsm_sar_is_loaded) {
              rx_sar_to_tx_eng_lup_rsp.read(rx_sar_rsp_reg);
              tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);
            }

            tx_eng_meta_data_reg.win_size =
                rx_sar_rsp_reg.win_size;  // Get our space, Advertise at least a
                                          // quarter/half, otherwise 0
            tx_eng_meta_data_reg.ack_number = rx_sar_rsp_reg.recvd;
            tx_eng_meta_data_reg.seq_number = tx_sar_rsp.not_ackd;
            tx_eng_meta_data_reg.ack        = 1;  // ACK is always set when established
            tx_eng_meta_data_reg.rst        = 0;
            tx_eng_meta_data_reg.syn        = 0;
            tx_eng_meta_data_reg.fin        = 0;
            // tx_eng_meta_data_reg.length = 0;

            curr_length = tx_eng_event_reg.length;
            used_length = tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0) - tx_sar_rsp.ackd;
            // min_window, is the min(tx_sar_rsp.recv_window, tx_sar_rsp.cong_window)
            /*if (tx_sar_rsp.min_window > used_length) {
                usable_window = tx_sar_rsp.min_window - used_length;
            }
            else {
                usable_window = 0;
            }*/
            if (tx_sar_rsp.usable_window < tx_eng_event_reg.length) {
              tx_eng_to_timer_set_ptimer.write(tx_eng_event_reg.session_id);
            }

            tx_eng_meta_data_reg.length = tx_eng_event_reg.length;

            // TODO some checking
            tx_sar_rsp.not_ackd += tx_eng_event_reg.length;

            tx_eng_to_tx_sar_req.write(
                TxEngToTxSarReq(tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd, 1));
            fsm_state = LOAD;

            // Send a packet only if there is data or we want to send an empty
            // probing message
            if (tx_eng_meta_data_reg.length !=
                0) {  // || tx_eng_event_reg.retransmit) //TODO retransmit
                      // boolean currently not set, should be removed

              tx_tcp_payload_length.write(tx_eng_meta_data_reg.length);
              tx_eng_fsm_meta_data.write(tx_eng_meta_data_reg);
              tx_four_tuple_source.write(true);
              tx_eng_is_ddr_bypass.write(true);
              tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);

              // Only set RT timer if we actually send sth, TODO only set if we
              // change state and sent sth
              tx_eng_to_timer_set_rtimer.write(TxEngToRetransTimerReq(tx_eng_event_reg.session_id));
            }  // TODO if probe send msg length 1
            tx_eng_fsm_sar_is_loaded = true;
          }

          break;
#else
        case TX:
          // Sends everything between tx_sar_rsp.not_ackd and tx_sar_rsp.app_written
          if ((!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) ||
              tx_eng_fsm_sar_is_loaded) {
            if (!tx_eng_fsm_sar_is_loaded) {
              rx_sar_to_tx_eng_lup_rsp.read(rx_sar_rsp_reg);
              tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);

              curr_length      = tx_sar_rsp.curr_length;
              used_length      = tx_sar_rsp.used_length;
              usableWindow_w   = tx_sar_rsp.usable_window;
              ackd_eq_not_ackd = tx_sar_rsp.ackd_eq_not_ackd;
              // Get our space, Advertise at least a  quarter/half, otherwise 0
              tx_eng_meta_data_reg.win_size   = rx_sar_rsp_reg.win_size;
              tx_eng_meta_data_reg.rst        = 0;
              tx_eng_meta_data_reg.syn        = 0;
              tx_eng_meta_data_reg.fin        = 0;
              tx_eng_meta_data_reg.length     = 0;
              tx_eng_meta_data_reg.ack        = 1;  // ACK is always set when established
              tx_eng_meta_data_reg.ack_number = rx_sar_rsp_reg.recvd;
              // If DDR is not used in the RX start from the beginning of the memory
              payload_mem_addr(31, 30) = (!TCP_RX_DDR_BYPASS);
              payload_mem_addr(29, 16) = tx_eng_event_reg.session_id(13, 0);
            } else {
              tx_sar_rsp = tx_sar_rsp_reg;

              if (!remaining_window.bit(16)) {
                usableWindow_w = remaining_window(15, 0);
              } else {
                usableWindow_w = 0;
              }
            }

            tx_eng_meta_data_reg.seq_number = tx_sar_rsp.not_ackd;

            // Construct address before modifying tx_sar_rsp.not_ackd
            payload_mem_addr(15, 0) = tx_sar_rsp.not_ackd(15, 0);  // tx_eng_event_reg.address;

            // Check length, if bigger than Usable Window or MMS
            if (curr_length <= usableWindow_w) {
              if (curr_length >= TCP_MSS) {  // TODO change to >= MSS, use maxSegmentCount
                // We stay in this state and sent immediately another packet
                txSar_not_ackd_w            = tx_sar_rsp.not_ackd + TCP_MSS;
                tx_eng_meta_data_reg.length = TCP_MSS;

                next_curr_length = curr_length - TCP_MSS;  // Update next current length in
                                                           // case the module is in a loop
                next_used_length = used_length + TCP_MSS;
              } else {
                // If we sent all data, there might be a fin we have to sent too
                if (tx_sar_rsp.fin_is_ready && (tx_sar_rsp.ackd_eq_not_ackd || curr_length == 0)) {
                  tx_eng_event_reg.type = FIN;
                } else {
                  fsm_state = LOAD;  // MR TODO: this not should be conditional
                }
                // Check if small segment and if unacknowledged data in pipe (Nagle)
                if (tx_sar_rsp.ackd_eq_not_ackd) {
                  next_curr_length = tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0) -
                                     curr_length;  // todo maybe precompute
                  next_used_length = tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0) + curr_length;
                  txSar_not_ackd_w = tx_sar_rsp.not_ackd_short;
                  tx_eng_meta_data_reg.length = curr_length;
                } else {
                  txSar_not_ackd_w = tx_sar_rsp.not_ackd;
                  tx_eng_to_timer_set_ptimer.write(
                      tx_eng_event_reg.session_id);  // 	TODO: if the app tries to write
                                                     // data which is not multiple of MSS
                                                     // this lead to retransmission
                }
                // Write back tx_sar_rsp not_ackd pointer
                tx_eng_to_tx_sar_req.write(
                    TxEngToTxSarReq(tx_eng_event_reg.session_id, txSar_not_ackd_w, 1));
              }
            } else {
              // code duplication, but better timing..
              if (usableWindow_w >= TCP_MSS) {
                txSar_not_ackd_w            = tx_sar_rsp.not_ackd + TCP_MSS;
                tx_eng_meta_data_reg.length = TCP_MSS;
                next_curr_length            = curr_length - TCP_MSS;
                next_used_length            = used_length + TCP_MSS;
              } else {
                // Check if we sent >= TCP_MSS data
                if (tx_sar_rsp.ackd_eq_not_ackd) {
                  txSar_not_ackd_w            = tx_sar_rsp.not_ackd + usableWindow_w;
                  tx_eng_meta_data_reg.length = usableWindow_w;
                  next_curr_length            = curr_length - usableWindow_w;
                  next_used_length            = used_length + usableWindow_w;
                } else
                  txSar_not_ackd_w = tx_sar_rsp.not_ackd;
                // Set probe Timer to try again later
                tx_eng_to_timer_set_ptimer.write(tx_eng_event_reg.session_id);
                tx_eng_to_tx_sar_req.write(
                    TxEngToTxSarReq(tx_eng_event_reg.session_id, txSar_not_ackd_w, 1));
                fsm_state = LOAD;
              }
            }

            if (tx_eng_meta_data_reg.length != 0) {
              tx_eng_to_mem_cmd.write(
                  MemBufferRWCmd(payload_mem_addr, tx_eng_meta_data_reg.length));
              // Send a packet only if there is data or we want to send an empty
              // probing message
              tx_tcp_payload_length.write(tx_eng_meta_data_reg.length);
              tx_eng_fsm_meta_data.write(tx_eng_meta_data_reg);
              tx_four_tuple_source.write(true);
              tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
              // Only set RT timer if we actually send sth, TODO only set if we
              // change state and sent sth
              tx_eng_to_timer_set_rtimer.write(TxEngToRetransTimerReq(tx_eng_event_reg.session_id));
            }  // TODO if probe send msg length 1

            remaining_window = tx_sar_rsp.min_window - next_used_length;

            curr_length              = next_curr_length;  // Update next iteration variables
            used_length              = next_used_length;
            tx_sar_rsp.not_ackd      = txSar_not_ackd_w;
            ackd_eq_not_ackd         = (tx_sar_rsp.ackd == txSar_not_ackd_w) ? true : false;
            tx_sar_rsp_reg           = tx_sar_rsp;
            tx_eng_fsm_sar_is_loaded = true;
          }
          break;
#endif
        case RT:
          if ((!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty())) {
            rx_sar_to_tx_eng_lup_rsp.read(rx_sar_rsp_reg);
            tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);

            // Compute how many bytes have to be retransmitted, If the fin was sent,
            // subtract 1 byte
            curr_length = tx_sar_rsp.used_length_rst;

            tx_eng_meta_data_reg.ack_number = rx_sar_rsp_reg.recvd;
            tx_eng_meta_data_reg.seq_number = tx_sar_rsp.ackd;
            // Get our space, Advertise at least a  quarter/half, otherwise 0
            tx_eng_meta_data_reg.win_size = rx_sar_rsp_reg.win_size;
            tx_eng_meta_data_reg.ack      = 1;  // ACK is always set when session is established
            tx_eng_meta_data_reg.rst      = 0;
            tx_eng_meta_data_reg.syn      = 0;
            tx_eng_meta_data_reg.fin      = 0;

            // Construct address before modifying tx_sar_rsp.ackd
            payload_mem_addr(31, 30) = (!TCP_RX_DDR_BYPASS);  // If DDR is not used in the RX start
                                                              // from the beginning of the memory
            payload_mem_addr(30, WINDOW_BITS) = tx_eng_event_reg.session_id(13, 0);
            payload_mem_addr(WINDOW_BITS - 1, 0) =
                tx_sar_rsp.ackd(WINDOW_BITS - 1, 0);  // tx_eng_event_reg.address;

            // Decrease Slow Start Threshold, only on first RT from retransmitTimer
            if (!tx_eng_fsm_sar_is_loaded && (tx_eng_event_reg.retrans_cnt == 1)) {
              if (curr_length > (4 * TCP_MSS)) {  // max( FlightSize/2, 2*TCP_MSS) RFC:5681
                slowstart_threshold = curr_length / 2;
              } else {
                slowstart_threshold = (2 * TCP_MSS);
              }
              tx_eng_to_tx_sar_req.write(
                  TxEngToTxSarRetransReq(tx_eng_event_reg.session_id, slowstart_threshold));
            }

            // Since we are retransmitting from tx_sar_rsp.ackd to tx_sar_rsp.not_ackd, this
            // data is already inside the usable_window
            // => no check is required
            // Only check if length is bigger than MMS
            if (curr_length > TCP_MSS) {
              // We stay in this state and sent immediately another packet
              tx_eng_meta_data_reg.length = TCP_MSS;
              tx_sar_rsp.ackd += TCP_MSS;
              tx_sar_rsp.used_length_rst -= TCP_MSS;
              // TODO replace with dynamic count, remove this
              if (ml_segmentCount == 3) {
                // Should set a probe or sth??
                // tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id,
                // tx_sar_rsp.not_ackd, 1));
                fsm_state = LOAD;
              }
              tx_eng_event_reg.type = RT_CONT;
              ml_segmentCount++;
            } else {
              tx_eng_meta_data_reg.length = curr_length;
              if (tx_sar_rsp.fin_is_sent) {  // In case a FIN was sent and not ack, has to be
                                             // sent again
                tx_eng_event_reg.type = FIN;
              } else {
                // set RT here???
                fsm_state = LOAD;
              }
            }

            // Only send a packet if there is data
            if (tx_eng_meta_data_reg.length != 0) {
              tx_eng_to_mem_cmd.write(
                  MemBufferRWCmd(payload_mem_addr, tx_eng_meta_data_reg.length));
              tx_tcp_payload_length.write(tx_eng_meta_data_reg.length);
              tx_eng_fsm_meta_data.write(tx_eng_meta_data_reg);
              tx_four_tuple_source.write(true);
#if (TCP_NODELAY)
              tx_eng_is_ddr_bypass.write(false);
#endif
              tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
              // Only set RT timer if we actually send sth
              tx_eng_to_timer_set_rtimer.write(TxEngToRetransTimerReq(tx_eng_event_reg.session_id));
            }
            tx_eng_fsm_sar_is_loaded = true;
            tx_sar_rsp_reg           = tx_sar_rsp;
          }
          break;

        case RT_CONT:

          // Compute how many bytes have to be retransmitted, If the fin was sent,
          // subtract 1 byte
          curr_length = tx_sar_rsp_reg.used_length_rst;

          tx_eng_meta_data_reg.ack_number = rx_sar_rsp_reg.recvd;
          tx_eng_meta_data_reg.seq_number = tx_sar_rsp_reg.ackd;
          tx_eng_meta_data_reg.win_size =
              rx_sar_rsp_reg.win_size;   // Get our space, Advertise at least
                                         // a quarter/half, otherwise 0
          tx_eng_meta_data_reg.ack = 1;  // ACK is always set when session is established
          tx_eng_meta_data_reg.rst = 0;
          tx_eng_meta_data_reg.syn = 0;
          tx_eng_meta_data_reg.fin = 0;

          // Construct address before modifying tx_sar_rsp_reg.ackd
          payload_mem_addr(31, 30) = (!TCP_RX_DDR_BYPASS);  // If DDR is not used in the RX start
                                                            // from the beginning of the memory
          payload_mem_addr(30, WINDOW_BITS) = tx_eng_event_reg.session_id(13, 0);
          payload_mem_addr(WINDOW_BITS - 1, 0) =
              tx_sar_rsp_reg.ackd(WINDOW_BITS - 1, 0);  // tx_eng_event_reg.address;

          // Decrease Slow Start Threshold, only on first RT from retransmitTimer
          if (!tx_eng_fsm_sar_is_loaded && (tx_eng_event_reg.retrans_cnt == 1)) {
            if (curr_length > (4 * TCP_MSS)) {  // max( FlightSize/2, 2*TCP_MSS) RFC:5681
              slowstart_threshold = curr_length / 2;
            } else {
              slowstart_threshold = (2 * TCP_MSS);
            }
            tx_eng_to_tx_sar_req.write(
                TxEngToTxSarRetransReq(tx_eng_event_reg.session_id, slowstart_threshold));
          }

          // Since we are retransmitting from tx_sar_rsp_reg.ackd to tx_sar_rsp_reg.not_ackd, this
          // data is already inside the usable_window
          // => no check is required
          // Only check if length is bigger than MMS
          if (curr_length > TCP_MSS) {
            // We stay in this state and sent immediately another packet
            tx_eng_meta_data_reg.length = TCP_MSS;
            tx_sar_rsp_reg.ackd += TCP_MSS;
            tx_sar_rsp_reg.used_length_rst -= TCP_MSS;
            // TODO replace with dynamic count, remove this
            if (ml_segmentCount == 3) {
              // Should set a probe or sth??
              // tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(tx_eng_event_reg.session_id,
              // tx_sar_rsp_reg.not_ackd, 1));
              fsm_state = LOAD;
            }
            ml_segmentCount++;
          } else {
            tx_eng_meta_data_reg.length = curr_length;
            if (tx_sar_rsp_reg.fin_is_sent) {  // In case a FIN was sent and not ack, has to be
                                               // sent again
              tx_eng_event_reg.type = FIN;
            } else {
              // set RT here???
              fsm_state = LOAD;
            }
          }

          // Only send a packet if there is data
          if (tx_eng_meta_data_reg.length != 0) {
            tx_eng_to_mem_cmd.write(MemBufferRWCmd(payload_mem_addr, tx_eng_meta_data_reg.length));
            tx_tcp_payload_length.write(tx_eng_meta_data_reg.length);
            tx_eng_fsm_meta_data.write(tx_eng_meta_data_reg);
            tx_four_tuple_source.write(true);
#if (TCP_NODELAY)
            tx_eng_is_ddr_bypass.write(false);
#endif
            tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
            // Only set RT timer if we actually send sth
            tx_eng_to_timer_set_rtimer.write(TxEngToRetransTimerReq(tx_eng_event_reg.session_id));
          }

          break;

        case ACK:
        case ACK_NODELAY:
          if (!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) {
            rx_sar_to_tx_eng_lup_rsp.read(rx_sar_rsp_reg);
            tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);

            tx_eng_meta_data_reg.ack_number = rx_sar_rsp_reg.recvd;
            tx_eng_meta_data_reg.seq_number = tx_sar_rsp.not_ackd;  // Always send SEQ
            tx_eng_meta_data_reg.win_size =
                rx_sar_rsp_reg.win_size;  // Get our space, Advertise at least a
                                          // quarter/half, otherwise 0
            tx_eng_meta_data_reg.length = 0;
            tx_eng_meta_data_reg.ack    = 1;
            tx_eng_meta_data_reg.rst    = 0;
            tx_eng_meta_data_reg.syn    = 0;
            tx_eng_meta_data_reg.fin    = 0;
            tx_tcp_payload_length.write(tx_eng_meta_data_reg.length);
            tx_eng_fsm_meta_data.write(tx_eng_meta_data_reg);
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
            fsm_state = LOAD;
          }
          break;
        case SYN:
          if (((tx_eng_event_reg.retrans_cnt != 0) && !tx_sar_to_tx_eng_rsp.empty()) ||
              (tx_eng_event_reg.retrans_cnt == 0)) {
            if (tx_eng_event_reg.retrans_cnt != 0) {
              tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);
              tx_eng_meta_data_reg.seq_number = tx_sar_rsp.ackd;
            } else {
              // tx_sar_rsp = tx_sar_rsp_reg;
              tx_sar_rsp.not_ackd             = ml_randomValue;  // FIXME better rand()
              ml_randomValue                  = (ml_randomValue * 8) xor ml_randomValue;
              tx_eng_meta_data_reg.seq_number = tx_sar_rsp.not_ackd;
              tx_eng_to_tx_sar_req.write(
                  TxEngToTxSarReq(tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd + 1, 1, 1));
            }
            tx_eng_meta_data_reg.ack_number = 0;
            // tx_eng_meta_data_reg.seq_number = tx_sar_rsp.not_ackd;
            tx_eng_meta_data_reg.win_size = 0xFFFF;
            tx_eng_meta_data_reg.ack      = 0;
            tx_eng_meta_data_reg.rst      = 0;
            tx_eng_meta_data_reg.syn      = 1;
            tx_eng_meta_data_reg.fin      = 0;
            tx_eng_meta_data_reg.length   = 4;  // For TCP_MSS Option, 4 bytes
#if (TCP_WINDOW_SCALE)
            tx_eng_meta_data_reg.length    = 8;  // Anounce our window scale
            tx_eng_meta_data_reg.win_scale = WINDOW_SCALE_BITS;
#else
            tx_eng_meta_data_reg.length = 4;
#endif
            tx_tcp_payload_length.write(tx_eng_meta_data_reg.length);  // length
            tx_eng_fsm_meta_data.write(tx_eng_meta_data_reg);
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
            // set retransmit timer
            tx_eng_to_timer_set_rtimer.write(
                TxEngToRetransTimerReq(tx_eng_event_reg.session_id, SYN));
            // tx_sar_rsp_reg = tx_sar_rsp ;
            fsm_state = LOAD;
#if (STATISTICS_MODULE)
            txEngStatsUpdate.write(txStatsUpdate(tx_eng_event_reg.session_id,
                                                 0,
                                                 true,
                                                 false,
                                                 false,
                                                 false));  // Initialize Statistics SYN
#endif
          }
          break;
        case SYN_ACK:
          if (!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) {
            rx_sar_to_tx_eng_lup_rsp.read(rx_sar_rsp_reg);
            tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);

            // construct SYN_ACK message
            tx_eng_meta_data_reg.ack_number = rx_sar_rsp_reg.recvd;
            tx_eng_meta_data_reg.win_size   = 0xFFFF;
            tx_eng_meta_data_reg.ack        = 1;
            tx_eng_meta_data_reg.rst        = 0;
            tx_eng_meta_data_reg.syn        = 1;
            tx_eng_meta_data_reg.fin        = 0;
            if (tx_eng_event_reg.retrans_cnt != 0) {
              tx_eng_meta_data_reg.seq_number = tx_sar_rsp.ackd;
            } else {
              tx_sar_rsp.not_ackd             = ml_randomValue;  // FIXME better rand();
              ml_randomValue                  = (ml_randomValue * 8) xor ml_randomValue;
              tx_eng_meta_data_reg.seq_number = tx_sar_rsp.not_ackd;
              tx_eng_to_tx_sar_req.write(
                  TxEngToTxSarReq(tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd + 1, 1, 1));
            }

#if (TCP_WINDOW_SCALE)
            tx_eng_meta_data_reg.length =
                4 + 4 * (rx_sar_rsp_reg.win_shift != 0);  // For TCP_MSS Option and WScale, 8 bytes
            tx_eng_meta_data_reg.win_scale = rx_sar_rsp_reg.win_shift;
#else
            tx_eng_meta_data_reg.length = 4;  // For TCP_MSS Option, 4 bytes
#endif
            tx_tcp_payload_length.write(tx_eng_meta_data_reg.length);  // length
            tx_eng_fsm_meta_data.write(tx_eng_meta_data_reg);
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);

            // set retransmit timer
            tx_eng_to_timer_set_rtimer.write(
                TxEngToRetransTimerReq(tx_eng_event_reg.session_id, SYN_ACK));
            fsm_state = LOAD;
          }
          break;
        case FIN:
          // The term tx_eng_fsm_sar_is_loaded is used for retransmission proposes
          if ((!rx_sar_to_tx_eng_lup_rsp.empty() && !tx_sar_to_tx_eng_rsp.empty()) ||
              tx_eng_fsm_sar_is_loaded) {
            if (!tx_eng_fsm_sar_is_loaded) {
              rx_sar_to_tx_eng_lup_rsp.read(rx_sar_rsp_reg);
              tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);
            } else {
              tx_sar_rsp = tx_sar_rsp_reg;
            }
            // construct FIN message
            tx_eng_meta_data_reg.ack_number = rx_sar_rsp_reg.recvd;
            tx_eng_meta_data_reg.win_size   = rx_sar_rsp_reg.win_size;  // Get our space
            tx_eng_meta_data_reg.length     = 0;
            tx_eng_meta_data_reg.ack        = 1;  // has to be set for FIN message as well
            tx_eng_meta_data_reg.rst        = 0;
            tx_eng_meta_data_reg.syn        = 0;
            tx_eng_meta_data_reg.fin        = 1;

            // Check if retransmission, in case of RT, we have to reuse not_ackd
            // number
            tx_eng_meta_data_reg.seq_number =
                tx_sar_rsp.not_ackd - (tx_eng_event_reg.retrans_cnt != 0);
            if (tx_eng_event_reg.retrans_cnt == 0) {
#if (!TCP_NODELAY)
              // Check if all data is sent, otherwise we have to delay FIN message
              // Set fin flag, such that probeTimer is informed
              if (tx_sar_rsp.app_written == tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0)) {
                tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(
                    tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd + 1, 1, 0, true, true));
              } else {
                tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(
                    tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd, 1, 0, true, false));
              }
#else
              tx_eng_to_tx_sar_req.write(TxEngToTxSarReq(
                  tx_eng_event_reg.session_id, tx_sar_rsp.not_ackd + 1, 1, 0, true, true));
#endif
            }

#if (!TCP_NODELAY)
            // Check if there is a FIN to be sent
            if (tx_eng_meta_data_reg.seq_number(WINDOW_BITS - 1, 0) == tx_sar_rsp.app_written)
#endif
            {
              tx_tcp_payload_length.write(tx_eng_meta_data_reg.length);
              tx_eng_fsm_meta_data.write(tx_eng_meta_data_reg);
              tx_four_tuple_source.write(true);
              tx_eng_to_slookup_rev_table_req.write(tx_eng_event_reg.session_id);
              // set retransmit timer
              // tx_eng_to_timer_set_rtimer.write(TxEngToRetransTimerReq(tx_eng_event_reg.session_id,
              // FIN));
              tx_eng_to_timer_set_rtimer.write(TxEngToRetransTimerReq(tx_eng_event_reg.session_id));
            }

            //					tx_sar_rsp_reg = tx_sar_rsp ;
            fsm_state = LOAD;
          }
          break;
        case RST:
          // Assumption RST length == 0
          rst_event = tx_eng_event_reg;
          if (!rst_event.has_session_id()) {
            tx_tcp_payload_length.write(0);
            tx_eng_fsm_meta_data.write(TxEngFsmMetaData(0, rst_event.getAckNumb(), 1, 1, 0, 0));
            tx_four_tuple_source.write(false);
            tx_eng_fsm_four_tuple.write(tx_eng_event_reg.four_tuple);
            // fsm_state = 0;
          } else if (!tx_sar_to_tx_eng_rsp.empty()) {
            tx_sar_to_tx_eng_rsp.read(tx_sar_rsp);
            tx_tcp_payload_length.write(0);
            tx_four_tuple_source.write(true);
            tx_eng_to_slookup_rev_table_req.write(
                rst_event.session_id);  // there is no session_id??
            // if (rst_event.getAckNumb() != 0)
            //{
            tx_eng_fsm_meta_data.write(
                TxEngFsmMetaData(tx_sar_rsp.not_ackd, rst_event.getAckNumb(), 1, 1, 0, 0));
            /*}
            /else
            {
                metaDataFifoOut.write(TxEngFsmMetaData(tx_sar_rsp.not_ackd, rx_sar_rsp_reg.recvd, 1,
            1, 0, 0));
            }*/
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
  static bool            source_is_read = false;
  static bool            tuple_is_from_slookup;
  ReverseTableToTxEngRsp slookup_rsp;
  FourTuple              tuple;

  // TODO: maybe use switch case is better
  if (!source_is_read) {
    if (!tx_four_tuple_source.empty()) {
      tx_four_tuple_source.read(tuple_is_from_slookup);
      source_is_read = true;
    }
  } else {
    if (!slookup_rev_table_to_tx_eng_rsp.empty() && tuple_is_from_slookup) {
      slookup_rev_table_to_tx_eng_rsp.read(slookup_rsp);
      tx_ip_pair_for_ip_header.write(
          IpAddrPair(slookup_rsp.four_tuple.src_ip_addr, slookup_rsp.four_tuple.dst_ip_addr));
      tx_four_tuple_for_tcp_header.write(tuple);
      source_is_read = false;
    } else if (!tx_eng_fsm_four_tuple.empty() && !tuple_is_from_slookup) {
      tx_eng_fsm_four_tuple.read(tuple);
      tx_ip_pair_for_ip_header.write(IpAddrPair(tuple.src_ip_addr, tuple.dst_ip_addr));
      tx_four_tuple_for_tcp_header.write(tuple);
      source_is_read = false;
    }
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
                                                    stream<NetAXIS> &         tx_tcp_pseudo_full_header_out) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  NetAXIS                 send_word = NetAXIS();
  static TxEngFsmMetaData tx_fsm_meta_data_reg;
  static FourTuple        four_tuple;
  bool                    packet_contains_payload;
  // used for `tcp length` in pseudo header
  // tcp payload length + 20B(TCP header length)
  ap_uint<16> tcp_header_payload_len = 0;
  ap_uint<16> window_size;

  if (!tx_four_tuple_for_tcp_header.empty() && !tx_eng_fsm_meta_data.empty()) {
    tx_four_tuple_for_tcp_header.read(four_tuple);
    tx_eng_fsm_meta_data.read(tx_fsm_meta_data_reg);
    tcp_header_payload_len = tx_fsm_meta_data_reg.length + 0x14;  // 20 bytes for the header

    if (tx_fsm_meta_data_reg.length == 0 ||
        tx_fsm_meta_data_reg.syn) {  // If length is 0 the packet or it is a SYN packet
                                     // the payload is not needed
      packet_contains_payload = false;
    } else {
      packet_contains_payload = true;
    }
    // Generate pseudoheader
    send_word.data(31, 0)  = four_tuple.src_ip_addr;
    send_word.data(63, 32) = four_tuple.dst_ip_addr;

    send_word.data(79, 64) = 0x0600;  // TCP

    send_word.data(95, 80)   = SwapByte<16>(tcp_header_payload_len);
    send_word.data(111, 96)  = four_tuple.src_tcp_port;  // srcPort
    send_word.data(127, 112) = four_tuple.dst_tcp_port;  // dstPort

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

#if TCP_WINDOW_SCALE
      // Only send WINDOW SCALE in SYN and SYN-ACK, in the latter only send if
      // WSopt was received RFC 7323 1.3
      if (tx_fsm_meta_data_reg.win_scale != 0) {
        send_word.data(199, 196) = 0x7;   // data offset
        send_word.data(295, 288) = 0x03;  // Option Kind
        send_word.data(303, 296) = 0x03;  // Option length
        send_word.data(311, 304) = tx_fsm_meta_data_reg.win_scale;
        send_word.data(319, 312) = 0X0;  // End of Option List
        send_word.keep           = 0xFFFFFFFFFF;
      } else {
        send_word.data(199, 196) = 0x6;  // data offset
        send_word.keep           = 0xFFFFFFFFF;
      }
#else
      send_word.data(199, 196) = 0x6;  // data offset
      send_word.keep = 0xFFFFFFFFF;
#endif
    } else {
      send_word.data(199, 196) = 0x5;  // data offset
      send_word.keep           = 0xFFFFFFFF;
    }

    send_word.last = 1;
    tx_tcp_pseudo_full_header_out.write(send_word);
    tx_tcp_packet_contains_payload.write(packet_contains_payload);
  }
}

/** @ingroup tx_engine
 *  It appends the pseudo TCP header with the corresponding payload stream.
 * @param[in] tx_tcp_pseudo_full_header = tcp pseudo header + tcp header
 */
void                 TxEngConstructPseudoPacket(stream<NetAXIS> &tx_tcp_pseudo_full_header,
                                                stream<bool> &   tx_tcp_packet_contains_payload,
                                                stream<NetAXIS> &tx_tcp_packet_payload,
                                                stream<NetAXIS> &tx_tcp_pseudo_packet_for_tx_eng,
                                                stream<NetAXIS> &tx_tcp_pseudo_packet_for_checksum) {
#pragma HLS INLINE   off
#pragma HLS LATENCY  max = 1
#pragma HLS pipeline II  = 1

  static NetAXIS prev_word;
  NetAXIS        payload_word;
  NetAXIS        send_word = NetAXIS();
  bool           packet_contains_payload;

  enum TxPseudoPktFsmState { READ_PSEUDO, READ_PAYLOAD, EXTRA_WORD };
  static TxPseudoPktFsmState fsm_state = READ_PSEUDO;

  switch (fsm_state) {
    case READ_PSEUDO:
      if (!tx_tcp_pseudo_full_header.empty() && !tx_tcp_packet_contains_payload.empty()) {
        tx_tcp_pseudo_full_header.read(prev_word);
        tx_tcp_packet_contains_payload.read(packet_contains_payload);

        if (!packet_contains_payload) {  // Payload is not needed because length==0 or
                                         // is a SYN packet, send it immediately
          tx_tcp_pseudo_packet_for_tx_eng.write(prev_word);
          tx_tcp_pseudo_packet_for_checksum.write(prev_word);
        } else {
          fsm_state = READ_PAYLOAD;
        }
      }
      break;
    case READ_PAYLOAD:
      if (!tx_tcp_packet_payload.empty()) {
        tx_tcp_packet_payload.read(payload_word);
        // TODO this is only valid with no TCP options
        send_word.data(255, 0)   = prev_word.data(255, 0);
        send_word.data(511, 256) = payload_word.data(255, 0);
        send_word.keep(31, 0)    = prev_word.keep(31, 0);
        send_word.keep(63, 32)   = payload_word.keep(31, 0);
        send_word.last           = payload_word.last;

        if (payload_word.last) {
          if (payload_word.keep.bit(32)) {
            send_word.last = 0;
            fsm_state      = EXTRA_WORD;
          } else {
            fsm_state = READ_PSEUDO;
          }
        }

        tx_tcp_pseudo_packet_for_tx_eng.write(send_word);
        tx_tcp_pseudo_packet_for_checksum.write(send_word);

        prev_word.data(255, 0) = payload_word.data(511, 256);
        prev_word.keep(31, 0)  = payload_word.keep(63, 32);
        prev_word.last         = payload_word.last;
      }
      break;
    case EXTRA_WORD:
      // the payload transaction has more than 32-bye
      send_word.data(255, 0) =
          prev_word.data(255, 0);  // TODO this is only valid with no TCP options
      send_word.keep(31, 0) = prev_word.keep(31, 0);
      send_word.last        = 1;
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
void                 TxEngRemovePseudoHeader(stream<NetAXIS> &tx_tcp_pseudo_packet_for_tx_eng,
                                             stream<NetAXIS> &tx_tcp_packet) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static NetAXIS prev_word;
  static bool    first_word = true;
  NetAXIS        cur_word;
  NetAXIS        send_word = NetAXIS();
  enum TxEngPseduoHdrRemoverFsmState { READ, EXTRA_WORD };
  static TxEngPseduoHdrRemoverFsmState fsm_state = READ;

  switch (fsm_state) {
    case READ:
      if (!tx_tcp_pseudo_packet_for_tx_eng.empty()) {
        tx_tcp_pseudo_packet_for_tx_eng.read(cur_word);
        if (first_word) {
          first_word = false;
          if (cur_word.last) {  // Short packets such as ACK or SYN-ACK
                                // TODO: use ConcatTwoWords instead
            send_word.data(415, 0) = cur_word.data(511, 96);
            send_word.keep(51, 0)  = cur_word.keep(63, 12);
            send_word.last         = 1;
            tx_tcp_packet.write(send_word);
          }
        } else {
          send_word.data(415, 0)   = prev_word.data(511, 96);
          send_word.data(511, 416) = cur_word.data(95, 0);
          send_word.keep(51, 0)    = prev_word.keep(63, 12);
          send_word.keep(63, 52)   = cur_word.keep(11, 0);

          send_word.last = cur_word.last;

          if (cur_word.last) {
            if (cur_word.keep.bit(12)) {
              fsm_state      = EXTRA_WORD;
              send_word.last = 0;
            }
          }
          tx_tcp_packet.write(send_word);
        }
        if (cur_word.last)
          first_word = true;

        prev_word = cur_word;
      }
      break;
    case EXTRA_WORD:
      send_word.data(415, 0) = prev_word.data(511, 96);
      send_word.keep(51, 0)  = prev_word.keep(63, 12);
      send_word.last         = 1;
      tx_tcp_packet.write(send_word);
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
                                              stream<NetAXIS> &     tx_ipv4_header) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  static IpAddrPair ip_addr_pair;

  NetAXIS send_word = NetAXIS(0, 0, 0, 1);
  // total length in ipv4 header
  ap_uint<16> ip_length          = 0;
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
  }
}

/** @ingroup tx_engine
 *  Reads the IP header stream and the payload stream. It also inserts TCP
 * checksum The complete packet is then streamed out of the TCP engine. The IP
 * checksum must be computed and inserted after
 *
 * insert TCP checksum, no insert ip checksum!
 */
void                 TxEngConstructIpv4Packet(stream<NetAXIS> &     tx_ipv4_header,
                                              stream<ap_uint<16> > &tx_tcp_checksum,
                                              stream<NetAXIS> &     tx_tcp_packet,
                                              stream<NetAXIS> &     tx_ip_pkt_out) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  NetAXIS        ipv4_header;
  NetAXIS        tcp_packet;
  NetAXIS        send_word = NetAXIS();
  static NetAXIS prevWord;

  ap_uint<16> tcp_checksum;
  enum teips_states { READ_FIRST, READ_PAYLOAD, EXTRA_WORD };
  static teips_states teips_fsm_state = READ_FIRST;

  switch (teips_fsm_state) {
    case READ_FIRST:
      if (!tx_ipv4_header.empty() && !tx_tcp_packet.empty() && !tx_tcp_checksum.empty()) {
        tx_ipv4_header.read(ipv4_header);
        tx_tcp_packet.read(tcp_packet);
        tx_tcp_checksum.read(tcp_checksum);
        // TODO: no IP options supported
        send_word.data(159, 0)   = ipv4_header.data(159, 0);
        send_word.keep(19, 0)    = 0xFFFFF;
        send_word.data(511, 160) = tcp_packet.data(351, 0);
        send_word.data(304, 288) = SwapByte<16>(tcp_checksum);
        send_word.keep(63, 20)   = tcp_packet.keep(43, 0);

        send_word.last = 0;

        if (tcp_packet.last) {
          if (tcp_packet.keep.bit(44))
            teips_fsm_state = EXTRA_WORD;
          else
            send_word.last = 1;
        } else
          teips_fsm_state = READ_PAYLOAD;

        prevWord = tcp_packet;
        tx_ip_pkt_out.write(send_word);
      }
      break;
    case READ_PAYLOAD:
      if (!tx_tcp_packet.empty()) {
        tx_tcp_packet.read(tcp_packet);

        send_word.data(159, 0)   = prevWord.data(511, 352);
        send_word.keep(19, 0)    = prevWord.keep(63, 44);
        send_word.data(511, 160) = tcp_packet.data(351, 0);
        send_word.keep(63, 20)   = tcp_packet.keep(43, 0);
        send_word.last           = tcp_packet.last;

        if (tcp_packet.last) {
          if (tcp_packet.keep.bit(44)) {
            send_word.last  = 0;
            teips_fsm_state = EXTRA_WORD;
          } else
            teips_fsm_state = READ_FIRST;
        }

        prevWord = tcp_packet;
        tx_ip_pkt_out.write(send_word);
      }
      break;
    case EXTRA_WORD:
      send_word.data(159, 0) = prevWord.data(511, 352);
      send_word.keep(19, 0)  = prevWord.keep(63, 44);
      send_word.last         = 1;
      tx_ip_pkt_out.write(send_word);
      teips_fsm_state = READ_FIRST;
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
#pragma HLS                        DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS                        INLINE

  // Memory Read delay around 76 cycles, 10 cycles/packet, so keep meta of at
  // least 8 packets
  static stream<TxEngFsmMetaData> tx_eng_fsm_meta_data_fifo("tx_eng_fsm_meta_data_fifo");
#pragma HLS stream variable = tx_eng_fsm_meta_data_fifo depth    = 16
#pragma HLS DATA_PACK                                   variable = tx_eng_fsm_meta_data_fifo

  static stream<ap_uint<16> > tx_tcp_payload_length_fifo("tx_tcp_payload_length_fifo");
#pragma HLS stream variable = tx_tcp_payload_length_fifo depth    = 16
#pragma HLS DATA_PACK                                    variable = tx_tcp_payload_length_fifo

  static stream<MemBufferRWCmd> tx_eng_to_mem_cmd_fifo("tx_eng_to_mem_cmd_fifo");
#pragma HLS stream variable = tx_eng_to_mem_cmd_fifo depth    = 16
#pragma HLS DATA_PACK                                variable = tx_eng_to_mem_cmd_fifo

  static stream<bool> tx_eng_is_ddr_bypass_fifo("tx_eng_is_ddr_bypass_fifo");
#pragma HLS stream variable = tx_eng_is_ddr_bypass_fifo depth = 16

  static stream<bool> tx_four_tuple_source_fifo("tx_four_tuple_source_fifo");
#pragma HLS stream variable = tx_four_tuple_source_fifo depth = 32

  static stream<FourTuple> tx_eng_fsm_four_tuple_fifo("tx_eng_fsm_four_tuple_fifo");
#pragma HLS stream variable = tx_eng_fsm_four_tuple_fifo depth    = 16
#pragma HLS DATA_PACK                                    variable = tx_eng_fsm_four_tuple_fifo

  static stream<FourTuple> tx_four_tuple_for_tcp_header_fifo("tx_four_tuple_for_tcp_header_fifo");
#pragma HLS stream variable = tx_four_tuple_for_tcp_header_fifo depth = 16
#pragma HLS DATA_PACK variable = tx_four_tuple_for_tcp_header_fifo

  static stream<IpAddrPair> tx_ip_pair_for_ip_header_fifo("tx_ip_pair_for_ip_header_fifo");
#pragma HLS stream variable = tx_ip_pair_for_ip_header_fifo depth    = 16
#pragma HLS DATA_PACK                                       variable = tx_ip_pair_for_ip_header_fifo

  static stream<MemBufferRWCmdDoubleAccess> mem_buffer_double_access_fifo(
      "mem_buffer_double_access_fifo");
#pragma HLS stream variable = mem_buffer_double_access_fifo depth    = 16
#pragma HLS DATA_PACK                                       variable = mem_buffer_double_access_fifo

  // read from memory
  static stream<NetAXIS> tx_eng_read_tcp_payload_fifo("tx_eng_read_tcp_payload_fifo");
#pragma HLS stream variable = tx_eng_read_tcp_payload_fifo depth    = 512
#pragma HLS DATA_PACK                                      variable = tx_eng_read_tcp_payload_fifo

  static stream<bool> tx_tcp_packet_contains_payload_fifo("tx_tcp_packet_contains_payload_fifo");
#pragma HLS stream variable = tx_tcp_packet_contains_payload_fifo depth = 32

  static stream<NetAXIS> tx_tcp_pseudo_full_header_fifo("tx_tcp_pseudo_full_header_fifo");
#pragma HLS stream variable = tx_tcp_pseudo_full_header_fifo depth = 512
#pragma HLS DATA_PACK variable                                     = tx_tcp_pseudo_full_header_fifo

  static stream<NetAXIS> tx_tcp_pseudo_packet_for_tx_eng_fifo(
      "tx_tcp_pseudo_packet_for_tx_eng_fifo");
#pragma HLS stream variable = tx_tcp_pseudo_packet_for_tx_eng_fifo depth = 512
#pragma HLS DATA_PACK variable = tx_tcp_pseudo_packet_for_tx_eng_fifo

  static stream<NetAXIS> tx_tcp_pseudo_packet_for_checksum_fifo(
      "tx_tcp_pseudo_packet_for_checksum_fifo");
#pragma HLS stream variable = tx_tcp_pseudo_packet_for_checksum_fifo depth = 512
#pragma HLS DATA_PACK variable = tx_tcp_pseudo_packet_for_checksum_fifo

  static stream<NetAXIS> tx_tcp_packet_fifo("tx_tcp_packet_fifo");
#pragma HLS stream variable = tx_tcp_packet_fifo depth    = 512
#pragma HLS DATA_PACK                            variable = tx_tcp_packet_fifo
  static stream<NetAXIS> tx_ipv4_header_fifo("tx_ipv4_header_fifo");
#pragma HLS stream variable = tx_ipv4_header_fifo depth    = 512
#pragma HLS DATA_PACK                             variable = tx_ipv4_header_fifo

  static stream<SubChecksum> tcp_pseudo_packet_subchecksum_fifo(
      "tcp_pseudo_packet_subchecksum_fifo");
#pragma HLS STREAM variable = tcp_pseudo_packet_subchecksum_fifo depth = 16
#pragma HLS DATA_PACK variable = tcp_pseudo_packet_subchecksum_fifo

  static stream<ap_uint<16> > tcp_pseudo_packet_checksum_fifo("tcp_pseudo_packet_checksum_fifo");
#pragma HLS STREAM variable = tcp_pseudo_packet_checksum_fifo depth = 16
#pragma HLS DATA_PACK variable = tcp_pseudo_packet_checksum_fifo

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

  ComputeSubChecksum(tx_tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_subchecksum_fifo);

  CheckChecksum(tcp_pseudo_packet_subchecksum_fifo, tcp_pseudo_packet_checksum_fifo);

  TxEngRemovePseudoHeader(tx_tcp_pseudo_packet_for_tx_eng_fifo, tx_tcp_packet_fifo);

  TxEngConstructIpv4Header(
      tx_tcp_payload_length_fifo, tx_ip_pair_for_ip_header_fifo, tx_ipv4_header_fifo);

  TxEngConstructIpv4Packet(
      tx_ipv4_header_fifo, tcp_pseudo_packet_checksum_fifo, tx_tcp_packet_fifo, tx_ip_pkt_out);
}