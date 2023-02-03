#include "icmp_server.hpp"

ap_uint<16> ComputeChechkSum20B(ap_uint<160> ip_header) {
#pragma HLS INLINE
  ap_uint<16> checksumL0[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  ap_uint<17> checksumL1[5]  = {0, 0, 0, 0, 0};
  ap_uint<18> checksumL2[2]  = {0, 0};
  ap_uint<20> checksumL3;
  ap_uint<17> checksumL4_r;
  ap_uint<17> checksumL4_o;
  ap_uint<16> checksum;

assign2array:
  for (int m = 0; m < 10; m++) {
#pragma HLS UNROLL
    checksumL0[m] = ip_header((m * 16) + 15, m * 16);
  }

sumElementsL1:
  for (int m = 0; m < 10; m = m + 2) {
#pragma HLS UNROLL
    checksumL1[m / 2] = checksumL0[m] + checksumL0[m + 1];
  }

  checksumL2[0] = checksumL1[0] + checksumL1[1];
  checksumL2[1] = checksumL1[2] + checksumL1[3];
  checksumL3    = checksumL2[0] + checksumL2[1] + checksumL1[4];

  checksumL4_r = checksumL3(15, 0) + checksumL3(19, 16);
  checksumL4_o = checksumL3(15, 0) + checksumL3(19, 16) + 1;

  if (checksumL4_r.bit(16))
    checksum = ~checksumL4_o(15, 0);
  else
    checksum = ~checksumL4_r(15, 0);

  return checksum;
}

// Echo or Echo Reply Message
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |     Type      |     Code      |          Checksum             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |           Identifier          |        Sequence Number        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |     Data ...
//   +-+-+-+-+-
//
//   Type
//      8 for echo message;
//      0 for echo reply message.
//
//   Code
//
//      0
//
//   Checksum
//      The checksum is the 16-bit ones's complement of the one's
//      complement sum of the ICMP message starting with the ICMP Type.
//      For computing the checksum , the checksum field should be zero.
//      If the total length is odd, the received data is padded with one
//      octet of zeros for computing the checksum.
//
//   Identifier
//      If code = 0, an identifier to aid in matching echos and replies,
//      may be zero.
//
//   Sequence Number
//      If code = 0, a sequence number to aid in matching echos and
//      replies, may be zero.
// Description
//
//      The data received in the echo message must be returned in the echo
//      reply message.

/** @ingroup icmp_server
 *  Main function
 *  @param[in]      data_in
 *  @param[out]     data_out
 */
void icmp_server(stream<NetAXIS> &data_in, ap_uint<32> &my_ipaddr, stream<NetAXIS> &data_out) {
#pragma HLS pipeline II = 1

#pragma HLS INTERFACE ap_ctrl_none       port = return
#pragma HLS INTERFACE axis register both port = data_in name = s_axis_icmp
#pragma HLS INTERFACE axis register both port = data_out name                  = m_axis_icmp
#pragma HLS INTERFACE                                    ap_none register port = my_ipaddr

  enum IcmpState { READ_PACKET, EVALUATE_CONDITIONS, SEND_FIRST_WORD, FORWARD, DROP_PACKET };
  static IcmpState fsm_state = READ_PACKET;

  static NetAXIS     prev_word;
  static ap_uint<32> dst_ip;
  static ap_uint<17> icmp_checksum;
  static ap_uint<8>  icmp_type;
  static ap_uint<8>  icmp_code;

  static ap_uint<16> auxin_checksum_reg;
  ap_uint<17>        icmp_checksum_tmp;
  NetAXIS            cur_word;
  ap_uint<160>       aux_ip_header;

  switch (fsm_state) {
    // Read a new packet (512bits) from data_in stream
    case READ_PACKET:
      if (!data_in.empty()) {  // Packet at IP level. No IP options are taken
                               // into account
        data_in.read(cur_word);
        for (int m = 0; m < 20; m++) {  // Arrange IP header
#pragma HLS UNROLL
          aux_ip_header(159 - m * 8, 152 - m * 8) = cur_word.data((m * 8) + 7, m * 8);
        }
        dst_ip    = cur_word.data(159, 128);  // Get destination IP to be verified
        icmp_type = cur_word.data(167, 160);
        icmp_code = cur_word.data(175, 168);

        icmp_checksum      = (cur_word.data(183, 176), cur_word.data(191, 184)) + 0x0800;
        auxin_checksum_reg = ComputeChechkSum20B(aux_ip_header);

        fsm_state = EVALUATE_CONDITIONS;

        prev_word = cur_word;
      }
      break;

    case EVALUATE_CONDITIONS:
      if ((dst_ip == my_ipaddr) && (icmp_type == ECHO_REQUEST && (icmp_code == 0)) &&
          auxin_checksum_reg == 0) {
        fsm_state = SEND_FIRST_WORD;
      } else {
        if (!cur_word.last) {
          fsm_state = DROP_PACKET;
        } else {
          fsm_state = READ_PACKET;
        }
      }
      break;

    case SEND_FIRST_WORD:

      cur_word = prev_word;

      cur_word.data(71, 64)   = 128;            // IP time to live
      cur_word.data(95, 72)   = ICMP_PROTOCOL;  // IP protocol
      cur_word.data(95, 80)   = 0;              // clear IP checksum, it is inserted later
      cur_word.data(127, 96)  = prev_word.data(159, 128);
      cur_word.data(159, 128) = prev_word.data(127, 96);  // swap IPs

      for (int m = 0; m < 20; m++) {  // Arrange IP header
        aux_ip_header(159 - m * 8, 152 - m * 8) = cur_word.data((m * 8) + 7, m * 8);
      }

      icmp_checksum_tmp       = icmp_checksum(15, 0) + icmp_checksum.bit(16);
      cur_word.data(167, 160) = ECHO_REPLY;
      cur_word.data(191, 176) =
          (icmp_checksum_tmp(7, 0), icmp_checksum_tmp(15, 8));  // Insert ICMP checksum

      if (prev_word.last)
        fsm_state = READ_PACKET;
      else
        fsm_state = FORWARD;

      data_out.write(cur_word);
      break;
    case DROP_PACKET:
      if (!data_in.empty()) {
        data_in.read(cur_word);

        if (cur_word.last)
          fsm_state = READ_PACKET;
      }
      break;
    case FORWARD:
      if (!data_in.empty()) {
        data_in.read(cur_word);
        data_out.write(cur_word);

        if (cur_word.last)
          fsm_state = READ_PACKET;
      }
      break;
  }
}
