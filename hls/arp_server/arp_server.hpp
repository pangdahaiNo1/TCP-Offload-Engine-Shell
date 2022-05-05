
#include "ap_int.h"
#include "toe/toe_intf.hpp"

#include <fstream>
#include <hls_stream.h>
#include <iostream>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string>

#ifndef _ARP_SERVER_HPP_
#define _ARP_SERVER_HPP_

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
  ArpTableRsp(ap_uint<48> macAdd, bool hit) : mac_addr(macAdd), hit(hit) {}
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

void arp_server(stream<NetAXIS> &     arpDataIn,
                stream<ap_uint<32> > &macIpEncode_req,
                stream<NetAXIS> &     arpDataOut,
                stream<ArpTableRsp> & macIpEncode_rsp,
                ArpTableEntry         arpTable[256],
                ap_uint<1> &          arp_scan,
                ap_uint<48> &         myMacAddress,
                ap_uint<32> &         myIpAddress,
                ap_uint<32> &         gatewayIP,
                ap_uint<32> &         networkMask);

#endif