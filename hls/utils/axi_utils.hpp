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

using hls::axis;
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

/**
 * Generic stream switch function, only support two TDEST: tdest 0x0 and tdest 0x1;
 */
template <typename T>
void                 AxiStreamSwitch(stream<axis<T, 0, 0, NET_TDEST_WIDTH> > &in,
                                     stream<axis<T, 0, 0, NET_TDEST_WIDTH> > &out0,
                                     stream<axis<T, 0, 0, NET_TDEST_WIDTH> > &out1,
                                     NetAXISDest                              out0_tdest,
                                     NetAXISDest                              out1_tdest) {
#pragma HLS PIPELINE II = 1

  if (!in.empty()) {
    axis<T, 0, 0, NET_TDEST_WIDTH> cur_word = in.read();
    if (cur_word.dest == out0_tdest) {
      out0.write(cur_word);
    } else if (cur_word.dest == out1_tdest) {
      out1.write(cur_word);
    }
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

/**
 * The template parameter is used to generate two different instances,
 * it is not used in function
 * in rx_engine @p is_rx = 1,
 * in tx_engine @p is_rx = 0
 */
template <int is_rx>
void ComputeSubChecksum(stream<NetAXISWord> &pkt_in, stream<SubChecksum> &sub_checksum) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off
  static SubChecksum tcp_checksums;
  if (!pkt_in.empty()) {
    NetAXISWord cur_word = pkt_in.read();
    for (int i = 0; i < NET_TDATA_WIDTH / 16; i++) {
#pragma HLS UNROLL
      ap_uint<16> temp;
      if (cur_word.keep(i * 2 + 1, i * 2) == 0x3) {
        temp(7, 0)  = cur_word.data(i * 16 + 15, i * 16 + 8);
        temp(15, 8) = cur_word.data(i * 16 + 7, i * 16);
        tcp_checksums.sum[i] += temp;
        tcp_checksums.sum[i] = (tcp_checksums.sum[i] + (tcp_checksums.sum[i] >> 16)) & 0xFFFF;
      } else if (cur_word.keep[i * 2] == 0x1) {
        temp(7, 0)  = 0;
        temp(15, 8) = cur_word.data(i * 16 + 7, i * 16);
        tcp_checksums.sum[i] += temp;
        tcp_checksums.sum[i] = (tcp_checksums.sum[i] + (tcp_checksums.sum[i] >> 16)) & 0xFFFF;
      }
    }
    if (cur_word.last == 1) {
      sub_checksum.write(tcp_checksums);
      for (int i = 0; i < NET_TDATA_WIDTH / 16; i++) {
#pragma HLS UNROLL
        tcp_checksums.sum[i] = 0;
      }
    }
  }
}

void CheckChecksum(stream<SubChecksum> &sub_checksum, stream<ap_uint<16> > &valid_pkt_out);

ap_uint<64> DataLengthToAxisKeep(ap_uint<6> length);

void ConcatTwoWords(const NetAXISWord &cur_word,
                    const NetAXISWord &prev_word,
                    const ap_uint<6>  &byte_offset,
                    NetAXISWord       &send_word);

void MergeTwoWordsHead(const NetAXISWord &cur_word,
                       const NetAXISWord &prev_word,
                       ap_uint<6>         byte_offset,
                       NetAXISWord       &send_word);
#endif