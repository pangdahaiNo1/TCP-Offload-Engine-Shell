#include "packet_handler.hpp"

/**
 * @brief      Shave off the Ethernet when is needed. (IPv4) packets
 *
 * @param      data_in   The data in
 * @param      data_out  The data out
 */
void                 EthernetRemover(stream<NetAXIS> &data_in, stream<NetAXIS> &data_out) {
#pragma HLS PIPELINE II = 1

  enum EthernetRemoverStates { FIRST_WORD, FWD, REMOVING, EXTRA };
  static EthernetRemoverStates er_fsm_state = FIRST_WORD;

  NetAXIS        curr_word;
  NetAXIS        send_word;
  static NetAXIS prev_word;

  switch (er_fsm_state) {
    case FIRST_WORD:
      if (!data_in.empty()) {
        data_in.read(curr_word);

        if (curr_word.dest == 0) {  // ARP packets must remain intact
          send_word    = curr_word;
          er_fsm_state = FWD;
        } else {  // No ARP packet, Re arrange the order in the output word
          send_word.data(511, 400) = 0;
          send_word.keep(63, 50)   = 0;
          send_word.data(399, 0)   = curr_word.data(511, 112);
          send_word.keep(49, 0)    = curr_word.keep(63, 14);
          send_word.dest           = curr_word.dest;
          send_word.last           = 1;
          er_fsm_state             = REMOVING;
        }

        if (curr_word.last) {  // If the packet is short stay in this state
          data_out.write(send_word);
          er_fsm_state = FIRST_WORD;
        } else if (curr_word.dest == 0) {  // ARP packets
          data_out.write(send_word);
        }

        prev_word = curr_word;
      }
      break;
    case FWD:
      if (!data_in.empty()) {
        data_in.read(curr_word);
        if (curr_word.last) {
          er_fsm_state = FIRST_WORD;
        }
        data_out.write(curr_word);
      }
      break;
    case REMOVING:
      if (!data_in.empty()) {
        data_in.read(curr_word);

        send_word.data(399, 0)   = prev_word.data(511, 112);
        send_word.keep(49, 0)    = prev_word.keep(63, 14);
        send_word.data(511, 400) = curr_word.data(111, 0);
        send_word.keep(63, 50)   = curr_word.keep(13, 0);
        send_word.dest           = prev_word.dest;

        if (curr_word.last) {
          if (curr_word.keep.bit(14)) {  // When the input packet ends we have to
                                         // check if all the input data
            er_fsm_state = EXTRA;        // was sent, if not an extra transaction is
                                         // needed, but for the current
            send_word.last = 0;          // transaction tlast must be 0
          } else {
            send_word.last = 1;
            er_fsm_state   = FIRST_WORD;
          }
        } else {
          send_word.last = 0;
        }

        prev_word = curr_word;
        data_out.write(send_word);
      }
      break;
    case EXTRA:
      send_word.data(511, 400) = 0;  // Send the remaining piece of information
      send_word.keep(63, 50)   = 0;
      send_word.data(399, 0)   = prev_word.data(511, 112);
      send_word.keep(49, 0)    = prev_word.keep(63, 14);
      send_word.dest           = prev_word.dest;
      send_word.last           = 1;
      data_out.write(send_word);
      er_fsm_state = FIRST_WORD;
      break;
  }
}

/**
 * @brief      Packet identification, depending on its kind.
 * 			   If the packet does not belong to one of these
 * 			   categories ARP, ICMP, TCP, UDP is dropped.
 * 			   It also a label for each kind of packet is added
 * 			   in the tdest.
 *             Tdest : 0 ARP
 *                   : 1 ICMP
 *                   : 2 TCP
 *                   : 3 UDP
 *
 * @param      data_in   The data in
 * @param      data_out  The data out
 */
void                 PacketIdentify(stream<NetAXIS> &data_in, stream<NetAXIS> &data_out) {
#pragma HLS PIPELINE II = 1

  enum PacketIdentifyStates { FIRST_WORD, FWD, DROP };
  static PacketIdentifyStates pi_fsm_state = FIRST_WORD;
  static NetAXISDest          tdest_r;

  NetAXISDest tdest;
  NetAXIS     curr_word;
  NetAXIS     send_word;
  ap_uint<16> ethernet_type;
  ap_uint<4>  ip_version;
  ap_uint<8>  ip_protocol;
  bool        forward = true;

  switch (pi_fsm_state) {
    case FIRST_WORD:
      if (!data_in.empty()) {
        data_in.read(curr_word);
        ethernet_type = SwapByte<16>(curr_word.data(111, 96));  // Get Ethernet type
        ip_version    = curr_word.data(119, 116);               // Get IPv4
        ip_protocol   = curr_word.data(191, 184);               // Get protocol for IPv4 packets

        if (ethernet_type == TYPE_ARP) {
          tdest          = 0;
          send_word.dest = 0;
        } else if (ethernet_type == TYPE_IPV4) {
          if (ip_version == 4) {  // Double check
            if (ip_protocol == PROTO_ICMP) {
              tdest = 1;
            } else if (ip_protocol == PROTO_TCP) {
              tdest = 2;
            } else if (ip_protocol == PROTO_UDP) {
              tdest = 3;
            } else {
              forward = false;
            }
          }
        } else {
          forward = false;
        }

        send_word.data = curr_word.data;
        send_word.keep = curr_word.keep;
        send_word.last = curr_word.last;
        send_word.dest = tdest;  // Compose output word

        tdest_r = tdest;  // Save tdest

        if (forward) {  // Evaluate if the packet has to be send or dropped
          data_out.write(send_word);
          pi_fsm_state = FWD;
        } else {
          pi_fsm_state = DROP;
        }

        if (curr_word.last) {
          pi_fsm_state = FIRST_WORD;
        }
      }
      break;
    case FWD:
      if (!data_in.empty()) {
        data_in.read(curr_word);

        send_word.data = curr_word.data;
        send_word.keep = curr_word.keep;
        send_word.last = curr_word.last;
        send_word.dest = tdest_r;
        data_out.write(send_word);

        if (curr_word.last) {  // Keep sending the packet until ends
          pi_fsm_state = FIRST_WORD;
        }
      }
      break;
    case DROP:
      if (!data_in.empty()) {
        data_in.read(curr_word);
        if (curr_word.last) {  // Dropping the packet until ends
          pi_fsm_state = FIRST_WORD;
        }
        break;
      }
  }
}

/**
 * @brief      PacketHandler: wrapper for packet identification and Ethernet
 * remover
 *
 * @param      data_in   Incoming data from the network interface, at Ethernet
 * level
 * @param      data_out  Output data. The tdest says which kind of packet it is.
 * 						The Ethernet header is shoved
 * off for IPv4 packets
 *
 */
void        PacketHandler(stream<NetAXIS> &data_in, stream<NetAXIS> &data_out) {
#pragma HLS DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port = return

#pragma HLS INTERFACE axis register both port = data_in name = s_axis
#pragma HLS INTERFACE axis register both port = data_out name = m_axis

  static stream<NetAXIS> eth_level_pkt("eth_level_pkt");
#pragma HLS DATA_PACK variable = eth_level_pkt
#pragma HLS STREAM variable = eth_level_pkt depth = 16

  PacketIdentify(data_in, eth_level_pkt);

  EthernetRemover(eth_level_pkt, data_out);
}
