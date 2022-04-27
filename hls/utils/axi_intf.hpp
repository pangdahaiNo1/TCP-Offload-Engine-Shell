#ifndef _AXI_INTF_HPP_
#define _AXI_INTF_HPP_
#include "ap_int.h"

#include <hls_stream.h>

#define NET_TDATA_WIDTH 512
#define NET_TID_WIDTH 16
#define NET_TUSER_WIDTH 16
#define NET_TDEST_WIDTH 4

typedef ap_uint<NET_TDATA_WIDTH>     NetAXISData;
typedef ap_uint<NET_TID_WIDTH>       NetAXISId;
typedef ap_uint<NET_TUSER_WIDTH>     NetAXISUser;
typedef ap_uint<NET_TDEST_WIDTH>     NetAXISDest;
typedef ap_uint<NET_TDATA_WIDTH / 8> NetAXISKeep;

const NetAXISDest INVALID_TDEST = 0xF;
struct NetAXIS {
  NetAXISData data;
  NetAXISId   id;
  NetAXISUser user;
  NetAXISDest dest;
  NetAXISKeep keep;
  ap_uint<1>  last;
  NetAXIS() : data(0), id(0), user(0), dest(0), keep(0), last(0) {}
};

#endif  // _AXI_INTF_HPP_