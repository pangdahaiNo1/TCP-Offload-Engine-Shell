
#ifndef _PACKET_HANDLER_HPP_
#define _PACKET_HANDLER_HPP_
#include "ap_int.h"
#include "utils/axi_utils.hpp"

#include <cstdlib>
#include <fstream>
#include <hls_stream.h>
#include <iostream>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string>

using namespace hls;

const ap_uint<16> TYPE_IPV4 = 0x0800;
const ap_uint<16> TYPE_ARP  = 0x0806;

const ap_uint<8> PROTO_ICMP = 1;
const ap_uint<8> PROTO_TCP  = 6;
const ap_uint<8> PROTO_UDP  = 17;

using hls::stream;
/**
 * @brief      PacketHandler: wrapper for packet identification and Ethernet
 * remover
 *
 * @param      dataIn   Incoming data from the network interface, at Ethernet
 * level
 * @param      dataOut  Output data. The tdest says which kind of packet it is.
 * 						The Ethernet header is shoved off for IPv4
 * packets
 *
 */
void PacketHandler(stream<NetAXIS> &dataIn, stream<NetAXIS> &dataOut);

#endif