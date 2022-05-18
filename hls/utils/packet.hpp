#ifndef PACKET_HPP
#define PACKET_HPP

#include "axi_utils.hpp"
#include "stdint.h"

#include <iostream>

using namespace hls;

template <int DATA_WIDTH, int HEADER_WIDTH>
class PacketHeader {
public:
  bool                  ready;
  uint16_t              idx;
  ap_uint<HEADER_WIDTH> header;

public:
  PacketHeader() : ready(false), idx(0) {}
  PacketHeader &operator=(const PacketHeader &other) {
    ready  = other.ready;
    idx    = other.idx;
    header = other.header;
    return *this;
  }

  void FromWord(const ap_uint<DATA_WIDTH> &w) {
    if (ready)
      return;

    if (idx * DATA_WIDTH + DATA_WIDTH < HEADER_WIDTH) {
      header(idx * DATA_WIDTH + DATA_WIDTH - 1, idx * DATA_WIDTH) = w;
    } else {
      header(HEADER_WIDTH - 1, idx * DATA_WIDTH) = w;
      ready                                      = true;
    }
    idx++;
  }
  // transform header into a AXIStream word
  ap_uint<8> IntoWord(ap_uint<DATA_WIDTH> &w) {
    if ((idx + 1) * DATA_WIDTH <= HEADER_WIDTH) {
      w = header(((idx + 1) * DATA_WIDTH) - 1, idx * DATA_WIDTH);
      idx++;
      return ((HEADER_WIDTH - (idx * DATA_WIDTH)) / 8);
    } else if (idx * DATA_WIDTH < HEADER_WIDTH) {
      w((HEADER_WIDTH % DATA_WIDTH) - 1, 0) = header(HEADER_WIDTH - 1, idx * DATA_WIDTH);
      idx++;
      return 0;  //(HEADER_WIDTH - (idx*W));
    }
    return 0;
  }
  void                  set_raw_header(ap_uint<HEADER_WIDTH> h) { header = h; }
  ap_uint<HEADER_WIDTH> get_raw_header() { return header; }
  bool                  IsReady() { return ready; }

  void Clear() {
#pragma HLS pipeline II = 1
    // header = 0;
    ready = false;
    idx   = 0;
  }
};

#endif
