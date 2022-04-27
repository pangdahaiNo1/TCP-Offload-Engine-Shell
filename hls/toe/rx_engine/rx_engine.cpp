#include "rx_engine.hpp"

#include "toe/toe_utils.hpp"

void                 RxEngTcpPseudoHeaderInsert(stream<NetAXIS> &ip_packet,
                                                stream<NetAXIS> &tcp_pseudo_packet_for_checksum,
                                                stream<NetAXIS> &tcp_pseudo_packet_for_rx_eng) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  NetAXIS cur_word;
  NetAXIS send_word;
  // tcp header and payload length
  ap_uint<16>        tcp_segment_length;
  static NetAXIS     prev_word;
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
      if (!ip_packet.empty()) {
        ip_packet.read(cur_word);
        ip_header_length = cur_word.data(3, 0);                  // Read IP header len
        ip_packet_length = SwapByte<16>(cur_word.data(31, 16));  // Read IP total len
        src_ip_addr      = cur_word.data(127, 96);
        dest_ip_addr     = cur_word.data(159, 128);

        keep_extra = 8 + (ip_header_length - 5) * 4;
        if (cur_word.last) {
          RemoveIpHeader(NetAXIS(), cur_word, ip_header_length, send_word);

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
      if (!ip_packet.empty()) {
        ip_packet.read(cur_word);
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
 *  @param[in]		pseudoPacket
 *  @param[out]		payload
 *  @param[out]		metaDataFifoOut
 */
void                 RxEngParseTcpHeader(stream<NetAXIS> &            tcp_pseudo_packet,
                                         stream<TcpPseudoHeaderMeta> &tcp_meta_data,
                                         stream<NetAXIS> &            tcp_payload) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum ParseTcpHeaderState { FIRST_WORD, REMAINING_WORDS, EXTRA_WORD };
  static ParseTcpHeaderState parse_fsm_state = FIRST_WORD;

  static NetAXIS prev_word;
  NetAXIS        cur_word;
  NetAXIS        send_word;

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

        tcp_seg_meta.src_ip             = cur_word.data(31, 0);
        tcp_seg_meta.dest_ip            = cur_word.data(63, 32);
        tcp_seg_meta.header.src_port    = cur_word.data(111, 96);
        tcp_seg_meta.header.dest_port   = cur_word.data(127, 112);
        tcp_seg_meta.header.data_offset = cur_word.data(199, 196);
        tcp_seg_meta.header.seq_number  = SwapByte<32>(cur_word.data(159, 128));
        tcp_seg_meta.header.ack_number  = SwapByte<32>(cur_word.data(191, 160));
        tcp_seg_meta.header.win_size    = SwapByte<16>(cur_word.data(223, 208));
        tcp_seg_meta.header.length      = tcp_payload_length;
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
