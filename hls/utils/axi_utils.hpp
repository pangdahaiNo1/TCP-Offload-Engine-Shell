#ifndef _AXI_UTILS_HPP_
#define _AXI_UTILS_HPP_
#define __gmp_const const
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

/**
 * Generic stream merger function
 */
template <typename T>
void                 AxiStreamMerger(stream<T> &in1, stream<T> &in2, stream<T> &out) {
#pragma HLS PIPELINE II = 1

  if (!in1.empty()) {
    out.write(in1.read());
  } else if (!in2.empty()) {
    out.write(in2.read());
  }
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

NetAXIS NewNetAXIS(NetAXISData data, NetAXISDest dest, NetAXISKeep keep, ap_uint<1> last);

void ComputeTxSubChecksum(stream<NetAXISWord> &pkt_in, stream<SubChecksum> &sub_checksum);
void ComputeRxSubChecksum(stream<NetAXISWord> &pkt_in, stream<SubChecksum> &sub_checksum);

void CheckChecksum(stream<SubChecksum> &sub_checksum, stream<ap_uint<16> > &valid_pkt_out);

ap_uint<64> DataLengthToAxisKeep(ap_uint<6> length);

void ConcatTwoWords(const NetAXISWord &cur_word,
                    const NetAXISWord &prev_word,
                    const ap_uint<6> & byte_offset,
                    NetAXISWord &      send_word);

void MergeTwoWordsHead(const NetAXISWord &cur_word,
                       const NetAXISWord &prev_word,
                       ap_uint<6>         byte_offset,
                       NetAXISWord &      send_word);
#endif