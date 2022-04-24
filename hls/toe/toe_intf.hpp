
#ifndef _TCP_INTF_HPP_
#define _TCP_INTF_HPP_

#include "tcp_conn.hpp"
#include "toe_config.hpp"
#include "utils/axi_utils.hpp"

struct NetAXISListenPortReq {
  TcpPortNumber data;
  NetAXISDest   dest;
};

struct NetAXISListenPortRsp {
  ListenPortRsp data;
  NetAXISDest   dest;
};

struct NetAXISAppReadReq {
  AppReadReq  data;
  NetAXISDest dest;
};

struct NetAXISAppReadRsp {
  TcpSessionID data;
  NetAXISDest  dest;
  NetAXISAppReadRsp() {}
  NetAXISAppReadRsp(TcpSessionID rsp_data, NetAXISDest role_dest)
      : data(rsp_data), dest(role_dest) {}
};

struct DataMoverCmd {
  ap_uint<23> bbt;
  ap_uint<1>  type;
  ap_uint<6>  dsa;
  ap_uint<1>  eof;
  ap_uint<1>  drr;
  ap_uint<40> saddr;
  ap_uint<4>  tag;
  ap_uint<4>  rsvd;
  DataMoverCmd() {}
  DataMoverCmd(ap_uint<32> addr, ap_uint<16> len)
      : bbt(len), type(1), dsa(0), eof(1), drr(1), tag(0), rsvd(0) {
    saddr = addr + TCP_DDR_BASE_ADDR;
  }
};

struct DataMoverStatus {
  ap_uint<4> tag;
  ap_uint<1> interr;
  ap_uint<1> decerr;
  ap_uint<1> slverr;
  ap_uint<1> okay;
};

#endif