#include "rx_engine.hpp"

#include "toe/toe_utils.hpp"

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

  static ap_uint<6> tcp_payload_offset_bytes;  // data_offset * 4 + 12
  ap_uint<4>        tcp_payload_offset_words;  // data_offset in TCP header
  ap_uint<16>       tcp_payload_length;        // in bytes

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
        if (cur_word.last) {
          if (tcp_payload_length != 0) {  // one-transaction packet with any data
            send_word.data = cur_word.data(511, tcp_payload_offset_bytes * 8);
            send_word.keep = cur_word.keep(63, tcp_payload_offset_bytes);
            send_word.last = 1;
            tcp_payload.write(send_word);
          }
        } else {
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
                                                stream<bool> &               tcp_payload_should_be_dropped,
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
        tcp_seg_is_valid = (tcp_seg_checksum == 0);
        if (tcp_seg_is_valid) {
          tcp_meta_data_out.write(tcp_meta_reg);
          rx_eng_to_ptable_check_req.write(SwapByte<16>(tcp_meta_reg.header.dest_port));
        }
        if (tcp_meta_reg.header.payload_length) {
          tcp_payload_should_be_dropped.write(!tcp_seg_is_valid);
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
                                         stream<bool> &               tcp_payload_should_be_dropped,
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
        // if port is closed
        if (!ptable_rsp_reg.is_open) {
          // when port is closed, send rst
          if (!tcp_meta_reg.header.rst) {
            four_tuple.src_ip_addr  = tcp_meta_reg.dest_ip;
            four_tuple.dst_ip_addr  = tcp_meta_reg.src_ip;
            four_tuple.src_tcp_port = tcp_meta_reg.header.dest_port;
            four_tuple.dst_tcp_port = tcp_meta_reg.header.src_port;
            rx_eng_meta_to_event_eng_set_event.write(EventWithTuple(
                RstEvent(tcp_meta_reg.header.seq_number + tcp_meta_reg.header.get_payload_length()),
                four_tuple));
          }
          if (tcp_meta_reg.header.payload_length != 0) {
            // Drop payload because port is closed
            tcp_payload_should_be_dropped.write(true);
            tcp_payload_tdest.write(ptable_rsp_reg.role_id);
          }
        } else {  // port is open
          // Make session lookup, only allow creation of new entry when SYN or SYN_ACK
          rx_eng_to_slookup_req.write(RxEngToSlookupReq(
              FourTuple(tcp_meta_reg.src_ip,
                        tcp_meta_reg.dest_ip,
                        SwapByte<32>(tcp_meta_reg.header.src_port),
                        SwapByte<32>(tcp_meta_reg.header.dest_port)),
              (tcp_meta_reg.header.syn && !tcp_meta_reg.header.rst && !tcp_meta_reg.header.fin),
              ptable_rsp_reg.role_id));
          fsm_state = LOOKUP;
        }
      }
      break;
    case LOOKUP:  // wait for slookup response
      if (!slookup_to_rx_eng_rsp.empty()) {
        slookup_to_rx_eng_rsp.read(slookup_rsp_reg);
        if (slookup_rsp_reg.hit) {
          rx_eng_fsm_meta_data_out.write(RxEngFsmMetaData(slookup_rsp_reg.session_id,
                                                          tcp_meta_reg.src_ip,
                                                          tcp_meta_reg.header.dest_port,
                                                          tcp_meta_reg.header,
                                                          ptable_rsp_reg.role_id));
        }
        if (tcp_meta_reg.header.payload_length != 0) {
          tcp_payload_should_be_dropped.write(!slookup_rsp_reg.hit);
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
void                 RxEngTcpPayloadDropper(stream<NetAXISWord> &tcp_payload_in,
                                            stream<bool> &       payload_should_dropped_by_checksum,
                                            stream<bool> &       payload_should_dropped_by_port_or_session,
                                            stream<NetAXISDest> &tcp_payload_tdest,
                                            stream<bool> &       payload_should_dropped_by_rx_fsm,
                                            stream<NetAXISWord> &tcp_payload_out) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum RxDropperFsmState { RD_VERIFY_CHECKSUM, RD_META_HANDLER, RD_FSM_DROP, FWD, DROP };
  static RxDropperFsmState fsm_state = RD_VERIFY_CHECKSUM;
  static NetAXISDest       tcp_payload_role_id;
  bool                     drop;
  NetAXISWord              cur_word;

  switch (fsm_state) {
    case RD_VERIFY_CHECKSUM:
      if (!payload_should_dropped_by_checksum.empty()) {
        payload_should_dropped_by_checksum.read(drop);
        fsm_state = (drop) ? DROP : RD_META_HANDLER;
      }
      break;
    case RD_META_HANDLER:
      if (!payload_should_dropped_by_port_or_session.empty() && !tcp_payload_tdest.empty()) {
        payload_should_dropped_by_port_or_session.read(drop);
        tcp_payload_tdest.read(tcp_payload_role_id);
        fsm_state = (drop) ? DROP : RD_FSM_DROP;
      }
      break;
    case RD_FSM_DROP:
      if (!payload_should_dropped_by_rx_fsm.empty()) {
        payload_should_dropped_by_rx_fsm.read(drop);
        fsm_state = (drop) ? DROP : FWD;
      }
      break;
    case FWD:
      if (!tcp_payload_in.empty() && !tcp_payload_tdest.empty()) {
        tcp_payload_in.read(cur_word);
        cur_word.dest = tcp_payload_role_id;
        tcp_payload_out.write(cur_word);

        fsm_state = (cur_word.last) ? RD_VERIFY_CHECKSUM : FWD;
      }
      break;
    case DROP:
      if (!tcp_payload_in.empty()) {
        tcp_payload_in.read(cur_word);
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
    stream<DataMoverCmd> &rx_buffer_write_cmd,
#endif
    // to app payload should be dropped?
    stream<bool> &payload_should_dropped

) {
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

  ap_uint<32> payload_mem_addr = 0;
  ap_uint<4>  rx_win_scale;  // used to computed the scale option for RX buffer
  ap_uint<4>  tx_win_scale;  // used to computed the scale option for TX buffer

  switch (fsm_state) {
    case LOAD:
      if (rx_eng_fsm_meta_data_in.empty()) {
        rx_eng_fsm_meta_data_in.read(tcp_rx_meta_reg);

        // query request for current session state
        rx_eng_to_sttable_req.write(StateTableReq(tcp_rx_meta_reg.session_id));
        // query requset for current session RX SAR table
        rx_eng_to_rx_sar_req.write(RxEngToRxSarReq(tcp_rx_meta_reg.session_id));

        if (tcp_rx_meta_reg.header.ack) {
          rx_eng_to_tx_sar_req.write(RxEngToTxSarReq(tcp_rx_meta_reg.session_id));
          rx_eng_sent_tx_sar_req = true;
        }
        rx_eng_ctrl_bits[0] = tcp_rx_meta_reg.header.ack;
        rx_eng_ctrl_bits[1] = tcp_rx_meta_reg.header.syn;
        rx_eng_ctrl_bits[2] = tcp_rx_meta_reg.header.fin;
        rx_eng_ctrl_bits[3] = tcp_rx_meta_reg.header.rst;

        fsm_state = TRANSITION;
      }
      break;
    case TRANSITION:
      if (!sttable_to_rx_eng_rsp.empty() && !rx_sar_to_rx_eng_rsp.empty() &&
          !(rx_eng_sent_tx_sar_req && tx_sar_to_rx_eng_rsp.empty())) {
        sttable_to_rx_eng_rsp.read(session_state_reg);
        rx_sar_to_rx_eng_rsp.read(rx_sar_reg);
        if (rx_eng_sent_tx_sar_req) {
          tx_sar_to_rx_eng_rsp.read(tx_sar_reg);
        }
        rx_eng_sent_tx_sar_req = false;

        fsm_state = LOAD;
      }
      switch (rx_eng_ctrl_bits) {
        case 1:  // ACK
          if (fsm_state == LOAD) {
            // cout << " ACK \n" << tcp_rx_meta_reg.to_string();
            // if ack number is expected, stop the retransmit timer
            rx_eng_to_timer_clear_rtimer.write(RxEngToRetransTimerReq(
                tcp_rx_meta_reg.session_id,
                (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte)));
            if (session_state_reg == ESTABLISHED || session_state_reg == SYN_RECEIVED ||
                session_state_reg == FIN_WAIT_1 || session_state_reg == CLOSING ||
                session_state_reg == LAST_ACK) {
              // if old ACK arrived
              if (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.perv_ack &&
                  tx_sar_reg.perv_ack != tx_sar_reg.next_byte) {
                // Old ACK increase counter only if it does not contain data
                if (tcp_rx_meta_reg.header.payload_length == 0) {
                  tx_sar_reg.retrans_count++;
                }
              } else {  // new ACK arrived
                // clear probe timer for new ACK
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
                RxEngToTxSarReq tx_sar_upd(
                    tcp_rx_meta_reg.session_id,
                    tcp_rx_meta_reg.header.ack_number,
                    tcp_rx_meta_reg.header.win_size,
                    tx_sar_reg.cong_window,
                    tx_sar_reg.retrans_count,
                    ((tx_sar_reg.retrans_count == 3) || tx_sar_reg.fast_retrans),
                    tx_sar_reg.win_shift);
                rx_eng_to_tx_sar_req.write(tx_sar_upd);
                // RX SAR update, when new payload arrived
                if (tcp_rx_meta_reg.header.payload_length != 0) {
                  ap_uint<32> new_recvd =
                      tcp_rx_meta_reg.header.seq_number + tcp_rx_meta_reg.header.payload_length;
                  TcpSessionBuffer free_space =
                      ((rx_sar_reg.app_read - rx_sar_reg.recvd(WINDOW_BITS - 1, 0)) - 1);
                  // check if segment is in order and if enough free space is available
                  if ((tcp_rx_meta_reg.header.seq_number == rx_sar_reg.recvd) &&
                      (free_space > tcp_rx_meta_reg.header.payload_length)) {
                    rx_eng_to_rx_sar_req.write(
                        RxEngToRxSarReq(tcp_rx_meta_reg.session_id, new_recvd, 1));
#if (!TCP_RX_DDR_BYPASS)
                    payload_mem_addr(31, 30)          = 0x0;
                    payload_mem_addr(30, WINDOW_BITS) = tcp_rx_meta_reg.session_id(13, 0);
                    payload_mem_addr(WINDOW_BITS - 1, 0) =
                        tcp_rx_meta_reg.header.seq_number(WINDOW_BITS - 1, 0);
                    rx_buffer_write_cmd.write(
                        DataMoverCmd(payload_mem_addr, tcp_rx_meta_reg.header.payload_length));
#endif
                    // Only notify when new data available
                    rx_eng_to_rx_app_notification.write(
                        AppNotificationNoTDEST(tcp_rx_meta_reg.session_id,
                                               tcp_rx_meta_reg.header.payload_length,
                                               tcp_rx_meta_reg.src_ip,
                                               tcp_rx_meta_reg.dst_port));
                    payload_should_dropped.write(false);
                  }
                } else {
                  payload_should_dropped.write(true);
                }
                // retransmit or ack
                if ((tx_sar_reg.retrans_count == 3) && !tx_sar_reg.fast_retrans) {
                  rx_eng_fsm_to_event_eng_set_event.write(Event(RT, tcp_rx_meta_reg.session_id));
                } else {
                  rx_eng_fsm_to_event_eng_set_event.write(Event(ACK, tcp_rx_meta_reg.session_id));
                }

                if (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte) {
                  // This is necessary to unlock stateTable
                  // TODO MAYBE REARRANGE
                  switch (session_state_reg) {
                    case SYN_RECEIVED:
                      rx_eng_to_sttable_req.write(
                          StateTableReq(tcp_rx_meta_reg.session_id, ESTABLISHED, 1));
                      // Notify the tx App about new client
                      // TODO 1460 is the default value, but it could change if options
                      // are presented in the TCP header
                      rx_eng_to_tx_app_new_client_notification.write(NewClientNotificationNoTDEST(
                          tcp_rx_meta_reg.session_id, 0, 1460, TCP_NODELAY, true));
                      break;
                    case CLOSING:
                      rx_eng_to_sttable_req.write(
                          StateTableReq(tcp_rx_meta_reg.session_id, TIME_WAIT, 1));
                      rx_eng_to_timer_set_ctimer.write(tcp_rx_meta_reg.session_id);
                      break;
                    case LAST_ACK:
                      rx_eng_to_sttable_req.write(
                          StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1));
                      break;
                    default:
                      rx_eng_to_sttable_req.write(
                          StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
                      break;
                  }
                } else {  // we have to release the lock
                  rx_eng_to_sttable_req.write(StateTableReq(
                      tcp_rx_meta_reg.session_id, session_state_reg, 1));  // or ESTABLISHED
                }
              }
            } else {  // state = CLOSED || SYN_SENT || CLOSE_WAIT || FIN_WAIT2 || TIME_WAIT
              // SENT RST, RFC 793: fig.11
              rx_eng_fsm_to_event_eng_set_event.write(RstEvent(
                  tcp_rx_meta_reg.session_id,
                  tcp_rx_meta_reg.header.seq_number + tcp_rx_meta_reg.header.payload_length));
              // if data is in the pipe it needs to be dropped
              if (tcp_rx_meta_reg.header.payload_length != 0) {
                payload_should_dropped.write(true);
              }
              rx_eng_to_sttable_req.write(
                  StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
            }
          }
          break;
        case 2:  // SYN
          if (fsm_state == LOAD) {
            // check state is LISTEN || SYN_SENT
            if (session_state_reg == CLOSED || session_state_reg == SYN_SENT) {
              // cout << " SYN \n" << tcp_rx_meta_reg.to_string();
              //  If the other side announces a WSopt we use WINDOW_SCALE_BITS
              rx_win_scale = (tcp_rx_meta_reg.header.win_scale == 0) ? 0 : WINDOW_SCALE_BITS;
              tx_win_scale = tcp_rx_meta_reg.header.win_scale;
              rx_eng_to_rx_sar_req.write(RxEngToRxSarReq(tcp_rx_meta_reg.session_id,
                                                         tcp_rx_meta_reg.header.seq_number + 1,
                                                         1,
                                                         1,
                                                         rx_win_scale));
              rx_eng_to_tx_sar_req.write(RxEngToTxSarReq(tcp_rx_meta_reg.session_id,
                                                         0,
                                                         tcp_rx_meta_reg.header.win_size,
                                                         tx_sar_reg.cong_window,
                                                         0,
                                                         0,
                                                         rx_win_scale));
              rx_eng_fsm_to_event_eng_set_event.write(Event(SYN_ACK, tcp_rx_meta_reg.session_id));
              rx_eng_to_sttable_req.write(
                  StateTableReq(tcp_rx_meta_reg.session_id, SYN_RECEIVED, 1));
            } else if (session_state_reg == SYN_RECEIVED) {
              // If it is the same SYN, we resent SYN-ACK, almost like quick RT, we
              // could also wait for RT timer
              if (tcp_rx_meta_reg.header.seq_number + 1 == rx_sar_reg.recvd) {
                // Retransmit SYN_ACK
                rx_eng_fsm_to_event_eng_set_event.write(
                    Event(SYN_ACK, tcp_rx_meta_reg.session_id, 1));
                rx_eng_to_sttable_req.write(
                    StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
              } else {  // Sent RST, RFC 793: fig.9 (old) duplicate SYN(+ACK)
                rx_eng_fsm_to_event_eng_set_event.write(
                    RstEvent(tcp_rx_meta_reg.session_id,
                             tcp_rx_meta_reg.header.seq_number + 1));  // length == 0
                rx_eng_to_sttable_req.write(StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1));
              }
            } else {  // Any synchronized state
              // Unexpected SYN arrived, reply with normal ACK, RFC 793: fig.10
              rx_eng_fsm_to_event_eng_set_event.write(
                  Event(ACK_NODELAY, tcp_rx_meta_reg.session_id));
              rx_eng_to_sttable_req.write(
                  StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
            }
          }

          break;
        case 3:  // SYN + ACK
          if (fsm_state == LOAD) {
            // Clear SYN retransmission time  if ack number is correct
            rx_eng_to_timer_clear_rtimer.write(RxEngToRetransTimerReq(
                tcp_rx_meta_reg.session_id,
                (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte)));
            // A SYN was already send, ack number has to be check,  if is correct send ACK is not
            // send a RST
            if (session_state_reg == SYN_SENT) {
              // check ack number is correct
              if (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte) {
                rx_eng_fsm_to_event_eng_set_event.write(
                    Event(ACK_NODELAY, tcp_rx_meta_reg.session_id));
                // Update TCP FSM to ESTABLISHED now data can be
                // transfer initialize rx_sar, SEQ + phantom byte, last
                // '1' for appd init + Window scale if enable
                rx_eng_to_sttable_req.write(
                    StateTableReq(tcp_rx_meta_reg.session_id, ESTABLISHED, 1));

                rx_win_scale = (tcp_rx_meta_reg.header.win_scale == 0) ? 0 : WINDOW_SCALE_BITS;
                tx_win_scale = tcp_rx_meta_reg.header.win_scale;

                rx_eng_to_rx_sar_req.write(RxEngToRxSarReq(tcp_rx_meta_reg.session_id,
                                                           tcp_rx_meta_reg.header.seq_number + 1,
                                                           1,
                                                           1,
                                                           rx_win_scale));
                // TX Sar table is initialized with the received window scale
                rx_eng_to_tx_sar_req.write(RxEngToTxSarReq(tcp_rx_meta_reg.session_id,
                                                           tcp_rx_meta_reg.header.ack_number,
                                                           tcp_rx_meta_reg.header.win_size,
                                                           tx_sar_reg.cong_window,
                                                           0,
                                                           false,
                                                           rx_win_scale));
                // to tx app
                rx_eng_to_tx_app_notification.write(
                    OpenConnRspNoTDEST(tcp_rx_meta_reg.session_id, true));
              } else {
                // Sent RST, RFC 793: fig.9 (old) duplicate SYN(+ACK)
                rx_eng_fsm_to_event_eng_set_event.write(RstEvent(
                    tcp_rx_meta_reg.session_id,
                    tcp_rx_meta_reg.header.seq_number + tcp_rx_meta_reg.header.payload_length + 1));
                rx_eng_to_sttable_req.write(StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1));
              }
            } else {
              // Unexpected SYN arrived, reply with normal ACK, RFC 793: fig.10
              rx_eng_fsm_to_event_eng_set_event.write(
                  Event(ACK_NODELAY, tcp_rx_meta_reg.session_id));
              rx_eng_to_sttable_req.write(
                  StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
            }
          }
          break;
        case 5:  // FIN + ACK
          if (fsm_state == LOAD) {
            rx_eng_to_timer_clear_rtimer.write(RxEngToRetransTimerReq(
                tcp_rx_meta_reg.session_id,
                (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte)));
            if ((session_state_reg == ESTABLISHED || session_state_reg == FIN_WAIT_1 ||
                 session_state_reg == FIN_WAIT_2) &&
                (rx_sar_reg.recvd == tcp_rx_meta_reg.header.seq_number)) {
              rx_eng_to_tx_sar_req.write((RxEngToTxSarReq(tcp_rx_meta_reg.session_id,
                                                          tcp_rx_meta_reg.header.ack_number,
                                                          tcp_rx_meta_reg.header.win_size,
                                                          tx_sar_reg.cong_window,
                                                          tx_sar_reg.retrans_count,
                                                          tx_sar_reg.fast_retrans,
                                                          tx_win_scale)));
              // +1 for phantom byte, there might be data too
              rx_eng_to_rx_sar_req.write(RxEngToRxSarReq(
                  tcp_rx_meta_reg.session_id,
                  tcp_rx_meta_reg.header.seq_number + tcp_rx_meta_reg.header.payload_length + 1,
                  1));

              // Clear the probe timer
              rx_eng_to_timer_clear_ptimer.write(tcp_rx_meta_reg.session_id);

              // Check if there is payload
              if (tcp_rx_meta_reg.header.payload_length != 0) {
                payload_mem_addr(31, 30)          = 0x0;
                payload_mem_addr(30, WINDOW_BITS) = tcp_rx_meta_reg.session_id(13, 0);
                payload_mem_addr(WINDOW_BITS - 1, 0) =
                    tcp_rx_meta_reg.header.seq_number(WINDOW_BITS - 1, 0);
#if (!TCP_RX_DDR_BYPASS)
                rx_buffer_write_cmd.write(
                    DataMoverCmd(payload_mem_addr, tcp_rx_meta_reg.header.payload_length));
#endif
                // Tell Application new data is available and connection got closed
                rx_eng_to_rx_app_notification.write(
                    AppNotificationNoTDEST(tcp_rx_meta_reg.session_id,
                                           tcp_rx_meta_reg.header.payload_length,
                                           tcp_rx_meta_reg.src_ip,
                                           tcp_rx_meta_reg.dst_port,
                                           true));
                payload_should_dropped.write(false);
              } else if (session_state_reg == ESTABLISHED) {
                // Tell Application connection got closed
                rx_eng_to_rx_app_notification.write(
                    AppNotificationNoTDEST(tcp_rx_meta_reg.session_id,
                                           tcp_rx_meta_reg.src_ip,
                                           tcp_rx_meta_reg.dst_port,
                                           true));
              }
              // update state
              if (session_state_reg == ESTABLISHED) {
                rx_eng_fsm_to_event_eng_set_event.write(Event(FIN, tcp_rx_meta_reg.session_id));
                rx_eng_to_sttable_req.write(StateTableReq(tcp_rx_meta_reg.session_id, LAST_ACK, 1));
              } else {  // FIN_WAIT_1 || FIN_WAIT_2
                        // check if final FIN is ACK'd -> LAST_ACK
                if (tcp_rx_meta_reg.header.ack_number == tx_sar_reg.next_byte) {
                  // std::cout << std::endl << "TCP FSM going to TIME_WAIT state "
                  // << std::endl << std::endl;
                  rx_eng_to_sttable_req.write(
                      StateTableReq(tcp_rx_meta_reg.session_id, TIME_WAIT, 1));
                  rx_eng_to_timer_set_ctimer.write(tcp_rx_meta_reg.session_id);
                } else {
                  rx_eng_to_sttable_req.write(
                      StateTableReq(tcp_rx_meta_reg.session_id, CLOSING, 1));
                }
                rx_eng_fsm_to_event_eng_set_event.write(Event(ACK, tcp_rx_meta_reg.session_id));
              }
            } else {  // NOT (ESTABLISHED || FIN_WAIT_1 || FIN_WAIT_2)
              rx_eng_fsm_to_event_eng_set_event.write(Event(ACK, tcp_rx_meta_reg.session_id));
              rx_eng_to_sttable_req.write(
                  StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
              // If there is payload we need to drop it
              if (tcp_rx_meta_reg.header.payload_length != 0) {
                payload_should_dropped.write(true);
              }
            }
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
                  rx_eng_to_sttable_req.write(StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1));
                  rx_eng_to_timer_clear_rtimer.write(
                      RxEngToRetransTimerReq(tcp_rx_meta_reg.session_id, true));
                } else {
                  // Ignore since not matching
                  rx_eng_to_sttable_req.write(
                      StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
                }
              } else {
                // Check if in window
                if (tcp_rx_meta_reg.header.seq_number == rx_sar_reg.recvd) {
                  // tell application, RST occurred, abort
                  rx_eng_to_rx_app_notification.write(
                      AppNotificationNoTDEST(tcp_rx_meta_reg.session_id,
                                             tcp_rx_meta_reg.src_ip,
                                             tcp_rx_meta_reg.dst_port,
                                             true));  // RESET
                                                      // TODO maybe some TIME_WAIT state
                  rx_eng_to_sttable_req.write(StateTableReq(tcp_rx_meta_reg.session_id, CLOSED, 1));
                  rx_eng_to_timer_clear_rtimer.write(
                      RxEngToRetransTimerReq(tcp_rx_meta_reg.session_id, true));
                } else {
                  // Ingore since not matching window
                  rx_eng_to_sttable_req.write(
                      StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
                }
              }
            } else {  // Handle non RST bogus packages

              // TODO maybe sent RST ourselves, or simply ignore
              // For now ignore, sent ACK??
              // eventsOut.write(rstEvent(mh_meta.seqNumb, 0, true));
              rx_eng_to_sttable_req.write(
                  StateTableReq(tcp_rx_meta_reg.session_id, session_state_reg, 1));
            }
          }
          break;
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
    // tcp payload to rx app
    stream<NetAXISWord> &rx_eng_to_rx_app_data

) {
#pragma HLS INTERFACE axis register both port = rx_ip_pkt_in

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

  ComputeSubChecksum(tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_subchecksum_fifo);

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
                         rx_eng_to_rx_app_data);
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
              rx_eng_to_rx_app_notification,
              payload_dropped_by_rx_fsm_fifo);

  RxEngEventMerger(
      rx_eng_meta_set_event_fifo, rx_eng_fsm_set_event_fifo, rx_eng_to_event_eng_set_event);
}