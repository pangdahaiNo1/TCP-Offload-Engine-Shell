#include "arp_server.hpp"

/** @ingroup    arp_server
 *
 * @brief      Parse ARP packets and generate a transaction with
 *             metatada if the packet is a REQUEST
 *             Generate a transaction to update the ARP table if
 *             the packet is a REPLAY
 *
 * @param      arp_data_in         Incoming ARP packet at Ethernet level
 * @param      arp_rsp_out    	   Internal metatadata to generate a replay
 * @param      arp_table_insert_out Insert a new element in the ARP table
 * @param      my_ip_addr           My IP address
 */
void                 ArpPkgReceiver(stream<NetAXIS>       &arp_data_in,
                                    stream<ArpMetaRsp>    &arp_rsp_out,
                                    stream<ArpTableEntry> &arp_table_insert_out,
                                    ap_uint<32>           &my_ip_addr) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static ap_uint<4> word_count = 0;
  ap_uint<16>       arp_op_code;
  ap_uint<32>       arp_dst_proto_addr;
  ArpMetaRsp        meta;
  NetAXIS           currWord;

  if (!arp_data_in.empty()) {
    arp_data_in.read(currWord);
    // The first transaction contains all the necessary information
    if (word_count == 0) {
      meta.src_mac_addr       = currWord.data(95, 48);
      meta.eth_type           = currWord.data(111, 96);
      meta.arp_hw_type        = currWord.data(127, 112);
      meta.arp_proto_type     = currWord.data(143, 128);
      meta.arp_hw_len         = currWord.data(151, 144);
      meta.arp_proto_len      = currWord.data(159, 152);
      arp_op_code             = currWord.data(175, 160);
      meta.arp_src_hw_addr    = currWord.data(223, 176);
      meta.arp_src_proto_addr = currWord.data(255, 224);
      arp_dst_proto_addr      = currWord.data(335, 304);

      if ((arp_op_code == ARP_REQUEST) && (arp_dst_proto_addr == my_ip_addr))
        arp_rsp_out.write(meta);
      else if ((arp_op_code == ARP_REPLY) && (arp_dst_proto_addr == my_ip_addr))
        arp_table_insert_out.write(
            ArpTableEntry(meta.arp_src_hw_addr, meta.arp_src_proto_addr, true));
    }

    if (currWord.last)
      word_count = 0;
    else
      word_count++;
  }
}

/** @ingroup    arp_server
 *
 * @brief      Sends both ARP replay or ARP request packets
 *
 * @param      arp_rsp_in   Metadata information to generate an ARP replay
 * @param      arp_request_ip_addr IP address to generate an ARP request
 * @param      arp_data_out   APR packet out at Ethernet level
 * @param      my_mac_addr My MAC address
 * @param      my_ip_addr  My IP address
 * @param      gateway_ip_addr    My gateway ip
 * @param      subnet_mask  Network mask
 */
void                 ArpPkgSender(stream<ArpMetaRsp>   &arp_rsp_in,
                                  stream<ap_uint<32> > &arp_request_ip_addr,
                                  stream<NetAXIS>      &arp_data_out,
                                  ap_uint<48>          &my_mac_addr,
                                  ap_uint<32>          &my_ip_addr,
                                  ap_uint<32>          &gateway_ip_addr,
                                  ap_uint<32>          &subnet_mask) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  enum ArpSendStateType { ARP_IDLE_STATE, ARP_REPLY_STATE, ARP_SENTREQ_STATE };
  static ArpSendStateType aps_fsmState = ARP_IDLE_STATE;

  static ArpMetaRsp  arp_meta_rsp;
  static ap_uint<32> arp_req_ip;

  NetAXIS     send_word;
  ap_uint<32> arp_req_next_hop_ip;
  switch (aps_fsmState) {
    case ARP_IDLE_STATE:
      if (!arp_rsp_in.empty()) {
        arp_rsp_in.read(arp_meta_rsp);
        aps_fsmState = ARP_REPLY_STATE;
      } else if (!arp_request_ip_addr.empty()) {
        arp_request_ip_addr.read(arp_req_ip);
        aps_fsmState = ARP_SENTREQ_STATE;
      }
      break;
    // SEND REQUEST
    case ARP_SENTREQ_STATE:

      // Check whether the IP is within the FPGA subnetwork.
      // IP within subnet, use IP
      // IP outside subnet use gateway_ip_addr
      if ((arp_req_ip & subnet_mask) == (my_ip_addr & subnet_mask))
        arp_req_next_hop_ip = arp_req_ip;
      else
        arp_req_next_hop_ip = gateway_ip_addr;

      send_word.data(47, 0)   = BROADCAST_MAC;
      send_word.data(95, 48)  = my_mac_addr;  // Source MAC
      send_word.data(111, 96) = 0x0608;       // Ethertype

      send_word.data(127, 112) = 0x0100;  // Hardware type
      send_word.data(143, 128) = 0x0008;  // Protocol type IP Address
      send_word.data(151, 144) = 6;       // HW Address Length
      send_word.data(159, 152) = 4;       // Protocol Address Length
      send_word.data(175, 160) = ARP_REQUEST;
      send_word.data(223, 176) = my_mac_addr;
      send_word.data(255, 224) = my_ip_addr;  // MY_IP_ADDR;
      send_word.data(303, 256) = 0;           // Sought-after MAC pt.1
      send_word.data(335, 304) = arp_req_next_hop_ip;
      send_word.data(383, 336) = 0;               // Sought-after MAC pt.1
      send_word.data(511, 384) = 0x454546464F43;  // padding

      send_word.keep = 0x0FFFFFFFFFFFFFFF;  // 60-Byte packet
      send_word.last = 1;
      aps_fsmState   = ARP_IDLE_STATE;

      arp_data_out.write(send_word);
      break;
    // SEND REPLAY
    case ARP_REPLY_STATE:

      send_word.data(47, 0)   = arp_meta_rsp.src_mac_addr;
      send_word.data(95, 48)  = my_mac_addr;            // Source MAC
      send_word.data(111, 96) = arp_meta_rsp.eth_type;  // Ethertype

      send_word.data(127, 112) = arp_meta_rsp.arp_hw_type;     // Hardware type
      send_word.data(143, 128) = arp_meta_rsp.arp_proto_type;  // Protocol type IP Address
      send_word.data(151, 144) = arp_meta_rsp.arp_hw_len;      // HW Address Length
      send_word.data(159, 152) = arp_meta_rsp.arp_proto_len;   // Protocol Address Length
      send_word.data(175, 160) = ARP_REPLY;
      send_word.data(223, 176) = my_mac_addr;
      send_word.data(255, 224) = my_ip_addr;                    // MY_IP_ADDR;
      send_word.data(303, 256) = arp_meta_rsp.arp_src_hw_addr;  // Sought-after MAC pt.1
      send_word.data(335, 304) = arp_meta_rsp.arp_src_proto_addr;
      send_word.data(383, 336) = 0;               // Sought-after MAC pt.1
      send_word.data(511, 384) = 0x454546464F43;  // padding

      send_word.keep = 0x0FFFFFFFFFFFFFFF;
      send_word.last = 1;
      aps_fsmState   = ARP_IDLE_STATE;

      arp_data_out.write(send_word);
      break;
  }  // switch
}

/** @ingroup    arp_server
 *
 * @brief      Update and query ARP table
 *
 * @param      arp_table_insert_in    Insert a new element in the table
 * @param      mac_ip_encode_req     Request to get the associated MAC address of
 * a given IP address
 * @param      mac_ip_encode_rsp     Response to an IP association
 * @param      arp_requested_ip  	   Generate an ARP request
 * @param      arp_cache_table            ARP table accessible from AXI4-Lite as well
 * @param      my_ip_addr         My IP address
 * @param      gateway_ip_addr           My gateway IP
 * @param      subnet_mask         Network mask
 */
void                 ArpTableHandler(stream<ArpTableEntry> &arp_table_insert_in,
                                     stream<ap_uint<32> >  &mac_ip_encode_req,
                                     stream<ArpTableRsp>   &mac_ip_encode_rsp,
                                     stream<ap_uint<32> >  &arp_requested_ip,
                                     ArpTableEntry          arp_cache_table[256],
                                     ap_uint<32>           &my_ip_addr,
                                     ap_uint<32>           &gateway_ip_addr,
                                     ap_uint<32>           &subnet_mask) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

#pragma HLS DEPENDENCE variable = arp_cache_table inter false

  ap_uint<32>   req_ip;
  ap_uint<32>   next_hop_ip;
  ArpTableEntry cur_table_entry;

  if (!arp_table_insert_in.empty()) {
    arp_table_insert_in.read(cur_table_entry);
    arp_cache_table[cur_table_entry.ip_addr(31, 24)] = cur_table_entry;
  } else if (!mac_ip_encode_req.empty()) {
    mac_ip_encode_req.read(req_ip);

    /*Check whether the current IP belongs to the FPGA subnet, otherwise use
     * gateway's IP*/
    if ((req_ip & subnet_mask) == (my_ip_addr & subnet_mask))
      next_hop_ip = req_ip;
    else
      next_hop_ip = gateway_ip_addr;

    cur_table_entry = arp_cache_table[next_hop_ip(31, 24)];
    // If the entry is not valid generate an ARP request
    if (!cur_table_entry.valid) {
      arp_requested_ip.write(req_ip);  // send ARP request
    }
    // Send response to an IP to MAC association
    mac_ip_encode_rsp.write(ArpTableRsp(cur_table_entry.mac_addr, cur_table_entry.valid));
  }
}

/**
 * @brief      Generate an ARP discovery for all the IP in the subnet
 *
 * @param      mac_ip_encode_req_in      Inbound request to associate an IP to a MAC
 * @param      mac_ip_encode_rsp_in  Internal response to an IP association
 * @param      mac_ip_encode_rsp_out  Outbound response to an IP association
 * @param      mac_ip_encode_req_out     Internal request to associate an IP to a MAC
 * @param      my_ip_addr        My ip address
 */
void                 GenArpScan(stream<ap_uint<32> > &mac_ip_encode_req_in,
                                stream<ArpTableRsp>  &mac_ip_encode_rsp_in,
                                stream<ArpTableRsp>  &mac_ip_encode_rsp_out,
                                stream<ap_uint<32> > &mac_ip_encode_req_out,
                                ap_uint<1>           &arp_scan,
                                ap_uint<32>          &my_ip_addr) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  enum GenScanFsmState { SETUP, WAIT_TIME, GEN_IP, FWD, WAIT_RESPONSE };
#if SCANNING
  static GenScanFsmState fsm_state = WAIT_TIME;
#else
  static GenScanFsmState fsm_state = FWD;
#endif

#pragma HLS RESET variable = fsm_state

  static ap_uint<32> time_counter = 0;
  static ap_uint<8>  ip_lsb       = 0;
  static ap_uint<1>  arp_scan_1d  = 0;
  ap_uint<32>        ip_aux;
  ArpTableRsp        macEnc_i;
  ap_uint<1>         checkArpScan = 0;

  switch (fsm_state) {
    // Wait for a period of time until starting with the ARP Request generation
    case WAIT_TIME:
      if (time_counter == MAX_COUNT)
        fsm_state = GEN_IP;
      time_counter++;
      break;
    // Generate IP addresses in the same subnetwork as my IP address and go to
    // wait state
    case GEN_IP:
      ip_aux = (ip_lsb, my_ip_addr(23, 0));
      mac_ip_encode_req_out.write(ip_aux);  // Support for Gratuitous ARP
      ip_lsb++;
      fsm_state = WAIT_RESPONSE;
      break;
    // Wait for ARP response
    case WAIT_RESPONSE:
      if (!mac_ip_encode_rsp_in.empty()) {
        mac_ip_encode_rsp_in.read(macEnc_i);
        // The ARP ends when overflow happens
        if (ip_lsb == 0) {
          fsm_state = FWD;
        } else
          fsm_state = GEN_IP;
      }
      break;
    // Forward incoming request and outgoing replays appropriately
    case FWD:
      if (!mac_ip_encode_req_in.empty())
        mac_ip_encode_req_out.write(mac_ip_encode_req_in.read());
      else
        checkArpScan = 1;

      if (!mac_ip_encode_rsp_in.empty())
        mac_ip_encode_rsp_out.write(mac_ip_encode_rsp_in.read());
      else
        checkArpScan = 1;

      if ((checkArpScan == 1) && (arp_scan_1d == 0) && (arp_scan == 1)) {
        arp_scan  = 0;
        fsm_state = SETUP;
      }
      break;
    // Clear variables
    case SETUP:
      ip_lsb    = 0;
      fsm_state = GEN_IP;
      break;
  }
  arp_scan_1d = arp_scan;
}

/** @ingroup    arp_server
 *
 * @brief Implements the basic functionality of ARP  Send REQUEST, process REPLAY and keep a table
 * where IP and MAC are associated
 *
 * @param arp_data_in      Inbound ARP packets
 * @param mac_ip_encode_req  Request to associate an IP to a MAC
 * @param arp_data_out     Outbound ARP packets
 * @param mac_ip_encode_rsp  Response to the IP to a MAC association
 * @param arp_cache_table  ARP table accessible from AXI4-Lite as well
 * @param my_mac_addr      My MAC address
 * @param my_ip_addr       My IP address
 * @param gateway_ip_addr  My gateway IP
 * @param subnet_mask      network mask
 */
void                               arp_server(stream<NetAXIS>      &arp_data_in,
                                              stream<ap_uint<32> > &mac_ip_encode_req,
                                              stream<NetAXIS>      &arp_data_out,
                                              stream<ArpTableRsp>  &mac_ip_encode_rsp,
                                              ArpTableEntry         arp_cache_table[256],
                                              ap_uint<1>           &arp_scan,
                                              ap_uint<48>          &my_mac_addr,
                                              ap_uint<32>          &my_ip_addr,
                                              ap_uint<32>          &gateway_ip_addr,
                                              ap_uint<32>          &subnet_mask) {
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS                        DATAFLOW

#pragma HLS INTERFACE ap_none register port = my_mac_addr
#pragma HLS INTERFACE ap_none register port = my_ip_addr
#pragma HLS INTERFACE ap_none register port = gateway_ip_addr
#pragma HLS INTERFACE ap_none register port = subnet_mask

#pragma HLS INTERFACE axis register both port = arp_data_in
#pragma HLS INTERFACE axis register both port = arp_data_out
#pragma HLS INTERFACE axis register both port = mac_ip_encode_req
#pragma HLS INTERFACE axis register both port = mac_ip_encode_rsp
#pragma HLS aggregate variable = mac_ip_encode_rsp compact = bit
#pragma HLS INTERFACE s_axilite port = arp_scan bundle = s_axilite
#pragma HLS INTERFACE s_axilite port = arp_cache_table bundle = s_axilite

  static stream<ArpMetaRsp> arp_reply_fifo("arp_reply_fifo");
#pragma HLS STREAM variable = arp_reply_fifo depth = 4
#pragma HLS aggregate variable = arp_reply_fifo compact = bit

  static stream<ap_uint<32> > arp_request_fifo("arp_request_fifo");
#pragma HLS STREAM variable = arp_request_fifo depth = 4

  static stream<ArpTableEntry> arp_cache_table_insert_fifo("arp_cache_table_insert_fifo");
#pragma HLS STREAM variable = arp_cache_table_insert_fifo depth = 4
#pragma HLS aggregate variable = arp_cache_table_insert_fifo compact = bit

  static stream<ap_uint<32> > mac_ip_encode_req_fifo("mac_ip_encode_req_fifo");
#pragma HLS STREAM variable = mac_ip_encode_req_fifo depth = 4

  static stream<ArpTableRsp> mac_ip_encode_rsp_in_fifo("mac_ip_encode_rsp_in_fifo");
#pragma HLS STREAM variable = mac_ip_encode_rsp_in_fifo depth = 4
#pragma HLS aggregate variable = mac_ip_encode_rsp_in_fifo compact = bit

  GenArpScan(mac_ip_encode_req,
             mac_ip_encode_rsp_in_fifo,
             mac_ip_encode_rsp,
             mac_ip_encode_req_fifo,
             arp_scan,
             my_ip_addr);

  ArpPkgReceiver(arp_data_in, arp_reply_fifo, arp_cache_table_insert_fifo, my_ip_addr);

  ArpPkgSender(arp_reply_fifo,
               arp_request_fifo,
               arp_data_out,
               my_mac_addr,
               my_ip_addr,
               gateway_ip_addr,
               subnet_mask);

  ArpTableHandler(arp_cache_table_insert_fifo,
                  mac_ip_encode_req_fifo,
                  mac_ip_encode_rsp_in_fifo,
                  arp_request_fifo,
                  arp_cache_table,
                  my_ip_addr,
                  gateway_ip_addr,
                  subnet_mask);
}
