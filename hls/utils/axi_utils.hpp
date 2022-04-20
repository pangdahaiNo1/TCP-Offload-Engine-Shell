#ifndef _ASI_UTILS_
#define _ASI_UTILS_
#include "ap_int.h"

#include <hls_stream.h>
#include <iostream>
using std::cout;
using std::dec;
using std::endl;
using std::hex;

#define NET_TDATA_WIDTH 512
#define NET_TID_WIDTH 16
#define NET_TUSER_WIDTH 16
#define NET_TDEST_WIDTH 4

typedef ap_uint<NET_TDATA_WIDTH>     NetAXISData;
typedef ap_uint<NET_TID_WIDTH>       NetAXISId;
typedef ap_uint<NET_TUSER_WIDTH>     NetAXISUser;
typedef ap_uint<NET_TDEST_WIDTH>     NetAXISDest;
typedef ap_uint<NET_TDATA_WIDTH / 8> NetAXISKeep;

struct NetAXIS {
  NetAXISData data;
  NetAXISId   id;
  NetAXISUser user;
  NetAXISDest dest;
  NetAXISKeep keep;
  ap_uint<1>  last;
};

// change the endian of parameter
template <int D>
ap_uint<D> SwapByte(const ap_uint<D> &w) {
  ap_uint<D> temp;
  for (int i = 0; i < D / 8; i++) {
#pragma HLS UNROLL
    temp(i * 8 + 7, i * 8) = w(D - (i * 8) - 1, D - (i * 8) - 8);
  }
  return temp;
}

#endif