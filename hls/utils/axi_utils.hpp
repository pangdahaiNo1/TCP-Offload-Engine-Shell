#ifndef _AXI_UTILS_HPP_
#define _AXI_UTILS_HPP_
#include "ap_int.h"
#include "axi_intf.hpp"

#include "stdint.h"
#include <hls_stream.h>
#include <iostream>

using std::cout;
using std::dec;
using std::endl;
using std::hex;

using hls::stream;
// change the endian of parameter
template <int BITWIDTH>
ap_uint<BITWIDTH> SwapByte(const ap_uint<BITWIDTH> &w) {
#pragma HLS       INLINE
  ap_uint<BITWIDTH> temp;
  for (int i = 0; i < BITWIDTH / 8; i++) {
#pragma HLS UNROLL
    temp(i * 8 + 7, i * 8) = w(BITWIDTH - (i * 8) - 1, BITWIDTH - (i * 8) - 8);
  }
  return temp;
}

struct SubChecksum {
  ap_uint<17> sum[NET_TDATA_WIDTH / 16];

#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    for (int i = 0; i < NET_TDATA_WIDTH / 16; i++) {
      sstream << sum[i] << " ";
    }
    return sstream.str();
  }
#endif
};

void ComputeSubChecksum(stream<NetAXIS> &    pkt_in,
                        stream<NetAXIS> &    pkt_out,
                        stream<SubChecksum> &sub_checksum);

void CheckChecksum(stream<SubChecksum> &sub_checksum, stream<bool> &valid_pkt_out);

#endif