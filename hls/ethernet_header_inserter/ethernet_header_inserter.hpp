
#ifndef ETHERNET_HEADER_INSERTER_H
#define ETHERNET_HEADER_INSERTER_H
#include "utils/axi_intf.hpp"

using namespace hls;
using namespace std;

struct ArpTableRsp {
  ap_uint<48> mac_addr;
  bool        hit;
  ArpTableRsp() {}
  ArpTableRsp(ap_uint<48> mac_addr_rsp, bool hit) : mac_addr(mac_addr_rsp), hit(hit) {}
};

struct NetAXISMacHeader {
  ap_uint<112> data;
  ap_uint<14>  keep;
  ap_uint<1>   last;
  NetAXISMacHeader() : data(0), keep(0), last(0) {}
};

/** @defgroup mac_ip_encode MAC-IP encode
 *
 */
void ethernet_header_inserter(

    stream<NetAXIS> &ip_seg_in,      // Input packet (ip aligned)
    stream<NetAXIS> &eth_frame_out,  // Output packet (ethernet aligned)

    stream<ArpTableRsp>  &arp_table_rsp,  // ARP cache replay
    stream<ap_uint<32> > &arp_table_req,  // ARP cache request

    ap_uint<48> &my_mac_addr,       // Server MAC address
    ap_uint<32> &subnet_mask,       // Server subnet mask
    ap_uint<32> &gateway_ip_addr);  // Server default gateway

#endif