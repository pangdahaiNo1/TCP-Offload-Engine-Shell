

#include "ethernet_header_inserter.hpp"

void                 BroadcastMacRequest(stream<NetAXIS>      &ip_seg_in,
                                         stream<ap_uint<32> > &arp_table_req,
                                         stream<NetAXISWord>  &ip_header_out,
                                         stream<NetAXISWord>  &no_ip_header_out,
                                         ap_uint<32>          &subnet_mask,
                                         ap_uint<32>          &gateway_ip_addr) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum BmrFsmState { FIRST_WORD, REMAINING };
  static BmrFsmState fsm_state = FIRST_WORD;

  NetAXIS     cur_word;
  ap_uint<32> dst_ip_addr;

  switch (fsm_state) {
    case FIRST_WORD:
      if (!ip_seg_in.empty()) {                 // There are data in the input stream
        ip_seg_in.read(cur_word);               // Reading input data
        dst_ip_addr = cur_word.data(159, 128);  // getting the IP address

        if ((dst_ip_addr & subnet_mask) ==
            (gateway_ip_addr & subnet_mask))  // Check if the destination address is in the
                                              // server subnetwork and asks for dst_ip_addr MAC
                                              // if not asks for default gateway MAC address
          arp_table_req.write(dst_ip_addr);
        else
          arp_table_req.write(gateway_ip_addr);

        ip_header_out.write(cur_word);  // Writing out first transaction
        if (!cur_word.last)
          fsm_state = REMAINING;
      }
      break;
    case REMAINING:
      if (!ip_seg_in.empty()) {  // There are data in the input stream
        ip_seg_in.read(cur_word);
        no_ip_header_out.write(cur_word);  // Writing out rest of transactions
        if (cur_word.last)
          fsm_state = FIRST_WORD;
      }
      break;
  }
}

void                 EthernetHandleOutput(stream<ArpTableRsp> &arp_table_rsp,
                                          stream<NetAXISWord> &ip_header_with_checksum,
                                          stream<NetAXISWord> &no_ip_header_out,
                                          ap_uint<48>         &my_mac_addr,
                                          stream<NetAXIS>     &eth_frame_out) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  enum mwState {
    WAIT_LOOKUP,
    DROP_IP,
    DROP_NO_IP,
    WRITE_FIRST_TRANSACTION,
    WRITE_REMAINING,
    WRITE_EXTRA_LAST_WORD
  };

  static mwState          mw_state = WAIT_LOOKUP;
  static NetAXISMacHeader previous_word;

  NetAXISWord sendWord;
  NetAXISWord current_ip_checksum;
  NetAXISWord current_no_ip;
  ArpTableRsp reply;

  switch (mw_state) {
    case WAIT_LOOKUP:
      if (!arp_table_rsp.empty()) {  // A valid response has been arrived
        arp_table_rsp.read(reply);

        if (reply.hit) {  // The MAC address related to IP destination address
                          // has been found
          previous_word.data(47, 0)   = reply.mac_addr;
          previous_word.data(95, 48)  = my_mac_addr;
          previous_word.data(111, 96) = 0x0008;

          previous_word.keep(13, 0) = 0x3FFF;

          mw_state = WRITE_FIRST_TRANSACTION;
        } else
          mw_state = DROP_IP;
      }
      break;
    case DROP_IP:
      if (!ip_header_with_checksum.empty()) {
        ip_header_with_checksum.read(current_ip_checksum);

        if (current_ip_checksum.last == 1)
          mw_state = WAIT_LOOKUP;
        else
          mw_state = DROP_NO_IP;
      }
      break;

    case DROP_NO_IP:
      if (!no_ip_header_out.empty()) {
        no_ip_header_out.read(current_no_ip);

        if (current_no_ip.last == 1)
          mw_state = WAIT_LOOKUP;
      }
      break;

    case WRITE_FIRST_TRANSACTION:
      if (!ip_header_with_checksum.empty()) {
        ip_header_with_checksum.read(current_ip_checksum);

        sendWord.data(111, 0) = previous_word.data;  // Insert Ethernet header
        sendWord.keep(13, 0)  = previous_word.keep;

        sendWord.data(511, 112) = current_ip_checksum.data(399, 0);  // Compose output word
        sendWord.keep(63, 14)   = current_ip_checksum.keep(49, 0);

        previous_word.data = current_ip_checksum.data(511, 400);
        previous_word.keep = current_ip_checksum.keep(63, 50);
        sendWord.last      = 0;

        if (current_ip_checksum.last == 1) {
          if (current_ip_checksum.keep[50]) {
            mw_state = WRITE_EXTRA_LAST_WORD;
          }  // If the current word has more than 50 bytes a extra transaction
             // for remaining data is needed
          else {
            sendWord.last = 1;
            mw_state      = WAIT_LOOKUP;
          }
        } else {
          mw_state = WRITE_REMAINING;
        }

        eth_frame_out.write(sendWord.to_net_axis());
      }
      break;

    case WRITE_REMAINING:
      if (!no_ip_header_out.empty()) {
        no_ip_header_out.read(current_no_ip);

        sendWord.data(111, 0) = previous_word.data;
        sendWord.keep(13, 0)  = previous_word.keep;

        sendWord.data(511, 112) = current_no_ip.data(399, 0);  // Compose output word
        sendWord.keep(63, 14)   = current_no_ip.keep(49, 0);

        previous_word.data = current_no_ip.data(511, 400);
        previous_word.keep = current_no_ip.keep(63, 50);
        sendWord.last      = 0;

        if (current_no_ip.last == 1) {
          if (current_no_ip.keep.bit(50)) {
            mw_state = WRITE_EXTRA_LAST_WORD;
          }  // If the current word has more than 50 bytes a extra transaction
             // for remaining data is needed
          else {
            sendWord.last = 1;
            mw_state      = WAIT_LOOKUP;
          }
        }

        eth_frame_out.write(sendWord.to_net_axis());
      }
      break;

    case WRITE_EXTRA_LAST_WORD:
      sendWord.data(111, 0)   = previous_word.data;
      sendWord.keep(13, 0)    = previous_word.keep;
      sendWord.data(511, 112) = 0;
      sendWord.keep(63, 14)   = 0;
      sendWord.last           = 1;
      eth_frame_out.write(sendWord.to_net_axis());
      mw_state = WAIT_LOOKUP;

      break;

    default:
      mw_state = WAIT_LOOKUP;
      break;
  }
}

void                 ComputeAndInsertIpv4Checksum(stream<NetAXISWord> &ip_header_without_checksum,
                                                  stream<NetAXISWord> &ip_header_with_checksum) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

  static ap_uint<16> ip_ops[30];
#pragma HLS ARRAY_PARTITION variable = ip_ops complete dim = 1
  static ap_uint<17> ip_sums_L1[15];
  static ap_uint<18> ip_sums_L2[8];
  static ap_uint<19> ip_sums_L3[4] = {0, 0, 0, 0};
  static ap_uint<20> ip_sums_L4[2];
  static ap_uint<21> ip_sums_L5;
  static ap_uint<17> final_sum;

  static ap_uint<16> ip_chksum;

  static ap_uint<4> ipHeaderLen = 0;
  NetAXISWord       cur_word;
  ap_uint<16>       temp;

  if (!ip_header_without_checksum.empty()) {
    ip_header_without_checksum.read(cur_word);  // 512 bits word

    ipHeaderLen = cur_word.data.range(3, 0);

  data_ops:
    for (int i = 0; i < 30; i++) {
#pragma HLS unroll
      temp(7, 0)  = cur_word.data(i * 16 + 15, i * 16 + 8);
      temp(15, 8) = cur_word.data(i * 16 + 7, i * 16);
      if (i < ipHeaderLen * 2)
        ip_ops[i] = temp;
      else
        ip_ops[i] = 0;
    }

    ip_ops[5] = 0;

    // adder tree
    for (int i = 0; i < 15; i++) {
#pragma HLS unroll
      ip_sums_L1[i] = ip_ops[i * 2] + ip_ops[i * 2 + 1];
    }

    // adder tree L2
    for (int i = 0; i < 7; i++) {
#pragma HLS unroll
      ip_sums_L2[i] = ip_sums_L1[i * 2 + 1] + ip_sums_L1[i * 2];
    }
    ip_sums_L2[7] = ip_sums_L1[14];

    // adder tree L3
    for (int i = 0; i < 4; i++) {
#pragma HLS unroll
      ip_sums_L3[i] = ip_sums_L2[i * 2 + 1] + ip_sums_L2[i * 2];
    }

    ip_sums_L4[0] = ip_sums_L3[0 * 2 + 1] + ip_sums_L3[0 * 2];
    ip_sums_L4[1] = ip_sums_L3[1 * 2 + 1] + ip_sums_L3[1 * 2];

    ip_sums_L5 = ip_sums_L4[1] + ip_sums_L4[0];
    final_sum  = ip_sums_L5.range(15, 0) + ip_sums_L5.range(20, 16);
    final_sum  = final_sum.bit(16) + final_sum;
    ip_chksum  = ~(final_sum.range(15, 0));  // ones complement

    // switch WORD_N
    cur_word.data(95, 88) = ip_chksum(7, 0);
    cur_word.data(87, 80) = ip_chksum(15, 8);
    ip_header_with_checksum.write(cur_word);
  }
}

/** @ingroup mac_ip_encode
 *  This module requests the MAC address of the destination IP address and
 * inserts the Ethener header to the IP packet
 */
void ethernet_header_inserter(

    stream<NetAXIS> &ip_seg_in,      // Input packet (ip aligned)
    stream<NetAXIS> &eth_frame_out,  // Output packet (ethernet aligned)

    stream<ArpTableRsp>  &arp_table_rsp,  // ARP cache replay
    stream<ap_uint<32> > &arp_table_req,  // ARP cache request

    ap_uint<48> &my_mac_addr,      // Server MAC address
    ap_uint<32> &subnet_mask,      // Server subnet mask
    ap_uint<32> &gateway_ip_addr)  // Server default gateway
{
#pragma HLS                        DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port = return

#pragma HLS INTERFACE axis register both port = ip_seg_in
#pragma HLS INTERFACE axis register both port = eth_frame_out

#pragma HLS INTERFACE axis register both port = arp_table_rsp
#pragma HLS aggregate variable = arp_table_rsp compact = bit
#pragma HLS INTERFACE axis register both       port    = arp_table_req

#pragma HLS INTERFACE ap_stable register port = my_mac_addr name = my_mac_addr
#pragma HLS INTERFACE ap_stable register port = subnet_mask name = subnet_mask
#pragma HLS INTERFACE ap_stable register port = gateway_ip_addr name = gateway_ip_addr

  // FIFOs
  static stream<NetAXISWord> ip_header_without_checksum_fifo;
#pragma HLS stream variable = ip_header_without_checksum_fifo depth = 32
#pragma HLS aggregate variable = ip_header_without_checksum_fifo compact = bit

  // second ip pkt word
  static stream<NetAXISWord> no_ip_header_out;
#pragma HLS stream variable = no_ip_header_out depth = 32
#pragma HLS aggregate variable = no_ip_header_out compact = bit

  static stream<NetAXISWord> ip_header_with_checksum_fifo;
#pragma HLS stream variable = ip_header_with_checksum_fifo depth = 32
#pragma HLS aggregate variable = ip_header_with_checksum_fifo compact = bit

  BroadcastMacRequest(ip_seg_in,
                      arp_table_req,
                      ip_header_without_checksum_fifo,
                      no_ip_header_out,
                      subnet_mask,
                      gateway_ip_addr);

  ComputeAndInsertIpv4Checksum(ip_header_without_checksum_fifo, ip_header_with_checksum_fifo);

  EthernetHandleOutput(
      arp_table_rsp, ip_header_with_checksum_fifo, no_ip_header_out, my_mac_addr, eth_frame_out);
}
