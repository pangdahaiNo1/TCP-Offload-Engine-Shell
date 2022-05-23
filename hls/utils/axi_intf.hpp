#ifndef _AXI_INTF_HPP_
#define _AXI_INTF_HPP_
#include "ap_axi_sdata.h"
#include "ap_int.h"

#include <hls_stream.h>

#define NET_TDATA_WIDTH 512
#define NET_TID_WIDTH 16
#define NET_TUSER_WIDTH 16
#define NET_TDEST_WIDTH 4

#define NET_TDATA_BYTES 64
#define NET_TDATA_BYTES_LOG2 6

typedef ap_uint<NET_TDATA_WIDTH>     NetAXISData;
typedef ap_uint<NET_TID_WIDTH>       NetAXISId;
typedef ap_uint<NET_TUSER_WIDTH>     NetAXISUser;
typedef ap_uint<NET_TDEST_WIDTH>     NetAXISDest;
typedef ap_uint<NET_TDATA_WIDTH / 8> NetAXISKeep;

const NetAXISDest INVALID_TDEST = 0xF;

typedef ap_axiu<NET_TDATA_WIDTH, 0, 0, NET_TDEST_WIDTH> NetAXIS;

struct NetAXISWord {
  NetAXISData data;
  // NetAXISId   id;
  // NetAXISUser user;
  NetAXISDest dest;
  NetAXISKeep keep;
  ap_uint<1>  last;
  // NetAXISWord() : data(0), id(0), user(0), dest(0), keep(0), last(0) {}
  NetAXISWord() : data(0), dest(0), keep(0), last(1) {}
  NetAXISWord(NetAXISData data, NetAXISDest dest, NetAXISKeep keep, ap_uint<1> last)
      : data(data), dest(dest), keep(keep), last(last) {}
  NetAXISWord(const NetAXIS &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
    keep = net_axis.keep;
    last = net_axis.last;
  }
  NetAXIS to_net_axis() {
#pragma HLS INLINE
    NetAXIS new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    new_axis.keep = keep;
    new_axis.last = last;
    return new_axis;
  }
};

#endif  // _AXI_INTF_HPP_