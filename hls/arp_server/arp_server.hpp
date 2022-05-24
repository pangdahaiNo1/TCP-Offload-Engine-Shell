#ifndef _ARP_SERVER_HPP_
#define _ARP_SERVER_HPP_

#include "toe/toe_intf.hpp"

using namespace hls;

const uint16_t ARP_REQUEST = 0x0100;
const uint16_t ARP_REPLY   = 0x0200;

const ap_uint<48> BROADCAST_MAC = 0xFFFFFFFFFFFF;  // Broadcast MAC Address

// This flag enables an aggressive scanning of the IP in the subnet
// For instance, at the beginning ARP packets will be sent to each IP in the
// subnet
#define SCANNING 0

#ifdef __SYNTHESIS__
#define MAX_COUNT 1500000000  // around 5 seconds
#else
#define MAX_COUNT 50  // short time for simulation purposes
#endif

struct ArpTableEntry {
  ap_uint<48> mac_addr;
  ap_uint<32> ip_addr;
  ap_uint<1>  valid;
  ArpTableEntry() {}
  ArpTableEntry(ap_uint<48> new_mac_addr, ap_uint<32> new_ip_addr, ap_uint<1> new_entry_is_valid)
      : mac_addr(new_mac_addr), ip_addr(new_ip_addr), valid(new_entry_is_valid) {}
};

struct ArpTableRsp {
  ap_uint<48> mac_addr;
  bool        hit;
  ArpTableRsp() {}
  ArpTableRsp(ap_uint<48> mac_addr_rsp, bool hit) : mac_addr(mac_addr_rsp), hit(hit) {}
};

struct ArpMetaRsp {
  ap_uint<48> src_mac_addr;  // rename
  ap_uint<16> eth_type;
  ap_uint<16> arp_hw_type;
  ap_uint<16> arp_proto_type;
  ap_uint<8>  arp_hw_len;
  ap_uint<8>  arp_proto_len;
  ap_uint<48> arp_src_hw_addr;
  ap_uint<32> arp_src_proto_addr;
  ArpMetaRsp() {}
};

void arp_server(stream<NetAXIS> &     arp_data_in,
                stream<ap_uint<32> > &mac_ip_encode_req,
                stream<NetAXIS> &     arp_data_out,
                stream<ArpTableRsp> & mac_ip_encode_rsp,
                ArpTableEntry         arp_cache_table[256],
                ap_uint<1> &          arp_scan,
                ap_uint<48> &         my_mac_addr,
                ap_uint<32> &         my_ip_addr,
                ap_uint<32> &         gateway_ip_addr,
                ap_uint<32> &         subnet_mask);

#endif