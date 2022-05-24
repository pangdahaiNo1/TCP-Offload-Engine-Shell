#ifndef _TCP_HEADER_
#define _TCP_HEADER_
#include "toe/toe_conn.hpp"
#include "utils/axi_utils.hpp"
#include "utils/packet.hpp"

const uint32_t TCP_PSEUDO_HEADER_WIDTH      = 96;
const uint32_t TCP_HEADER_WITH_PSEUDO_WIDTH = 256;
const uint32_t TCP_HEADER_WIDTH             = 160;

/**
 * [31:4] = source address;
 * [63:32] = destination address;
 * [71:64] = zeros;
 * [79:72] = protocol;
 * [95:80] = length;
 */
class TcpPseudoHeader : public PacketHeader<NET_TDATA_WIDTH, TCP_PSEUDO_HEADER_WIDTH> {
  using PacketHeader<NET_TDATA_WIDTH, TCP_PSEUDO_HEADER_WIDTH>::header;

public:
  TcpPseudoHeader() {
    header(71, 64) = 0x00;  // zeros
    header(79, 72) = 0x06;  // protocol
  }

  void        set_src_addr(const ap_uint<32> &addr) { header(31, 0) = addr; }
  ap_uint<32> get_src_addr() { return header(31, 0); }
  void        set_dst_addr(const ap_uint<32> &addr) { header(63, 32) = addr; }
  ap_uint<32> get_dst_addr() { return header(63, 32); }
  void        set_length(const ap_uint<16> len) { header(95, 80) = SwapByte<16>(len); }
  ap_uint<16> get_length() { return SwapByte<16>((ap_uint<16>)header(95, 80)); }
  void        set_protocol(const ap_uint<8> &protocol) { header(79, 72) = protocol; }
  ap_uint<8>  get_protocol() { return header(79, 72); }

#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TCP PseudoHeader: \n" << std::hex;
    sstream << "Source Addr: " << SwapByte(this->get_src_addr()) << endl;
    sstream << "Dest Addr: " << SwapByte(this->get_dst_addr()) << endl;
    sstream << "TCP Packet length: " << this->get_length() << endl;
    return sstream.str();
  }
#endif
};

/**
 * TCP psesudo header + TCP header with opitions
 *
 * [31:0] = source address
 * [63:32] = destination address
 * [71:64] = zeros
 * [79:72] = protocol
 * [95:80] = length
 *
 * [111:96] = source port
 * [127:112] = destination port
 * [159:128] = sequence number
 * [191:160] = acknowledgement number
 * [192] = NS flag, RFC 3540
 * [195:193] = reserved
 * [199:196] = data offset
 * [200] = FIN flag
 * [201] = SYN flag
 * [202] = RST flag
 * [203] = PSH flag
 * [204] = ACK flag
 * [205] = URG flag
 * [206] = ECE flag
 * [207] = CWR flag
 * [223:208] = window
 * [239:224] = checksum
 * [255:240] = urgent pointer
 * [263:256] = option kind
 * [271:264] = option length
 * [287:272] = MSS
 */
class TcpPseudoFullHeader : public PacketHeader<NET_TDATA_WIDTH, TCP_HEADER_WITH_PSEUDO_WIDTH> {
  using PacketHeader<NET_TDATA_WIDTH, TCP_HEADER_WITH_PSEUDO_WIDTH>::header;

public:
  TcpPseudoFullHeader() {
    header(71, 64)   = 0x00;  // zeros
    header(79, 72)   = 0x06;  // protocol
    header[192]      = 0;     // NS bit
    header(195, 193) = 0;     // reserved
    header[203]      = 0;     // flag
    header(207, 205) = 0;     // flags
    header(255, 240) = 0;     // urgent pointer
  }

  void        set_src_addr(const ap_uint<32> &addr) { header(31, 0) = addr; }
  ap_uint<32> get_src_addr() { return header(31, 0); }
  void        set_dst_addr(const ap_uint<32> &addr) { header(63, 32) = addr; }
  ap_uint<32> get_dst_addr() { return header(63, 32); }
  void        set_protocol(const ap_uint<8> &protocol) { header(79, 72) = protocol; }
  ap_uint<8>  get_protocol() { return header(79, 72); }
  void        set_length(const ap_uint<16> len) { header(95, 80) = SwapByte<16>(len); }
  ap_uint<16> get_length() { return SwapByte<16>(header(95, 80)); }
  void        set_src_port(const ap_uint<16> &port) { header(111, 96) = port; }
  ap_uint<16> get_src_port() { return header(111, 96); }
  void        set_dst_port(const ap_uint<16> &port) { header(127, 112) = port; }
  ap_uint<16> get_dst_port() { return header(127, 112); }
  void        set_seq_number(const ap_uint<32> &seq) { header(159, 128) = SwapByte<32>(seq); }
  ap_uint<32> get_seq_number() { return SwapByte<32>((ap_uint<32>)header(159, 128)); }
  void        set_ack_number(const ap_uint<32> &ack) { header(191, 160) = SwapByte<32>(ack); }
  ap_uint<32> get_ack_number() { return SwapByte<32>((ap_uint<32>)header(191, 160)); }
  void        set_data_offset(ap_uint<4> offset) { header(199, 196) = offset; }
  ap_uint<4>  get_data_offset() { return header(199, 196); }
  ap_uint<1>  get_ack_flag() { return header[204]; }
  void        set_ack_flag(ap_uint<1> flag) { header[204] = flag; }
  ap_uint<1>  get_rst_flag() { return header[202]; }
  void        set_fst_flag(ap_uint<1> flag) { header[202] = flag; }
  ap_uint<1>  get_syn_flag() { return header[201]; }
  void        set_syn_flag(ap_uint<1> flag) { header[201] = flag; }
  ap_uint<1>  get_fin_flag() { return header[200]; }
  void        set_fin_flag(ap_uint<1> flag) { header[200] = flag; }
  void        set_window_size(const ap_uint<16> &size) { header(223, 208) = SwapByte<16>(size); }
  ap_uint<16> get_window_size() { return SwapByte<16>((ap_uint<16>)header(223, 208)); }
  void        set_checksum(const ap_uint<16> &checksum) { header(239, 224) = checksum; }
  ap_uint<16> get_checksum() { return SwapByte<16>(header(239, 224)); }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TCP PseudoHeader: \n";
    sstream << "Source Addr: " << SwapByte(this->get_src_addr()).to_string(16) << endl;
    sstream << "Dest Addr: " << SwapByte(this->get_dst_addr()).to_string(16) << endl;
    sstream << "TCP Packet length(Header+Payload): " << this->get_length().to_string(16) << endl;
    sstream << "TCP Packet: \n";
    sstream << "Flag: " << (this->get_syn_flag() == 1 ? "S" : "")
            << (this->get_fin_flag() == 1 ? "F" : "") << (this->get_ack_flag() == 1 ? "A" : "")
            << (this->get_rst_flag() == 1 ? "R" : "") << endl;
    sstream << "Source Port: " << SwapByte<16>(this->get_src_port()).to_string(16) << endl;
    sstream << "Dest Port: " << SwapByte<16>(this->get_dst_port()).to_string(16) << endl;
    sstream << "Seqence Number: " << this->get_seq_number().to_string(16) << endl;
    sstream << "Acknowlegnement Number: " << this->get_ack_number().to_string(16) << endl;
    sstream << "Window Size: " << this->get_window_size().to_string(16) << endl;
    sstream << "Checksum: " << this->get_checksum().to_string(16) << endl;
    return sstream.str();
  }
#endif
};

/**
 * [15:0] source port
 * [31:16] = destination port
 * [63:32] = sequence number
 * [95:64] = acknowledgement number
 * [96] = NS flag
 * [99:97] = reserved
 * [103:100] = data offset
 * [104] = FIN flag
 * [105] = SYN flag
 * [106] = RST flag
 * [107] = PSH flag
 * [108] = ACK flag
 * [109] = URG flag
 * [110] = ECE flag
 * [111] = CWR flag
 * [127:112] = window size
 * [143:128] = checksum
 * [159:144] = urgent pointer
 */
class TcpHeader : public PacketHeader<NET_TDATA_WIDTH, TCP_HEADER_WIDTH> {
  using PacketHeader<NET_TDATA_WIDTH, TCP_HEADER_WIDTH>::header;

public:
  TcpHeader() {
    // header(71, 64) = 0x00; // zeros
    // header(79, 72) = 0x06; // protocol
    header(99, 97) = 0;  // reserved
  }

  void        set_src_port(const ap_uint<16> &port) { header(15, 0) = port; }
  ap_uint<16> get_src_port() { return header(15, 0); }
  void        set_dst_port(const ap_uint<16> &port) { header(31, 16) = port; }
  ap_uint<16> get_dst_port() { return header(31, 16); }
  void        set_seq_number(const ap_uint<32> &seq) { header(63, 32) = SwapByte<32>(seq); }
  ap_uint<32> get_seq_number() { return SwapByte<32>((ap_uint<32>)header(63, 32)); }
  void        set_ack_number(const ap_uint<32> &ack) { header(95, 64) = SwapByte<32>(ack); }
  ap_uint<32> get_ack_number() { return SwapByte<32>((ap_uint<32>)header(95, 64)); }
  ap_uint<4>  get_data_offset() { return header(103, 100); }
  ap_uint<1>  get_ack_flag() { return header[108]; }
  ap_uint<1>  get_rst_flag() { return header[106]; }
  ap_uint<1>  get_syn_flag() { return header[105]; }
  ap_uint<1>  get_fin_flag() { return header[104]; }
  void        set_window_size(const ap_uint<16> &size) { header(127, 112) = SwapByte<16>(size); }
  ap_uint<16> get_window_size() { return SwapByte<16>((ap_uint<16>)header(127, 112)); }
  void        set_checksum(const ap_uint<16> &checksum) { header(143, 128) = checksum; }
  ap_uint<16> get_checksum() { return header(143, 128); }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TCP Packet: \n" << std::hex;
    sstream << "Flag: " << (this->get_syn_flag() == 1 ? "S" : "")
            << (this->get_fin_flag() == 1 ? "F" : "") << (this->get_ack_flag() == 1 ? "A" : "")
            << (this->get_rst_flag() == 1 ? "R" : "") << endl;
    sstream << "Source Port: " << SwapByte<16>(this->get_src_port()) << endl;
    sstream << "Dest Port: " << SwapByte<16>(this->get_dst_port()) << endl;
    sstream << "Seqence Number: " << this->get_seq_number() << endl;
    sstream << "Acknowlegnement Number: " << this->get_ack_number() << endl;
    sstream << "Window Size: " << this->get_window_size() << endl;
    return sstream.str();
  }
#endif
};

// used by Rx engine and Tx eng
struct TcpHeaderMeta {
  // big endian
  ap_uint<16> src_port;
  // big endian
  ap_uint<16> dest_port;
  ap_uint<32> seq_number;
  ap_uint<32> ack_number;
  ap_uint<16> win_size;
  ap_uint<4>  data_offset;  // data offset = tcp header length, in 32bit word
  ap_uint<1>  cwr;
  ap_uint<1>  ecn;
  ap_uint<1>  urg;
  ap_uint<1>  ack;
  ap_uint<1>  psh;
  ap_uint<1>  rst;
  ap_uint<1>  syn;
  ap_uint<1>  fin;
  // options field
  // receive window scale
  TcpSessionBufferScale win_scale;
  // not in the header field, but necessary
  ap_uint<16> payload_length;
  TcpHeaderMeta()
      : src_port(0)
      , dest_port(0)
      , seq_number(0)
      , ack_number(0)
      , win_size(0)
      , data_offset(0)
      , cwr(0)
      , ecn(0)
      , urg(0)
      , ack(0)
      , psh(0)
      , rst(0)
      , syn(0)
      , fin(0)
      , win_scale(0) {}
  // for generate next ack no
  ap_uint<16> get_payload_length() {
#pragma HLS INLINE
    return payload_length + fin + syn;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TCP Packet: \n" << std::hex;
    sstream << "Flag: " << (this->urg == 1 ? "U/" : "") << (this->ack == 1 ? "A/" : "")
            << (this->psh == 1 ? "P/" : "") << (this->rst == 1 ? "R/" : "")
            << (this->syn == 1 ? "S/" : "") << (this->fin == 1 ? "F" : "") << endl;
    sstream << "Source Port: " << SwapByte<16>(this->src_port) << endl;
    sstream << "Dest Port: " << SwapByte<16>(this->dest_port) << endl;
    sstream << "Seqence Number: " << this->seq_number << endl;
    sstream << "Acknowlegnement Number: " << this->ack_number << endl;
    sstream << "Window Size: " << this->win_size << endl;
    sstream << "Window Scale: " << this->win_scale << endl;
    sstream << "Data Offset: " << this->data_offset;
    return sstream.str();
  }
#endif
};

// TCP pseudo header + TCP header
struct TcpPseudoHeaderMeta {
  // not swapped
  IpAddr src_ip;
  // not swapped
  IpAddr dest_ip;
  // header only contains window scale option
  TcpHeaderMeta header;
  // all options in segment
  ap_uint<320> tcp_options;
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TCP/IP: \t" << std::hex;
    sstream << "Source IP: " << SwapByte<32>(src_ip) << "\t"
            << " Dest IP: " << SwapByte<32>(dest_ip) << endl;
    sstream << header.to_string() << endl;

    return sstream.str();
  }
#endif
};

#endif  //_TCP_HEADER_