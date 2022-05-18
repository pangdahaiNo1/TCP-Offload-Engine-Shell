
#ifndef _TCP_INTF_HPP_
#define _TCP_INTF_HPP_

#include "tcp_conn.hpp"
#include "toe_config.hpp"
#include "utils/axi_utils.hpp"

struct NetAXISListenPortReq {
  TcpPortNumber data;
  NetAXISDest   dest;
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Listen Port Req: " << data.to_string(16) << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

struct NetAXISListenPortRsp {
  ListenPortRsp data;
  NetAXISDest   dest;
  NetAXISListenPortRsp() {}

#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Listen Port Rsp: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

struct NetAXISAppReadReq {
  AppReadReq  data;
  NetAXISDest dest;
  NetAXISAppReadReq() {}
  NetAXISAppReadReq(AppReadReq req, NetAXISDest dest) : data(req), dest(dest) {}
};

struct NetAXISAppReadRsp {
  TcpSessionID data;
  NetAXISDest  dest;
  NetAXISAppReadRsp() {}
  NetAXISAppReadRsp(TcpSessionID rsp_data, NetAXISDest role_dest)
      : data(rsp_data), dest(role_dest) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "App read data rsp: SessionID: " << data.to_string(16) << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

struct NetAXISAppNotification {
  AppNotification data;
  NetAXISDest     dest;
  NetAXISAppNotification() {}
  NetAXISAppNotification(AppNotification notify, NetAXISDest dest) : data(notify), dest(dest) {}
};

// Tx app use ip:port to create session with other side
struct NetAXISAppOpenConnReq {
  IpPortTuple data;
  NetAXISDest dest;
};

// When a session established, notify Tx app, active open
struct NetAXISAppOpenConnRsp {
  OpenSessionStatus data;
  NetAXISDest       dest;
  NetAXISAppOpenConnRsp() {}
  NetAXISAppOpenConnRsp(OpenSessionStatus state, NetAXISDest dest) : data(state), dest(dest) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "OpenSessionSts: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

// When a session established, notify app new client info
struct NetAXISNewClientNotificaion {
  NewClientNotification data;
  NetAXISDest           dest;
  NetAXISNewClientNotificaion() {}
  NetAXISNewClientNotificaion(NewClientNotification status, NetAXISDest dest)
      : data(status), dest(dest) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "PassiveOpenSessionSts: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

// Tx app use session id to close session
struct NetAXISAppCloseConnReq {
  TcpSessionID data;
  NetAXISDest  dest;
  NetAXISAppCloseConnReq() {}
  NetAXISAppCloseConnReq(TcpSessionID id, NetAXISDest dest) : data(id), dest(dest) {}
};

// net app want to sending data, notify the tx app intf the data length and session id
struct NetAXISAppTransDataReq {
  TcpSessionBuffer data;  // length
  TcpSessionID     id;    // session id
  NetAXISDest      dest;  // role id
  NetAXISAppTransDataReq() {}
  NetAXISAppTransDataReq(TcpSessionBuffer length, TcpSessionID id, NetAXISDest dest)
      : data(length), id(id), dest(dest) {}
};

// tx app interface check the net app trans data request, and response to net app
struct NetAXISAppTransDataRsp {
  AppTransDataRsp data;
  NetAXISDest     dest;
  NetAXISAppTransDataRsp() {}
  NetAXISAppTransDataRsp(AppTransDataRsp rsp, NetAXISDest role_id) : data(rsp), dest(role_id) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TransRsp " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

/***
 * @brief DataMoverCmd
 * Refenrence:
 * https://www.xilinx.com/support/documentation/ip_documentation/axi_datamover/v5_1/pg022_axi_datamover.pdf
 * Table 2‐7: Command Word Description
 */
struct DataMoverCmd {
  ap_uint<23> bbt;    /// Bytes to Transfer, 23-bits field, from 1 to 8388607 Bytes
  ap_uint<1>  type;   /// 1 enables INCR, 0 enables FIXED addr AXI4 trans
  ap_uint<6>  dsa;    /// DRE Stream Alignment, not used
  ap_uint<1>  eof;    /// End of Frame, when it is set, MM2S assert TLAST on the last Data
  ap_uint<1>  drr;    /// DRE ReAlignment Request
  ap_uint<40> saddr;  /// Start Address
  ap_uint<4>  tag;
  ap_uint<4>  rsvd;
  DataMoverCmd() {}
  DataMoverCmd(ap_uint<32> addr, ap_uint<23> len)
      : bbt(len), type(1), dsa(0), eof(1), drr(1), tag(0), rsvd(0) {
    saddr = addr(31, 0) + TCP_DDR_BASE_ADDR;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "BTT: " << bbt.to_string(16) << "\t";
    sstream << "Addr: " << saddr.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

struct DataMoverStatus {
  ap_uint<4> tag;
  ap_uint<1> interr;
  ap_uint<1> decerr;
  ap_uint<1> slverr;
  ap_uint<1> okay;
};

#endif