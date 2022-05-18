#ifndef _IPV4_HPP_
#define _IPV4_HPP_

#include "stdint.h"
#include "utils/axi_utils.hpp"
#include "utils/packet.hpp"
const uint32_t IPV4_HEADER_WIDTH = 160;
/**
 * [7:4] = version;
 * [3:0] = IHL;
 * [13:8] = DSCP;
 * [15:14] = ECN;
 * [31:16] = length;
 * [47:32] = Idendification;
 * [50:48] = Flags;
 * [63:51] = fragment offset;
 * [71:64] = TTL;
 * [79:72] = Protocol;
 * [95:80] = HeaderChecksum;
 * [127:96] = SrcAddr;
 * [159:128] = DstAddr;
 * [...] = IHL;
 */
class Ipv4Header : public PacketHeader<NET_TDATA_WIDTH, IPV4_HEADER_WIDTH> {
  using PacketHeader<NET_TDATA_WIDTH, IPV4_HEADER_WIDTH>::header;

public:
  Ipv4Header() {
    header(7, 0)   = 0x45;  // version & IHL
    header(71, 64) = 0x40;  // TTL
  }

  void        set_src_addr(const ap_uint<32> &addr) { header(127, 96) = addr; }
  ap_uint<32> get_src_addr() { return header(127, 96); }
  void        set_dst_addr(const ap_uint<32> &addr) { header(159, 128) = addr; }
  ap_uint<32> get_dst_addr() { return header(159, 128); }
  void        set_length(const ap_uint<16> len) { header(31, 16) = SwapByte<16>(len); }
  ap_uint<16> get_length() { return SwapByte<16>((ap_uint<16>)header(31, 16)); }
  void        set_protocol(const ap_uint<8> &protocol) { header(79, 72) = protocol; }
  ap_uint<8>  get_protocol() { return header(79, 72); }
  ap_uint<4>  get_header_length() { return header(3, 0); }
  ap_uint<16> get_checksum() { return SwapByte<16>(ap_uint<16>(header(95, 80))); }

#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "IP Packet: \n" << std::hex;
    sstream << "Version: " << header(7, 4) << " IHL: " << header(3, 0)
            << " Total Len: " << this->get_length() << endl;
    sstream << "Protocol: " << this->get_protocol() << " IP checksum: " << this->get_checksum()
            << endl;
    sstream << "Src IP: " << SwapByte<32>(this->get_src_addr())
            << " Dst IP: " << SwapByte<32>(this->get_dst_addr()) << endl;
    return sstream.str();
  }
#endif
};

#endif