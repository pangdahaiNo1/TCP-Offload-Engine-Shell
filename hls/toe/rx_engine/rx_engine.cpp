#include "rx_engine.hpp"

#include "toe/memory_access/memory_access.hpp"
#include "toe/toe_utils.hpp"
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;
/**
 * @details remove the ipv4 hedaaer, and generate the TCP pseudo header above the tcp packet
 *
 * the output @p tcp_pseudo_packet_* are same: tcp pseudo header + tcp header + tcp payload
 * one for validating checksum, the other one for rx eng fsm parse the meta data
 */
void                 RxEngTcpPseudoHeaderInsert(stream<NetAXIS> &    rx_ip_pkt_in,
                                                stream<NetAXISWord> &tcp_pseudo_packet_for_checksum,
                                                stream<NetAXISWord> &tcp_pseudo_packet_for_rx_eng) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  NetAXIS     cur_word;
  NetAXISWord send_word;
  // tcp header and payload length
  ap_uint<16>        tcp_segment_length;
  static NetAXISWord prev_word;
  static ap_uint<4>  ip_header_length;
  static ap_uint<16> ip_packet_length;
  static ap_uint<6>  keep_extra;
  static ap_uint<32> dest_ip_addr;
  static ap_uint<32> src_ip_addr;
  static bool        pseudo_header_not_inserted = false;

  enum PseudoHeaderFsmState { IP_HEADER, TCP_PAYLOAD, EXTRA_WORD };
  static PseudoHeaderFsmState fsm_state = IP_HEADER;

  switch (fsm_state) {
    case (IP_HEADER):
      if (!rx_ip_pkt_in.empty()) {
        rx_ip_pkt_in.read(cur_word);
        ip_header_length = cur_word.data(3, 0);                  // Read IP header len
        ip_packet_length = SwapByte<16>(cur_word.data(31, 16));  // Read IP total len
        src_ip_addr      = cur_word.data(127, 96);
        dest_ip_addr     = cur_word.data(159, 128);

        keep_extra = 8 + (ip_header_length - 5) * 4;
        if (cur_word.last) {
          RemoveIpHeader(NetAXISWord(), cur_word, ip_header_length, send_word);

          tcp_segment_length     = ip_packet_length - (ip_header_length * 4);
          send_word.data(63, 0)  = (dest_ip_addr, src_ip_addr);
          send_word.data(79, 64) = 0x0600;
          send_word.data(95, 80) = SwapByte<16>(tcp_segment_length);
          send_word.keep(11, 0)  = 0xFFF;
          send_word.last         = 1;

          tcp_pseudo_packet_for_checksum.write(send_word);
          tcp_pseudo_packet_for_rx_eng.write(send_word);
        } else {
          pseudo_header_not_inserted = true;
          fsm_state                  = TCP_PAYLOAD;
        }
        prev_word = cur_word;
      }
      break;
    case (TCP_PAYLOAD):
      if (!rx_ip_pkt_in.empty()) {
        rx_ip_pkt_in.read(cur_word);
        RemoveIpHeader(cur_word, prev_word, ip_header_length, send_word);
        if (pseudo_header_not_inserted) {
          pseudo_header_not_inserted = false;
          tcp_segment_length         = ip_packet_length - (ip_header_length * 4);
          send_word.data(63, 0)      = (dest_ip_addr, src_ip_addr);
          send_word.data(79, 64)     = 0x0600;
          send_word.data(95, 80)     = SwapByte<16>(tcp_segment_length);
          send_word.keep(11, 0)      = 0xFFF;
        }
        send_word.last = 0;
        if (cur_word.last) {
          if (cur_word.keep.bit(keep_extra)) {
            fsm_state = EXTRA_WORD;
          } else {
            send_word.last = 1;
            fsm_state      = IP_HEADER;
          }
        }
        prev_word = cur_word;
        tcp_pseudo_packet_for_checksum.write(send_word);
        tcp_pseudo_packet_for_rx_eng.write(send_word);
      }
      break;
    case EXTRA_WORD:
      cur_word.data = 0;
      cur_word.keep = 0;
      RemoveIpHeader(cur_word, prev_word, ip_header_length, send_word);
      send_word.last = 1;
      tcp_pseudo_packet_for_checksum.write(send_word);
      tcp_pseudo_packet_for_rx_eng.write(send_word);
      fsm_state = IP_HEADER;
      break;
  }
}

/** @ingroup rx_engine
 *  This module gets the packet at Pseudo TCP layer.
 *  First of all, it removes the pseudo TCP header and forward the payload if
 * any. It also sends the metaData information to the following module.
 *  @param[in]		tcp_pseudo_packet
 *  @param[out]		tcp_meta_data
 *  @param[out]		tcp_payload
 */
void                 RxEngParseTcpHeader(stream<NetAXISWord> &        tcp_pseudo_packet,
                                         stream<TcpPseudoHeaderMeta> &tcp_meta_data,
                                         stream<NetAXISWord> &        tcp_payload) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum ParseTcpHeaderState { FIRST_WORD, REMAINING_WORDS, EXTRA_WORD };
  static ParseTcpHeaderState parse_fsm_state = FIRST_WORD;

  static NetAXISWord prev_word;
  NetAXISWord        cur_word;
  NetAXISWord        send_word;
  // data_offset * 4 + 12, 12 is the psesudo header length
  static ap_uint<6> tcp_payload_offset_bytes;
  // data_offset in TCP header
  ap_uint<4> tcp_payload_offset_words;
  // in bytes
  ap_uint<16> tcp_payload_length;

  TcpPseudoHeaderMeta tcp_seg_meta;

  switch (parse_fsm_state) {
    case FIRST_WORD:
      if (!tcp_pseudo_packet.empty()) {
        tcp_pseudo_packet.read(cur_word);
        /* Get the TCP Pseudo header total length and subtract the TCP header
         * size. This value is the payload size*/
        // TODO: Use TCP full header class to improve readablity
        tcp_payload_offset_words = cur_word.data(199, 196);
        tcp_payload_length = SwapByte<16>(cur_word.data(95, 80)) - tcp_payload_offset_words * 4;
        /* Only sending data when one-transaction packet has data */
        tcp_payload_offset_bytes = tcp_payload_offset_words * 4 + 12;

        tcp_seg_meta.src_ip             = (cur_word.data(31, 0));
        tcp_seg_meta.dest_ip            = (cur_word.data(63, 32));
        tcp_seg_meta.header.src_port    = (cur_word.data(111, 96));
        tcp_seg_meta.header.dest_port   = (cur_word.data(127, 112));
        tcp_seg_meta.header.data_offset = cur_word.data(199, 196);
        tcp_seg_meta.header.seq_number  = SwapByte<32>(cur_word.data(159, 128));
        tcp_seg_meta.header.ack_number  = SwapByte<32>(cur_word.data(191, 160));
        tcp_seg_meta.header.win_size    = SwapByte<16>(cur_word.data(223, 208));
        tcp_seg_meta.header.cwr         = cur_word.data.bit(207);
        tcp_seg_meta.header.ecn         = cur_word.data.bit(206);
        tcp_seg_meta.header.urg         = cur_word.data.bit(205);
        tcp_seg_meta.header.ack         = cur_word.data.bit(204);
        tcp_seg_meta.header.psh         = cur_word.data.bit(203);
        tcp_seg_meta.header.rst         = cur_word.data.bit(202);
        tcp_seg_meta.header.syn         = cur_word.data.bit(201);
        tcp_seg_meta.header.fin         = cur_word.data.bit(200);

        // Initialize window shift to 0
        tcp_seg_meta.header.win_scale = 0;
        tcp_seg_meta.tcp_options      = cur_word.data(511, 256);
        // record the payload length
        tcp_seg_meta.header.payload_length = tcp_payload_length;
        logger.Info(RX_ENGINE, "Payload Length", tcp_payload_length.to_string(16));

        if (cur_word.last) {
          if (tcp_payload_length != 0) {  // one-transaction packet with any data
            send_word.data = cur_word.data(511, tcp_payload_offset_bytes * 8);
            send_word.keep = cur_word.keep(63, tcp_payload_offset_bytes);
            send_word.last = 1;

            tcp_payload.write(send_word);
          }
        } else if (tcp_payload_length != 0) {
          parse_fsm_state = REMAINING_WORDS;  // TODO: if tcp_data_offset > 13 the payload starts in
                                              // the second transaction
        }
        prev_word = cur_word;
        tcp_meta_data.write(tcp_seg_meta);
      }
      break;
    case REMAINING_WORDS:
      if (!tcp_pseudo_packet.empty()) {
        tcp_pseudo_packet.read(cur_word);

        // Compose the output word
        send_word.data = ((cur_word.data((tcp_payload_offset_bytes * 8) - 1, 0)),
                          prev_word.data(511, tcp_payload_offset_bytes * 8));
        send_word.keep = ((cur_word.keep(tcp_payload_offset_bytes - 1, 0)),
                          prev_word.keep(63, tcp_payload_offset_bytes));

        send_word.last = 0;
        if (cur_word.last) {
          if (cur_word.keep.bit(tcp_payload_offset_bytes)) {
            parse_fsm_state = EXTRA_WORD;
          } else {
            send_word.last  = 1;
            parse_fsm_state = FIRST_WORD;
          }
        }
        prev_word = cur_word;
        tcp_payload.write(send_word);
      }
      break;
    case EXTRA_WORD:
      send_word.data = prev_word.data(511, tcp_payload_offset_bytes * 8);
      send_word.keep = prev_word.keep(63, tcp_payload_offset_bytes);
      send_word.last = 1;
      tcp_payload.write(send_word);
      parse_fsm_state = FIRST_WORD;
      break;
  }
}

/**
 * Optional Header Fields are only parse on connection establishment to determine
 * the window scaling factor
 * Available options (during SYN):
 * kind | length | description
 *   0  |     1B | End of options list
 *   1  |     1B | NOP/Padding
 *   2  |     4B | MSS (Maximum segment size)
 *   3  |     3B | Window scale
 *   4  |     2B | SACK permitted (Selective Acknowledgment)
 *
 * if the segment is not a syn packet, the meta info is forword directly
 *
 * @param[in] tcp_meta_data_in, without options
 * @param[out] tcp_meta_data_out, with options field
 */
void                 RxEngParseTcpHeaderOptions(stream<TcpPseudoHeaderMeta> &tcp_meta_data_in,
                                                stream<TcpPseudoHeaderMeta> &tcp_meta_data_out) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum RxParseOptionsState {
    READ_META,
    PARSE_OPTIONS
    //  WRITE_META
  };  // WRITE_META not used, it will reduce one cycle
  static RxParseOptionsState fsm_state = READ_META;
  static TcpPseudoHeaderMeta tcp_meta_reg;

  static ap_uint<9> tcp_options_length;  // max = 320bit
  static ap_uint<8> cur_tcp_options_index;

  ap_uint<8> cur_option_kind;
  ap_uint<8> cur_option_length;
  bool       meta_to_be_sent = false;

  switch (fsm_state) {
    case READ_META:
      if (!tcp_meta_data_in.empty()) {
        tcp_meta_data_in.read(tcp_meta_reg);

        if (tcp_meta_reg.header.syn && tcp_meta_reg.header.data_offset > 5) {
          cur_tcp_options_index = 0;
          tcp_options_length    = (tcp_meta_reg.header.data_offset - 5) * 4;
          fsm_state             = PARSE_OPTIONS;
        } else {
          tcp_meta_reg.tcp_options = 0;
          tcp_meta_data_out.write(tcp_meta_reg);
        }
      }
      break;
    case PARSE_OPTIONS:
      cur_option_kind   = tcp_meta_reg.tcp_options(7, 0);
      cur_option_length = tcp_meta_reg.tcp_options(15, 8);
      if (tcp_options_length > cur_tcp_options_index) {
        switch (cur_option_kind) {
          case 0:
            meta_to_be_sent = true;
            break;
          case 1:
            cur_option_length = 1;
            break;
          case 3:
            meta_to_be_sent = true;
            if (cur_option_length == 3) {
              // choose the min
              tcp_meta_reg.header.win_scale = (tcp_meta_reg.tcp_options(19, 16) > WINDOW_SCALE_BITS)
                                                  ? WINDOW_SCALE_BITS
                                                  : tcp_meta_reg.tcp_options(19, 16);
            }
            break;
          default:
            break;
        }
        tcp_meta_reg.tcp_options = tcp_meta_reg.tcp_options >> (cur_option_length * 8);
        cur_tcp_options_index    = cur_tcp_options_index + cur_option_length;
      } else {
        meta_to_be_sent = true;
      }
      if (meta_to_be_sent) {
        tcp_meta_reg.tcp_options = 0;
        tcp_meta_data_out.write(tcp_meta_reg);
        fsm_state = READ_META;
      }
      break;
  }
}

/**
 * check the tcp pseudo header checksum is equal to zero, and send the checking port request
 *
 * @param[in] tcp_checksum_in
 * @param[in] tcp_meta_data_in
 * @param[out] tcp_payload_should_be_dropped: if checksum is invalid, the payload should be valid
 * @param[out] tcp_meta_data_out
 * @param[out] rx_eng_to_ptable_check_req
 */
void                 RxEngVerifyChecksumAndPort(stream<ap_uint<16> > &       tcp_checksum_in,
                                                stream<TcpPseudoHeaderMeta> &tcp_meta_data_in,
                                                stream<bool> &               tcp_payload_dropped_by_checksum,
                                                stream<TcpPseudoHeaderMeta> &tcp_meta_data_out,
                                                stream<TcpPortNumber> &      rx_eng_to_ptable_check_req) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1
  enum RxVerifyFsmState { READ_META_INFO, READ_CHECKSUM };
  static RxVerifyFsmState    fsm_state = READ_META_INFO;
  static TcpPseudoHeaderMeta tcp_meta_reg;

  ap_uint<16> tcp_seg_checksum;
  bool        tcp_seg_is_valid;

  switch (fsm_state) {
    case READ_META_INFO:
      if (!tcp_meta_data_in.empty()) {
        tcp_meta_data_in.read(tcp_meta_reg);
        fsm_state = READ_CHECKSUM;
      }
      break;
    case READ_CHECKSUM:
      if (!tcp_checksum_in.empty()) {
        tcp_checksum_in.read(tcp_seg_checksum);
        logger.Info(TOE_TOP, RX_ENGINE, "Recv Seg Checksum", tcp_seg_checksum.to_string(16), false);
        tcp_seg_is_valid = (tcp_seg_checksum == 0);
        if (tcp_seg_is_valid) {
          tcp_meta_data_out.write(tcp_meta_reg);
          rx_eng_to_ptable_check_req.write(SwapByte<16>(tcp_meta_reg.header.dest_port));
          logger.Info(RX_ENGINE,
                      PORT_TBLE,
                      "Check port req",
                      SwapByte<16>(tcp_meta_reg.header.dest_port).to_string(16));
        }
        if (tcp_meta_reg.header.payload_length) {
          tcp_payload_dropped_by_checksum.write(!tcp_seg_is_valid);
        }
        fsm_state = READ_META_INFO;
      }
      break;
  }
}

/**
 * check port and session is valid, get the role id from port table
 *
 * @param[in] tcp_meta_data_in
 * @param[in] ptable_to_rx_eng_check_rsp
 * @param[out] rx_eng_to_slookup_req
 * @param[in] slookup_to_rx_eng_rsp
 * @param[out] rx_eng_fsm_meta_data_out
 * @param[out] tcp_payload_should_be_dropped: if the port or session id not valid, the payload
 * should be dropped
 * @param[out] tcp_payload_tdest:
 * @param[out] rx_eng_meta_to_event_eng_set_event
 *
 * */
void                 RxEngTcpMetaHandler(stream<TcpPseudoHeaderMeta> &tcp_meta_data_in,
                                         stream<PtableToRxEngRsp> &   ptable_to_rx_eng_check_rsp,
                                         stream<RxEngToSlookupReq> &  rx_eng_to_slookup_req,
                                         stream<SessionLookupRsp> &   slookup_to_rx_eng_rsp,
                                         stream<RxEngFsmMetaData> &   rx_eng_fsm_meta_data_out,
                                         stream<bool> &               tcp_payload_dropped_by_port_or_session,
                                         stream<NetAXISDest> &        tcp_payload_tdest,
                                         stream<EventWithTuple> &     rx_eng_meta_to_event_eng_set_event) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum RxTcpMetaFsmState { META, LOOKUP };
  static RxTcpMetaFsmState fsm_state = META;

  static TcpPseudoHeaderMeta tcp_meta_reg;
  static PtableToRxEngRsp    ptable_rsp_reg;
  static SessionLookupRsp    slookup_rsp_reg;
  static TcpPortNumber       meta_dst_port_reg;
  static IpAddr              meta_src_addr_reg;
  FourTuple                  four_tuple;

  switch (fsm_state) {
    case META:
      if (!tcp_meta_data_in.empty() && !ptable_to_rx_eng_check_rsp.empty()) {
        tcp_meta_data_in.read(tcp_meta_reg);
        ptable_to_rx_eng_check_rsp.read(ptable_rsp_reg);
        logger.Info(PORT_TBLE, RX_ENGINE, "PortChk Rsp", ptable_rsp_reg.to_string());
        // if port is closed
        if (!ptable_rsp_reg.is_open) {
          // when port is closed, send rst
          if (!tcp_meta_reg.header.rst) {
            four_tuple.src_ip_addr  = tcp_meta_reg.dest_ip;
            four_tuple.dst_ip_addr  = tcp_meta_reg.src_ip;
            four_tuple.src_tcp_port = tcp_meta_reg.header.dest_port;
            four_tuple.dst_tcp_port = tcp_meta_reg.header.src_port;
            EventWithTuple rst_event(
                RstEvent(tcp_meta_reg.header.seq_number + tcp_meta_reg.header.get_payload_length()),
                four_tuple);
            logger.Info(TOE_TOP, RX_ENGINE, "RST Event", rst_event.to_string());
            rx_eng_meta_to_event_eng_set_event.write(rst_event);
          }
          if (tcp_meta_reg.header.payload_length != 0) {
            // Drop payload because port is closed
            tcp_payload_dropped_by_port_or_session.write(true);
            tcp_payload_tdest.write(ptable_rsp_reg.role_id);
          }
        } else {
          // port is open
          // Make session lookup, only allow creation of new entry when SYN or SYN_ACK
          RxEngToSlookupReq slookup_req(
              FourTuple(tcp_meta_reg.src_ip,
                        tcp_meta_reg.dest_ip,
                        (tcp_meta_reg.header.src_port),
                        (tcp_meta_reg.header.dest_port)),
              (tcp_meta_reg.header.syn && !tcp_meta_reg.header.rst && !tcp_meta_reg.header.fin),
              ptable_rsp_reg.role_id);
          rx_eng_to_slookup_req.write(slookup_req);
          logger.Info(RX_ENGINE, SLUP_CTRL, "Session Lup or Creaate", slookup_req.to_string());
          fsm_state = LOOKUP;
        }
      }
      break;
    case LOOKUP:  // wait for slookup response
      if (!slookup_to_rx_eng_rsp.empty()) {
        slookup_to_rx_eng_rsp.read(slookup_rsp_reg);
        logger.Info(SLUP_CTRL, RX_ENGINE, "Session Lup Rsp", slookup_rsp_reg.to_string());
        if (slookup_rsp_reg.hit) {
          RxEngFsmMetaData rx_eng_fsm_meta(slookup_rsp_reg.session_id,
                                           tcp_meta_reg.src_ip,
                                           tcp_meta_reg.header.dest_port,
                                           tcp_meta_reg.header,
                                           ptable_rsp_reg.role_id);
          logger.Info(TOE_TOP, RX_ENGINE, "Rx FSM Meta", rx_eng_fsm_meta.to_string(), true);
          rx_eng_fsm_meta_data_out.write(rx_eng_fsm_meta);
        }
        if (tcp_meta_reg.header.payload_length != 0) {
          tcp_payload_dropped_by_port_or_session.write(!slookup_rsp_reg.hit);
          tcp_payload_tdest.write(ptable_rsp_reg.role_id);
        }
        fsm_state = META;
      }
      break;
  }
}

/**
 * the output payload is valid, should be delivery to Memory or App
 */
void RxEngTcpPayloadDropper(stream<NetAXISWord> &tcp_payload_in,
                            stream<bool> &       tcp_payload_dropped_by_checksum,
                            stream<bool> &       tcp_payload_dropped_by_port_or_session,
                            stream<NetAXISDest> &tcp_payload_tdest,
                            stream<bool> &       tcp_payload_dropped_by_rx_fsm,
#if !TCP_RX_DDR_BYPASS
                            // send data to datamover
                            stream<NetAXISWord> &tcp_payload_out
#else
                            // send data to rx app intf
                            stream<NetAXISWord> &tcp_payload_out
#endif
) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum RxDropperFsmState { RD_VERIFY_CHECKSUM, RD_META_HANDLER, RD_FSM_DROP, FWD, DROP };
  static RxDropperFsmState fsm_state = RD_VERIFY_CHECKSUM;
  static NetAXISDest       tcp_payload_role_id;
  bool                     drop;
  NetAXISWord              cur_word;

  switch (fsm_state) {
    case RD_VERIFY_CHECKSUM:
      if (!tcp_payload_dropped_by_checksum.empty()) {
        tcp_payload_dropped_by_checksum.read(drop);
        logger.Info(TOE_TOP, RX_ENGINE, "Drop Payload by checksum?", (drop ? "1" : "0"));
        fsm_state = (drop) ? DROP : RD_META_HANDLER;
      }
      break;
    case RD_META_HANDLER:
      if (!tcp_payload_dropped_by_port_or_session.empty() && !tcp_payload_tdest.empty()) {
        tcp_payload_dropped_by_port_or_session.read(drop);
        tcp_payload_tdest.read(tcp_payload_role_id);
        logger.Info(TOE_TOP, RX_ENGINE, "Drop Payload by port or session?", (drop ? "1" : "0"));
        fsm_state = (drop) ? DROP : RD_FSM_DROP;
      }
      break;
    case RD_FSM_DROP:
      if (!tcp_payload_dropped_by_rx_fsm.empty()) {
        tcp_payload_dropped_by_rx_fsm.read(drop);
        logger.Info(RX_ENGINE, NET_APP, "Drop Payload by FSM?: ", (drop ? "1" : "0"));
        fsm_state = (drop) ? DROP : FWD;
      }
      break;
    case FWD:
      if (!tcp_payload_in.empty()) {
        tcp_payload_in.read(cur_word);
        cur_word.dest = tcp_payload_role_id;
        logger.Info(RX_ENGINE, MISC_MDLE, "Recv TCP Payload", cur_word.to_string());
#if !TCP_RX_DDR_BYPASS
        // send data to datamover
        tcp_payload_out.write(cur_word.to_net_axis());
#else
        // send data to rx app intf
        tcp_payload_out.write(cur_word);
#endif
        fsm_state = (cur_word.last) ? RD_VERIFY_CHECKSUM : FWD;
      }
      break;
    case DROP:
      if (!tcp_payload_in.empty()) {
        tcp_payload_in.read(cur_word);
        logger.Info(RX_ENGINE, NET_APP, "Drop TCP Payload", cur_word.to_string());
        fsm_state = (cur_word.last) ? RD_VERIFY_CHECKSUM : DROP;
      }
      break;
  }  // switch
}

/**
 * @param[in] rx_eng_fsm_meta_data_in
 * @param[in] slookup_to_rx_eng_rsp
 * @param[out] rx_eng_to_rx_sar_req
 * @param[in] rx_sar_to_rx_eng_rsp
 * @param[out] rx_eng_to_tx_sar_req
 * @param[in] tx_sar_to_rx_eng_rsp
 * @param[out] rx_eng_to_timer_clear_rtimer
 * @param[out] rx_eng_to_timer_clear_ptimer
 * @param[out] rx_eng_to_timer_set_ctimer
 * @param[out]
 */
void RxEngTcpFsm(
    // read in rx eng meta data
    stream<RxEngFsmMetaData> &rx_eng_fsm_meta_data_in,
    // Rx SAR R/W
    stream<RxEngToRxSarReq> &rx_eng_to_rx_sar_req,
    stream<RxSarTableEntry> &rx_sar_to_rx_eng_rsp,
    // TX SAR R/W
    stream<RxEngToTxSarReq> &rx_eng_to_tx_sar_req,
    stream<TxSarToRxEngRsp> &tx_sar_to_rx_eng_rsp,
    // state table
    stream<StateTableReq> &rx_eng_to_sttable_req,
    stream<SessionState> & sttable_to_rx_eng_rsp,
    // update timer state
    stream<TcpSessionID> &          rx_eng_to_timer_set_ctimer,
    stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
    stream<TcpSessionID> &          rx_eng_to_timer_clear_ptimer,
    // set event engine
    stream<Event> &rx_eng_fsm_to_event_eng_set_event,
    // to app connection notify, when net app active open a connection
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // to app connection notify, when net app passive open a conection
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    // to app data notify
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification,
#if !TCP_RX_DDR_BYPASS
    stream<MemBufferRWCmd> &rx_eng_to_mem_cmd,
#endif
    // to app payload should be dropped?
    stream<bool> &tcp_payload_dropped_by_rx_fsm

) {
#pragma HLS LATENCY  max = 2
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum RxEngTcpFsmState { LOAD, TRANSITION };
  static RxEngTcpFsmState fsm_state = LOAD;

  static RxEngFsmMetaData tcp_rx_meta_reg;

  static bool       rx_eng_sent_tx_sar_req = false;
  static ap_uint<4> rx_eng_ctrl_bits       = 0;
  // current session state, RX SAR, etc
  static SessionState    session_state_reg;
  static RxSarTableEntry rx_sar_reg;
  static TxSarToRxEngRsp tx_sar_reg;

  ap_uint<32>                  payload_mem_addr = 0;
  ap_uint<4>                   rx_win_scale;  // used to computed the scale option for RX buffer
  ap_uint<4>                   tx_win_scale;  // used to computed the scale option for TX buffer
  RxEngToRetransTimerReq       to_rtimer_req;
  AppNotificationNoTDEST       to_rx_app_notify;
  Event                        to_event_eng_event;
  RxEngToTxSarReq              to_tx_sar_req;
  RxEngToRxSarReq              to_rx_sar_req;
  StateTableReq                to_sttable_req;
  OpenConnRspNoTDEST           to_tx_app_open_conn;
  NewClientNotificationNoTDEST to_tx_app_new_client_notify;
  MemBufferRWCmd               to_mem_write_cmd;

  switch (fsm_state) {
    case LOAD:
      if (!rx_eng_fsm_meta_data_in.empty()) {
        rx_eng_fsm_meta_data_in.read(tcp_rx_meta_reg);

        // query request for current session state
        rx_eng_to_sttable_req.write(StateTableReq(tcp_rx_meta_reg.session_id));
        logger.Info(RX_ENGINE, STAT_TBLE, "State LupReq", tcp_rx_meta_reg.session_id.to_string(16));
        // query requset for current session RX SAR table
        to_rx_sar_req = RxEngToRxSarReq(tcp_rx_meta_reg.session_id);
        rx_eng_to_rx_sar_req.write(to_rx_sar_req);
        logger.Info(RX_ENGINE, RX_SAR_TB, "RX SAR Req", to_rx_sar_req.to_string());

        if (tcp_rx_meta_reg.header.ack) {
          rx_eng_to_tx_sar_req.write(RxEngToTxSarReq(tcp_rx_meta_reg.session_id));
          logger.Info(RX_ENGINE, TX_SAR_TB, "TX SAR Req", tcp_rx_meta_reg.session_id.to_string(16));
          rx_eng_sent_tx_sar_req = true;
        }
        rx_eng_ctrl_bits[0] = tcp_rx_meta_reg.header.ack;
        rx_eng_ctrl_bits[1] = tcp_rx_meta_reg.header.syn;
        rx_eng_ctrl_bits[2] = tcp_rx_meta_reg.header.fin;
        rx_eng_ctrl_bits[3] = tcp_rx_meta_reg.header.rst;
        logger.Info(RX_ENGINE, "CtrlBits", rx_eng_ctrl_bits.to_string(2));
        fsm_state = TRANSITION;
      }
      break;
    case TRANSITION:
      if (!sttable_to_rx_eng_rsp.empty() && !rx_sar_to_rx_eng_rsp.empty() &&
          !(rx_eng_sent_tx_sar_req && tx_sar_to_rx_eng_rsp.empty())) {
        sttable_to_rx_eng_rsp.read(session_state_reg);
        rx_sar_to_rx_eng_rsp.read(rx_sar_reg);
        logger.Info(STAT_TBLE, RX_ENGINE, "State LupRsp", state_to_string(session_state_reg));
        logger.Info(RX_SAR_TB, RX_ENGINE, "RX SAR Rsp", rx_sar_reg.to_string());
        if (rx_eng_sent_tx_sar_req) {
          tx_sar_to_rx_eng_rsp.read(tx_sar_reg);
          logger.Info(TX_SAR_TB, RX_ENGINE, "TX SAR Rsp", tx_sar_reg.to_string());
        }
        rx_eng_sent_tx_sar_req = false;

        fsm_state = LOAD;
      }
      switch (rx_eng_ctrl_bits) {
        case 1:  // ACK
          if (fsm_state == LOAD) {
            logger.Info(RX_ENGINE, "Recv ACK");
            // if ack number is expected, stop the retransmit timer
            to_rtimer_req =
                RxEngToRetransTimerReq(tcp_rx_meta_reg.session_id,
                                       (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte));
            rx_eng_to_timer_clear_rtimer.write(to_rtimer_req);
            logger.Info(RX_ENGINE, RTRMT_TMR, "Clear RTimer?", to_rtimer_req.to_string());
            if (session_state_reg == ESTABLISHED || session_state_reg == SYN_RECEIVED ||
                session_state_reg == FIN_WAIT_1 || session_state_reg == CLOSING ||
                session_state_reg == LAST_ACK) {
              // if old ACK arrived
              if (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.perv_ack &&
                  tx_sar_reg.perv_ack != tx_sar_reg.next_byte) {
                // Old ACK increase counter only if it does not contain data
                logger.Info(RX_ENGINE, "Recv Old ACK");
                if (tcp_rx_meta_reg.header.payload_length == 0) {
                  tx_sar_reg.retrans_count++;
                  logger.Info(RX_ENGINE, "Payload is empty, increase RT cnt");
                }
              } else {  // new ACK arrived
                // clear probe timer for new ACK
                logger.Info(RX_ENGINE, "Recv New ACK");
                logger.Info(
                    RX_ENGINE, PROBE_TMR, "Clr PTimer", tcp_rx_meta_reg.session_id.to_string(16));
                rx_eng_to_timer_clear_ptimer.write(tcp_rx_meta_reg.session_id);
                // Check for SlowStart & Increase Congestion Window
                if (tx_sar_reg.cong_window <= (tx_sar_reg.slowstart_threshold - TCP_MSS)) {
                  tx_sar_reg.cong_window += TCP_MSS;
                } else if (tx_sar_reg.cong_window <= CONGESTION_WINDOW_MAX) {
                  /*In theory (1/cong_window) should be added however increasing
                   * with floating/fixed point would increase memory usage */
                  tx_sar_reg.cong_window++;
                }
                tx_sar_reg.retrans_count = 0;
                tx_sar_reg.fast_retrans  = false;
              }
              // TX SAR update, when new valid ACK
              if ((tx_sar_reg.perv_ack <= tcp_rx_meta_reg.header.ack_number &&
                   tcp_rx_meta_reg.header.ack_number <= tx_sar_reg.next_byte) ||
                  ((tx_sar_reg.perv_ack <= tcp_rx_meta_reg.header.ack_number ||
                    tcp_rx_meta_reg.header.ack_number <= tx_sar_reg.next_byte) &&
                   tx_sar_reg.next_byte < tx_sar_reg.perv_ack)) {
                to_tx_sar_req =
                    RxEngToTxSarReq(tcp_rx_meta_reg.session_id,
                                    tcp_rx_meta_reg.header.ack_number,
                                    tcp_rx_meta_reg.header.win_size,
                                    tx_sar_reg.cong_window,
                                    tx_sar_reg.retrans_count,
                                    ((tx_sar_reg.retrans_count == 3) || tx_sar_reg.fast_retrans),
                                    tx_sar_reg.win_shift);
                logger.Info(RX_ENGINE, TX_SAR_TB, "Tx SAR Upd", to_tx_sar_req.to_string());
                rx_eng_to_tx_sar_req.write(to_tx_sar_req);
                // RX SAR update, when new payload arrived
                if (tcp_rx_meta_reg.header.payload_length != 0) {
                  ap_uint<32> new_recvd =
                      tcp_rx_meta_reg.header.seq_number + tcp_rx_meta_reg.header.payload_length;
                  TcpSessionBuffer free_space =
                      ((rx_sar_reg.app_read - rx_sar_reg.recvd(WINDOW_BITS - 1, 0)) - 1);
                  // check if segment is in order and if enough free space is available
                  if ((tcp_rx_meta_reg.header.seq_number == rx_sar_reg.recvd) &&
                      (free_space > tcp_rx_meta_reg.header.payload_length)) {
                    to_rx_sar_req = RxEngToRxSarReq(tcp_rx_meta_reg.session_id, new_recvd, 1);
                    rx_eng_to_rx_sar_req.write(to_rx_sar_req);
                    logger.Info(
                        RX_ENGINE, RX_SAR_TB, "Upd Req, Seg is inorder", to_rx_sar_req.to_string());
#if (!TCP_RX_DDR_BYPASS)
                    GetSessionMemAddr<1>(tcp_rx_meta_reg.session_id,
                                         tcp_rx_meta_reg.header.seq_number,
                                         payload_mem_addr);
                    to_mem_write_cmd =
                        MemBufferRWCmd(payload_mem_addr, tcp_rx_meta_reg.header.payload_length);
                    rx_eng_to_mem_cmd.write(to_mem_write_cmd);
                    logger.Info(RX_ENGINE, "WriteMem Cmd", to_mem_write_cmd.to_string());
#endif
                    // Only notify when new data available
                    to_rx_app_notify = AppNotificationNoTDEST(tcp_rx_meta_reg.session_id,
                                                              tcp_rx_meta_reg.header.payload_length,
                                                              tcp_rx_meta_reg.src_ip,
                                                              tcp_rx_meta_reg.dst_port);
                    logger.Info(
                        RX_ENGINE, RX_APP_IF, "New Data Notify", to_rx_app_notify.to_string());
                    rx_eng_to_rx_app_notification.write(to_rx_app_notify);
                    tcp_payload_dropped_by_rx_fsm.write(false);
                  } else {
                    tcp_payload_dropped_by_rx_fsm.write(true);
                  }
                }
                // retransmit or ack
                if ((tx_sar_reg.retrans_count == 3) && !tx_sar_reg.fast_retrans) {
                  to_event_eng_event = Event(RT, tcp_rx_meta_reg.session_id);
                } else {
                  to_event_eng_event = Event(ACK, tcp_rx_meta_reg.session_id);
                }
                // only ack to other side, if the segment contains the data
                if (tcp_rx_meta_reg.header.payload_length != 0) {
                  logger.Info(RX_ENGINE,
                              EVENT_ENG,
                              "Recv inorder ACK Event",
                              to_event_eng_event.to_string());
                  rx_eng_fsm_to_event_eng_set_event.write(to_event_eng_event);
                }

                if (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte) {
                  // This is necessary to unlock stateTable
                  switch (session_state_reg) {
                    case SYN_RECEIVED:
                      to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, ESTABLISHED, 1);
                      // Notify the tx App about new client
                      // TODO 1460 is the default value, but it could change if options
                      // are presented in the TCP header
                      to_tx_app_new_client_notify = NewClientNotificationNoTDEST(
                          tcp_rx_meta_reg.session_id, 0, 1460, TCP_NODELAY, true);
                      logger.Info(RX_ENGINE,
                                  TX_APP_IF,
                                  "New Client Notify",
                                  to_tx_app_new_client_notify.to_string());
                      rx_eng_to_tx_app_new_client_notification.write(to_tx_app_new_client_notify);
                      break;
                    case CLOSING:
                      logger.Info(RX_ENGINE,
                                  CLOSE_TMR,
                                  "Set CTimer",
                                  tcp_rx_meta_reg.session_id.to_string(16));
                      to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, TIME_WAIT, 1);
                      rx_eng_to_timer_set_ctimer.write(tcp_rx_meta_reg.session_id);
                      break;
                    case LAST_ACK:
                      to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1);
                      break;
                    default:
                      to_sttable_req =
                          StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
                      break;
                  }

                } else {  // we have to release the lock
                  to_sttable_req = StateTableReq(
                      tcp_rx_meta_reg.session_id, session_state_reg, 1);  // or ESTABLISHED
                }
                logger.Info(
                    RX_ENGINE, STAT_TBLE, "Release lock/Upd State", to_sttable_req.to_string());
                rx_eng_to_sttable_req.write(to_sttable_req);
              }
            } else {  // state = CLOSED || SYN_SENT || CLOSE_WAIT || FIN_WAIT2 || TIME_WAIT
              // SENT RST, RFC 793: fig.11
              to_event_eng_event = RstEvent(tcp_rx_meta_reg.session_id,
                                            tcp_rx_meta_reg.header.seq_number +
                                                tcp_rx_meta_reg.header.payload_length);
              logger.Info(RX_ENGINE, EVENT_ENG, "RST Event", to_event_eng_event.to_string());
              rx_eng_fsm_to_event_eng_set_event.write(to_event_eng_event);
              // if data is in the pipe it needs to be dropped
              if (tcp_rx_meta_reg.header.payload_length != 0) {
                tcp_payload_dropped_by_rx_fsm.write(true);
              }
              to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
              logger.Info(RX_ENGINE, STAT_TBLE, "Release lock", to_sttable_req.to_string());
              rx_eng_to_sttable_req.write(
                  StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
            }
          }
          break;
        case 2:  // SYN
          if (fsm_state == LOAD) {
            // check state is LISTEN || SYN_SENT
            if (session_state_reg == CLOSED || session_state_reg == SYN_SENT) {
              logger.Info(RX_ENGINE, "Recv SYN");

              //  If the other side announces a WSopt we use WINDOW_SCALE_BITS
              rx_win_scale  = (tcp_rx_meta_reg.header.win_scale == 0) ? 0 : WINDOW_SCALE_BITS;
              tx_win_scale  = tcp_rx_meta_reg.header.win_scale;
              to_rx_sar_req = RxEngToRxSarReq(tcp_rx_meta_reg.session_id,
                                              tcp_rx_meta_reg.header.seq_number + 1,
                                              1,
                                              1,
                                              rx_win_scale);
              logger.Info(RX_ENGINE, RX_SAR_TB, "Upd Req", to_rx_sar_req.to_string());
              rx_eng_to_rx_sar_req.write(to_rx_sar_req);
              to_tx_sar_req = RxEngToTxSarReq(tcp_rx_meta_reg.session_id,
                                              0,
                                              tcp_rx_meta_reg.header.win_size,
                                              tx_sar_reg.cong_window,
                                              0,
                                              false,
                                              true,
                                              rx_win_scale);
              rx_eng_to_tx_sar_req.write(to_tx_sar_req);
              logger.Info(RX_ENGINE, TX_SAR_TB, "Upd Req", to_tx_sar_req.to_string());
              to_event_eng_event = Event(SYN_ACK, tcp_rx_meta_reg.session_id);
              logger.Info(RX_ENGINE, EVENT_ENG, "Recv SYN Event", to_event_eng_event.to_string());
              to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, SYN_RECEIVED, 1);
              logger.Info(RX_ENGINE, STAT_TBLE, "Upd State", to_sttable_req.to_string());

            } else if (session_state_reg == SYN_RECEIVED) {
              // If it is the same SYN, we resent SYN-ACK, almost like quick RT, we
              // could also wait for RT timer
              if (tcp_rx_meta_reg.header.seq_number + 1 == rx_sar_reg.recvd) {
                // Retransmit SYN_ACK

                to_event_eng_event = Event(SYN_ACK, tcp_rx_meta_reg.session_id, 1);
                logger.Info(
                    RX_ENGINE, EVENT_ENG, "RT SYN_ACK Event", to_event_eng_event.to_string());

                to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
                logger.Info(RX_ENGINE, STAT_TBLE, "Release State", to_sttable_req.to_string());

              } else {  // Sent RST, RFC 793: fig.9 (old) duplicate SYN(+ACK)
                        // length == 0
                to_event_eng_event =
                    RstEvent(tcp_rx_meta_reg.session_id, tcp_rx_meta_reg.header.seq_number + 1);
                logger.Info(RX_ENGINE, EVENT_ENG, "Recv SYN Event", to_event_eng_event.to_string());

                to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1);
                logger.Info(RX_ENGINE, STAT_TBLE, "Upd State", to_sttable_req.to_string());
              }
            } else {  // Any synchronized state
              // Unexpected SYN arrived, reply with normal ACK, RFC 793: fig.10
              to_event_eng_event = Event(ACK_NODELAY, tcp_rx_meta_reg.session_id);
              logger.Info(RX_ENGINE, EVENT_ENG, "Recv SYN Event", to_event_eng_event.to_string());

              to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
              logger.Info(RX_ENGINE, STAT_TBLE, "Release State", to_sttable_req.to_string());
            }
            rx_eng_to_sttable_req.write(to_sttable_req);
            rx_eng_fsm_to_event_eng_set_event.write(to_event_eng_event);
          }

          break;
        case 3:  // SYN + ACK
          if (fsm_state == LOAD) {
            logger.Info(RX_ENGINE, "Recv SYN+ACK");
            // Clear SYN retransmission time  if ack number is correct
            to_rtimer_req =
                RxEngToRetransTimerReq(tcp_rx_meta_reg.session_id,
                                       (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte));
            logger.Info(RX_ENGINE, RTRMT_TMR, "Clear RTimer?", to_rtimer_req.to_string());
            rx_eng_to_timer_clear_rtimer.write(to_rtimer_req);
            // A SYN was already send, ack number has to be check,  if is correct send ACK is not
            // send a RST
            if (session_state_reg == SYN_SENT) {
              // check ack number is correct
              if (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte) {
                to_event_eng_event = Event(ACK_NODELAY, tcp_rx_meta_reg.session_id);
                logger.Info(RX_ENGINE, EVENT_ENG, "ACK Event", to_event_eng_event.to_string());
                // Update TCP FSM to ESTABLISHED now data can be`
                // transfer initialize rx_sar, SEQ + phantom byte, last
                // '1' for appd init + Window scale if enable
                to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, ESTABLISHED, 1);
                logger.Info(RX_ENGINE, STAT_TBLE, "Upd State", to_sttable_req.to_string());

                rx_win_scale  = (tcp_rx_meta_reg.header.win_scale == 0) ? 0 : WINDOW_SCALE_BITS;
                tx_win_scale  = tcp_rx_meta_reg.header.win_scale;
                to_rx_sar_req = RxEngToRxSarReq(tcp_rx_meta_reg.session_id,
                                                tcp_rx_meta_reg.header.seq_number + 1,
                                                1,
                                                1,
                                                rx_win_scale);

                rx_eng_to_rx_sar_req.write(to_rx_sar_req);
                logger.Info(RX_ENGINE, RX_SAR_TB, "Upd Req", to_rx_sar_req.to_string());

                // TX Sar table is initialized with the received window scale
                to_tx_sar_req = RxEngToTxSarReq(tcp_rx_meta_reg.session_id,
                                                tcp_rx_meta_reg.header.ack_number,
                                                tcp_rx_meta_reg.header.win_size,
                                                tx_sar_reg.cong_window,
                                                0,
                                                false,
                                                true,
                                                tx_win_scale);
                rx_eng_to_tx_sar_req.write(to_tx_sar_req);
                logger.Info(RX_ENGINE, TX_SAR_TB, "Upd Req", to_tx_sar_req.to_string());

                // to tx app
                logger.Info(RX_ENGINE,
                            TX_APP_IF,
                            "Open Conn Success",
                            tcp_rx_meta_reg.session_id.to_string(16));
                rx_eng_to_tx_app_notification.write(
                    OpenConnRspNoTDEST(tcp_rx_meta_reg.session_id, true));
              } else {
                // Sent RST, RFC 793: fig.9 (old) duplicate SYN(+ACK)
                to_event_eng_event = RstEvent(tcp_rx_meta_reg.session_id,
                                              tcp_rx_meta_reg.header.seq_number +
                                                  tcp_rx_meta_reg.header.payload_length + 1);
                logger.Info(RX_ENGINE, EVENT_ENG, "RST Event", to_event_eng_event.to_string());

                to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1);
                logger.Info(RX_ENGINE, STAT_TBLE, "Upd State", to_sttable_req.to_string());
              }
            } else {
              // Unexpected SYN arrived, reply with normal ACK, RFC 793: fig.10
              to_event_eng_event = Event(ACK_NODELAY, tcp_rx_meta_reg.session_id);
              to_sttable_req     = StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
              logger.Info(RX_ENGINE, EVENT_ENG, "ACK Event", to_event_eng_event.to_string());
              logger.Info(RX_ENGINE, STAT_TBLE, "Release State", to_sttable_req.to_string());
            }
            rx_eng_to_sttable_req.write(to_sttable_req);
            rx_eng_fsm_to_event_eng_set_event.write(to_event_eng_event);
          }
          break;
        case 5:  // FIN + ACK
          if (fsm_state == LOAD) {
            logger.Info(RX_ENGINE, "Recv FIN+ACK");
            to_rtimer_req =
                RxEngToRetransTimerReq(tcp_rx_meta_reg.session_id,
                                       (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte));
            rx_eng_to_timer_clear_rtimer.write(to_rtimer_req);
            logger.Info(RX_ENGINE, RTRMT_TMR, "Clear RTimer?", to_rtimer_req.to_string());

            if ((session_state_reg == ESTABLISHED || session_state_reg == FIN_WAIT_1 ||
                 session_state_reg == FIN_WAIT_2) &&
                (rx_sar_reg.recvd == tcp_rx_meta_reg.header.seq_number)) {
              to_tx_sar_req = RxEngToTxSarReq(tcp_rx_meta_reg.session_id,
                                              tcp_rx_meta_reg.header.ack_number,
                                              tcp_rx_meta_reg.header.win_size,
                                              tx_sar_reg.cong_window,
                                              tx_sar_reg.retrans_count,
                                              tx_sar_reg.fast_retrans,
                                              tx_win_scale);
              rx_eng_to_tx_sar_req.write(to_tx_sar_req);
              logger.Info(RX_ENGINE, TX_SAR_TB, "Upd Req", to_tx_sar_req.to_string());

              // +1 for phantom byte, there might be data too
              to_rx_sar_req = RxEngToRxSarReq(tcp_rx_meta_reg.session_id,
                                              tcp_rx_meta_reg.header.seq_number +
                                                  tcp_rx_meta_reg.header.payload_length + 1,
                                              1);
              rx_eng_to_rx_sar_req.write(to_rx_sar_req);
              logger.Info(RX_ENGINE, RX_SAR_TB, "Upd Req", to_rx_sar_req.to_string());

              // Clear the probe timer
              rx_eng_to_timer_clear_ptimer.write(tcp_rx_meta_reg.session_id);
              logger.Info(
                  RX_ENGINE, PROBE_TMR, "Clear PTimer", tcp_rx_meta_reg.session_id.to_string(16));

              // Check if there is payload
              if (tcp_rx_meta_reg.header.payload_length != 0) {
#if (!TCP_RX_DDR_BYPASS)
                GetSessionMemAddr<1>(tcp_rx_meta_reg.session_id,
                                     tcp_rx_meta_reg.header.seq_number,
                                     payload_mem_addr);
                to_mem_write_cmd =
                    MemBufferRWCmd(payload_mem_addr, tcp_rx_meta_reg.header.payload_length);
                rx_eng_to_mem_cmd.write(to_mem_write_cmd);
                logger.Info(RX_ENGINE, "WriteMem Cmd", to_mem_write_cmd.to_string());
#endif
                // Tell Application new data is available and connection got closed
                to_rx_app_notify = AppNotificationNoTDEST(tcp_rx_meta_reg.session_id,
                                                          tcp_rx_meta_reg.header.payload_length,
                                                          tcp_rx_meta_reg.src_ip,
                                                          tcp_rx_meta_reg.dst_port,
                                                          true);
                rx_eng_to_rx_app_notification.write(to_rx_app_notify);
                logger.Info(RX_ENGINE,
                            RX_APP_IF,
                            "New Data Notify& Conn Close",
                            to_rx_app_notify.to_string());
                tcp_payload_dropped_by_rx_fsm.write(false);
              } else if (session_state_reg == ESTABLISHED) {
                // Tell Application connection got closed
                to_rx_app_notify = AppNotificationNoTDEST(tcp_rx_meta_reg.session_id,
                                                          tcp_rx_meta_reg.src_ip,
                                                          tcp_rx_meta_reg.dst_port,
                                                          true);
                rx_eng_to_rx_app_notification.write(to_rx_app_notify);
                logger.Info(RX_ENGINE, RX_APP_IF, "Conn Close", to_rx_app_notify.to_string());
              }
              // update state
              if (session_state_reg == ESTABLISHED) {
                to_event_eng_event = Event(FIN, tcp_rx_meta_reg.session_id);
                to_sttable_req     = StateTableReq(tcp_rx_meta_reg.session_id, LAST_ACK, 1);

              } else {  // FIN_WAIT_1 || FIN_WAIT_2
                        // check if final FIN is ACK'd -> LAST_ACK
                if (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte) {
                  to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, TIME_WAIT, 1);
                  logger.Info(
                      RX_ENGINE, CLOSE_TMR, "Set CTimer", tcp_rx_meta_reg.session_id.to_string(16));
                  rx_eng_to_timer_set_ctimer.write(tcp_rx_meta_reg.session_id);
                } else {
                  to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, CLOSING, 1);
                }
                to_event_eng_event = Event(ACK, tcp_rx_meta_reg.session_id);
              }
              logger.Info(RX_ENGINE, STAT_TBLE, "Upd State", to_sttable_req.to_string());
              logger.Info(RX_ENGINE, EVENT_ENG, "Event", to_event_eng_event.to_string());

            } else {  // NOT (ESTABLISHED || FIN_WAIT_1 || FIN_WAIT_2)
              to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
              logger.Info(RX_ENGINE, STAT_TBLE, "Release State", to_sttable_req.to_string());
              to_event_eng_event = Event(ACK, tcp_rx_meta_reg.session_id);
              logger.Info(RX_ENGINE, EVENT_ENG, "Event", to_event_eng_event.to_string());

              // If there is payload we need to drop it
              if (tcp_rx_meta_reg.header.payload_length != 0) {
                tcp_payload_dropped_by_rx_fsm.write(true);
              }
            }
            rx_eng_to_sttable_req.write(to_sttable_req);
            rx_eng_fsm_to_event_eng_set_event.write(to_event_eng_event);
          }
          break;
        default:  // TODO MAYBE load everything all the time
          // stateTable is locked, make sure it is released in at the end
          if (fsm_state == LOAD) {
            // Handle if RST
            if (tcp_rx_meta_reg.header.rst) {
              if (session_state_reg == SYN_SENT) {  // TODO this would be a RST,ACK i think
                if (tcp_rx_meta_reg.header.ack_number ==
                    tx_sar_reg.next_byte) {  // Check if matching SYN
                  // tell application, could not open connection
                  rx_eng_to_tx_app_notification.write(
                      OpenConnRspNoTDEST(tcp_rx_meta_reg.session_id, false));
                  to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1);
                  to_rtimer_req  = RxEngToRetransTimerReq(tcp_rx_meta_reg.session_id, true);
                  rx_eng_to_timer_clear_rtimer.write(to_rtimer_req);
                  logger.Info(RX_ENGINE, RTRMT_TMR, "Set RTimer", to_rtimer_req.to_string());

                } else {
                  // Ignore since not matching
                  to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
                }
              } else {
                // Check if in window
                if (tcp_rx_meta_reg.header.seq_number == rx_sar_reg.recvd) {
                  // tell application, RST occurred, abort
                  to_rx_app_notify = AppNotificationNoTDEST(tcp_rx_meta_reg.session_id,
                                                            tcp_rx_meta_reg.src_ip,
                                                            tcp_rx_meta_reg.dst_port,
                                                            true);
                  rx_eng_to_rx_app_notification.write(to_rx_app_notify);
                  logger.Info(RX_ENGINE, RX_APP_IF, "Conn RST", to_rx_app_notify.to_string());

                  // TODO maybe some TIME_WAIT state
                  to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1);
                  to_rtimer_req  = RxEngToRetransTimerReq(tcp_rx_meta_reg.session_id, true);
                  rx_eng_to_timer_clear_rtimer.write(to_rtimer_req);
                  logger.Info(RX_ENGINE, RTRMT_TMR, "Set RTimer", to_rtimer_req.to_string());

                } else {
                  // Ingore since not matching window
                  to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
                }
              }
            } else {  // Handle non RST bogus packages

              // TODO maybe sent RST ourselves, or simply ignore
              // For now ignore, sent ACK??
              // eventsOut.write(rstEvent(mh_meta.seqNumb, 0, true));
              to_sttable_req = StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1);
            }
            logger.Info(RX_ENGINE, STAT_TBLE, "Release State/Close", to_sttable_req.to_string());

            rx_eng_to_sttable_req.write(to_sttable_req);
          }
          break;
      }
      break;
  }
}

/** @ingroup rx_engine
 *  Delays the notifications to the application until the data is actually
 * written to memory
 */
void RxEngNotificaionHandler(
    // from  datamover status
    stream<DataMoverStatus> &mover_to_rx_eng_write_status,
    stream<ap_uint<1> > &    mem_buffer_double_access_flag,
    // from rx eng fsm
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification_cache,
    // to rx app intf
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

#pragma HLS INTERFACE axis register both port = mover_to_rx_eng_write_status

  enum NotificaionHandlerFsmState { READ_NOTIFY, READ_STATUS_1, READ_STATUS_2 };
  static NotificaionHandlerFsmState fsm_state = READ_NOTIFY;

  bool                          double_access_flag = false;
  DataMoverStatus               datamover_sts;
  static AppNotificationNoTDEST app_notificaion_reg1;
  static AppNotificationNoTDEST app_notificaion_reg2;
  switch (fsm_state) {
    case READ_NOTIFY:
      if (!rx_eng_to_rx_app_notification_cache.empty()) {
        app_notificaion_reg1 = rx_eng_to_rx_app_notification_cache.read();
        logger.Info(
            RX_ENGINE, "Read App Notify in NotificaionHandler", app_notificaion_reg1.to_string());
        if (app_notificaion_reg1.length != 0) {
          fsm_state = READ_STATUS_1;
        } else {
          logger.Info(RX_ENGINE, RX_APP_IF, "To RxApp Notify", app_notificaion_reg1.to_string());
          rx_eng_to_rx_app_notification.write(app_notificaion_reg1);
        }
      }
      break;
    case READ_STATUS_1:
      if (!mem_buffer_double_access_flag.empty() && !mover_to_rx_eng_write_status.empty()) {
        double_access_flag = mem_buffer_double_access_flag.read();
        datamover_sts      = mover_to_rx_eng_write_status.read();
        logger.Info(DATA_MVER, RX_ENGINE, "WriteMemSts1", datamover_sts.to_string());

        if (datamover_sts.okay) {
          // overflow write mem twice
          if (double_access_flag == true) {
            app_notificaion_reg2 = app_notificaion_reg1;
            fsm_state            = READ_STATUS_2;
          } else {
            logger.Info(RX_ENGINE, RX_APP_IF, "To RxApp Notify", app_notificaion_reg1.to_string());
            rx_eng_to_rx_app_notification.write(app_notificaion_reg1);
            fsm_state = READ_NOTIFY;
          }
        } else {
          fsm_state = READ_NOTIFY;
        }
      }
      break;
    case READ_STATUS_2:
      if (!mover_to_rx_eng_write_status.empty()) {
        datamover_sts = mover_to_rx_eng_write_status.read();
        logger.Info(DATA_MVER, RX_ENGINE, "WriteMemSts2", datamover_sts.to_string());

        if (datamover_sts.okay) {
          logger.Info(RX_ENGINE, RX_APP_IF, "To RxApp Notify", app_notificaion_reg2.to_string());
          rx_eng_to_rx_app_notification.write(app_notificaion_reg2);
        }
        fsm_state = READ_NOTIFY;
      }
      break;
  }
}

void                 RxEngEventMerger(stream<EventWithTuple> &in1,
                                      stream<Event> &         in2,
                                      stream<EventWithTuple> &out) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  if (!in1.empty()) {
    out.write(in1.read());
  } else if (!in2.empty()) {
    out.write(in2.read());
  }
}

void rx_engine(
    // ip packet
    stream<NetAXIS> &rx_ip_pkt_in,
    // port table check open and TDEST
    stream<TcpPortNumber> &   rx_eng_to_ptable_check_req,
    stream<PtableToRxEngRsp> &ptable_to_rx_eng_check_rsp,
    // to session lookup R/W
    stream<RxEngToSlookupReq> &rx_eng_to_slookup_req,
    stream<SessionLookupRsp> & slookup_to_rx_eng_rsp,
    // FSM
    // Rx SAR R/W
    stream<RxEngToRxSarReq> &rx_eng_to_rx_sar_req,
    stream<RxSarTableEntry> &rx_sar_to_rx_eng_rsp,
    // TX SAR R/W
    stream<RxEngToTxSarReq> &rx_eng_to_tx_sar_req,
    stream<TxSarToRxEngRsp> &tx_sar_to_rx_eng_rsp,
    // state table
    stream<StateTableReq> &rx_eng_to_sttable_req,
    stream<SessionState> & sttable_to_rx_eng_rsp,
    // update timer state
    stream<TcpSessionID> &          rx_eng_to_timer_set_ctimer,
    stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
    stream<TcpSessionID> &          rx_eng_to_timer_clear_ptimer,
    // to app connection notify, when net app active open a connection
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // to app connection notify, when net app passive open a conection
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    // to app data notify
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification,

    // to event engine
    stream<EventWithTuple> &rx_eng_to_event_eng_set_event,
#if !TCP_RX_DDR_BYPASS
    // tcp payload to mem
    stream<DataMoverCmd> &   rx_eng_to_mover_write_cmd,
    stream<NetAXIS> &        rx_eng_to_mover_write_data,
    stream<DataMoverStatus> &mover_to_rx_eng_write_status
#else
    // tcp payload to rx app
    stream<NetAXISWord> &rx_eng_to_rx_app_data
#endif
) {
#pragma HLS DATAFLOW
#pragma HLS INLINE

//#pragma HLS INTERFACE ap_ctrl_none       port = return
#pragma HLS INTERFACE axis register both port = rx_ip_pkt_in
#if !TCP_RX_DDR_BYPASS
#pragma HLS INTERFACE axis register both port = rx_eng_to_mover_write_cmd
#pragma HLS INTERFACE axis register both port = rx_eng_to_mover_write_data
#pragma HLS INTERFACE axis register both port = mover_to_rx_eng_write_status
#endif

  static stream<NetAXISWord> tcp_pseudo_packet_for_checksum_fifo(
      "tcp_pseudo_packet_for_checksum_fifo");
#pragma HLS STREAM variable = tcp_pseudo_packet_for_checksum_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_packet_for_checksum_fifo compact = bit

  static stream<NetAXISWord> tcp_pseudo_packet_for_rx_eng_fifo("tcp_pseudo_packet_for_rx_eng_fifo");
#pragma HLS STREAM variable = tcp_pseudo_packet_for_rx_eng_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_packet_for_rx_eng_fifo compact = bit

  static stream<SubChecksum> tcp_pseudo_packet_subchecksum_fifo(
      "tcp_pseudo_packet_subchecksum_fifo");
#pragma HLS STREAM variable = tcp_pseudo_packet_subchecksum_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_packet_subchecksum_fifo compact = bit

  static stream<ap_uint<16> > tcp_pseudo_packet_checksum_fifo("tcp_pseudo_packet_checksum_fifo");
#pragma HLS STREAM variable = tcp_pseudo_packet_checksum_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_packet_checksum_fifo compact = bit

  // tcp option are raw in tcp header
  static stream<TcpPseudoHeaderMeta> tcp_pseudo_header_meta_fifo("tcp_pseudo_header_meta_fifo");
#pragma HLS STREAM variable = tcp_pseudo_header_meta_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_header_meta_fifo compact = bit
  // tcp option are parsed into meta header
  static stream<TcpPseudoHeaderMeta> tcp_pseudo_header_meta_parsed_fifo(
      "tcp_pseudo_header_meta_parsed_fifo");
#pragma HLS STREAM variable = tcp_pseudo_header_meta_parsed_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_header_meta_parsed_fifo compact = bit
  // tcp option are parsed into meta header and checked by checksum
  static stream<TcpPseudoHeaderMeta> tcp_pseudo_header_meta_valid_fifo(
      "tcp_pseudo_header_meta_valid_fifo");
#pragma HLS STREAM variable = tcp_pseudo_header_meta_valid_fifo depth = 16
#pragma HLS aggregate variable = tcp_pseudo_header_meta_valid_fifo compact = bit

  static stream<RxEngFsmMetaData> rx_eng_fsm_meta_data_fifo("rx_eng_fsm_meta_data_fifo");
#pragma HLS STREAM variable = rx_eng_fsm_meta_data_fifo depth = 16
#pragma HLS aggregate variable = rx_eng_fsm_meta_data_fifo compact = bit

  static stream<NetAXISWord> tcp_payload_fifo("tcp_payload_fifo");
#pragma HLS STREAM variable = tcp_payload_fifo depth = 512
#pragma HLS aggregate variable = tcp_payload_fifo compact = bit

#if (!TCP_RX_DDR_BYPASS)
  static stream<NetAXISWord> rx_eng_to_mem_write_data_fifo("rx_eng_to_mem_write_data_fifo");
#pragma HLS STREAM variable = rx_eng_to_mem_write_data_fifo depth = 512
#pragma HLS aggregate variable = rx_eng_to_mem_write_data_fifo compact = bit

  static stream<MemBufferRWCmd> rx_eng_to_mem_write_cmd_fifo("rx_eng_to_mem_write_cmd_fifo");
#pragma HLS STREAM variable = rx_eng_to_mem_write_cmd_fifo depth = 16
#pragma HLS aggregate variable = rx_eng_to_mem_write_cmd_fifo compact = bit

  static stream<ap_uint<1> > mem_buffer_double_access_flag_fifo(
      "mem_buffer_double_access_flag_fifo");
#pragma HLS STREAM variable = mem_buffer_double_access_flag_fifo depth = 16

  static stream<AppNotificationNoTDEST> rx_eng_to_rx_app_notification_cache_fifo(
      "rx_eng_to_rx_app_notification_cache_fifo");
#pragma HLS STREAM variable = rx_eng_to_rx_app_notification_cache_fifo depth = 256
#pragma HLS aggregate variable = rx_eng_to_rx_app_notification_cache_fifo compact = bit

#endif
  // payload drop fifo
  static stream<bool> payload_dropped_by_checksum_fifo("payload_dropped_by_checksum_fifo");
#pragma HLS STREAM variable = payload_dropped_by_checksum_fifo depth = 16
#pragma HLS aggregate variable = payload_dropped_by_checksum_fifo compact = bit

  static stream<bool> payload_dropped_by_port_session_fifo("payload_dropped_by_port_session_fifo");
#pragma HLS STREAM variable = payload_dropped_by_port_session_fifo depth = 16
#pragma HLS aggregate variable = payload_dropped_by_port_session_fifo compact = bit

  static stream<bool> payload_dropped_by_rx_fsm_fifo("payload_dropped_by_rx_fsm_fifo");
#pragma HLS STREAM variable = payload_dropped_by_rx_fsm_fifo depth = 16
#pragma HLS aggregate variable = payload_dropped_by_rx_fsm_fifo compact = bit
  // payload corresponding TDSET fifo, write by port table , read by RxEngTcpPayloadDropper
  static stream<NetAXISDest> tcp_payload_tdest_fifo("tcp_payload_tdest_fifo");
#pragma HLS STREAM variable = tcp_payload_tdest_fifo depth = 16
#pragma HLS aggregate variable = tcp_payload_tdest_fifo compact = bit

  // event
  static stream<EventWithTuple> rx_eng_meta_set_event_fifo("rx_eng_meta_set_event_fifo");
#pragma HLS STREAM variable = rx_eng_meta_set_event_fifo depth = 8
#pragma HLS aggregate variable = rx_eng_meta_set_event_fifo compact = bit

  static stream<Event> rx_eng_fsm_set_event_fifo("rx_eng_fsm_set_event_fifo");
#pragma HLS STREAM variable = rx_eng_fsm_set_event_fifo depth = 8
#pragma HLS aggregate variable = rx_eng_fsm_set_event_fifo compact = bit

  RxEngTcpPseudoHeaderInsert(
      rx_ip_pkt_in, tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_for_rx_eng_fifo);

  ComputeSubChecksum<1>(tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_subchecksum_fifo);

  CheckChecksum(tcp_pseudo_packet_subchecksum_fifo, tcp_pseudo_packet_checksum_fifo);

  RxEngParseTcpHeader(
      tcp_pseudo_packet_for_rx_eng_fifo, tcp_pseudo_header_meta_fifo, tcp_payload_fifo);

  RxEngParseTcpHeaderOptions(tcp_pseudo_header_meta_fifo, tcp_pseudo_header_meta_parsed_fifo);

  RxEngVerifyChecksumAndPort(tcp_pseudo_packet_checksum_fifo,
                             tcp_pseudo_header_meta_parsed_fifo,
                             payload_dropped_by_checksum_fifo,
                             tcp_pseudo_header_meta_valid_fifo,
                             rx_eng_to_ptable_check_req);

  RxEngTcpMetaHandler(tcp_pseudo_header_meta_valid_fifo,
                      ptable_to_rx_eng_check_rsp,
                      rx_eng_to_slookup_req,
                      slookup_to_rx_eng_rsp,
                      rx_eng_fsm_meta_data_fifo,
                      payload_dropped_by_port_session_fifo,
                      tcp_payload_tdest_fifo,
                      rx_eng_meta_set_event_fifo);

  RxEngTcpPayloadDropper(tcp_payload_fifo,
                         payload_dropped_by_checksum_fifo,
                         payload_dropped_by_port_session_fifo,
                         tcp_payload_tdest_fifo,
                         payload_dropped_by_rx_fsm_fifo,
#if (!TCP_RX_DDR_BYPASS)
                         rx_eng_to_mem_write_data_fifo
#else
                         rx_eng_to_rx_app_data
#endif
  );
  RxEngTcpFsm(rx_eng_fsm_meta_data_fifo,
              rx_eng_to_rx_sar_req,
              rx_sar_to_rx_eng_rsp,
              rx_eng_to_tx_sar_req,
              tx_sar_to_rx_eng_rsp,
              rx_eng_to_sttable_req,
              sttable_to_rx_eng_rsp,
              rx_eng_to_timer_set_ctimer,
              rx_eng_to_timer_clear_rtimer,
              rx_eng_to_timer_clear_ptimer,
              rx_eng_fsm_set_event_fifo,
              rx_eng_to_tx_app_notification,
              rx_eng_to_tx_app_new_client_notification,
#if (!TCP_RX_DDR_BYPASS)
              // if tcp rx ddr bypass is disabled, rx app notificaion should be delayed
              rx_eng_to_rx_app_notification_cache_fifo,
              rx_eng_to_mem_write_cmd_fifo,
#else
              // if tcp rx ddr bypass, rx app notifications should be pushed directly to the
              // rx_app_intf
              rx_eng_to_rx_app_notification,
#endif
              payload_dropped_by_rx_fsm_fifo);

#if (!TCP_RX_DDR_BYPASS)
  WriteDataToMem<1>(rx_eng_to_mem_write_cmd_fifo,
                    rx_eng_to_mem_write_data_fifo,
                    rx_eng_to_mover_write_cmd,
                    rx_eng_to_mover_write_data,
                    mem_buffer_double_access_flag_fifo);

  RxEngNotificaionHandler(mover_to_rx_eng_write_status,
                          mem_buffer_double_access_flag_fifo,
                          rx_eng_to_rx_app_notification_cache_fifo,
                          rx_eng_to_rx_app_notification);
#endif

  RxEngEventMerger(
      rx_eng_meta_set_event_fifo, rx_eng_fsm_set_event_fifo, rx_eng_to_event_eng_set_event);
}