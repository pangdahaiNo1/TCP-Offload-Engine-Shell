#ifndef _MOCK_TOE_HPP_
#define _MOCK_TOE_HPP_
#pragma once
#include "toe/toe_conn.hpp"
#include "utils/axi_utils.hpp"
/**
 * NOTE: all varibles are in big endian
 */

// test mac addr
ap_uint<48> mock_my_mac_addr     = SwapByte<48>(0x000A35029DE5);
ap_uint<32> mock_gateway_ip_addr = SwapByte<32>(0xC0A80001);
ap_uint<32> mock_subnet_mask     = SwapByte<32>(0xFFFFFF00);
// test tuple
IpAddr        mock_dst_ip_addr  = 0x0200A8C0;  // 192.168.0.2
IpAddr        mock_src_ip_addr  = 0x0500A8C0;  // 192.168.0.5
TcpPortNumber mock_dst_tcp_port = 0x1580;      // 32789
TcpPortNumber mock_src_tcp_port = 0x8000;      // 128

FourTuple mock_tuple(mock_src_ip_addr, mock_dst_ip_addr, mock_src_tcp_port, mock_dst_tcp_port);
FourTuple reverse_mock_tuple(mock_dst_ip_addr,
                             mock_src_ip_addr,
                             mock_dst_tcp_port,
                             mock_src_tcp_port);

#endif