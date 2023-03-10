#pragma once

#include "toe/toe_config.hpp"
#include "utils/axi_utils.hpp"
typedef ap_uint<16> TcpSessionID;
// only use in little endian
typedef ap_uint<16>                TcpPortNumber;
typedef ap_uint<WINDOW_SCALE_BITS> TcpSessionBufferScale;
typedef ap_uint<WINDOW_BITS>       TcpSessionBuffer;
typedef ap_uint<32>                IpAddr;

struct FourTuple;
struct ThreeTuple;
#if MULTI_IP_ADDR
using CamTuple = FourTuple;
#else
using CamTuple                      = ThreeTuple;
#endif

#define CLOCK_PERIOD 0.003103
#ifndef __SYNTHESIS__
static const ap_uint<32> TIME_64us  = 1;  // used for ACK_DELAY
static const ap_uint<32> TIME_128us = 2;
static const ap_uint<32> TIME_1ms   = 1;
static const ap_uint<32> TIME_5ms   = 5;
static const ap_uint<32> TIME_25ms  = 25;
static const ap_uint<32> TIME_50ms  = 50;
static const ap_uint<32> TIME_100ms = 100;
static const ap_uint<32> TIME_250ms = 250;
static const ap_uint<32> TIME_500ms = 500;
static const ap_uint<32> TIME_1s    = 1;
static const ap_uint<32> TIME_5s    = 5;
static const ap_uint<32> TIME_7s    = 7;
static const ap_uint<32> TIME_10s   = 10;
static const ap_uint<32> TIME_15s   = 15;
static const ap_uint<32> TIME_20s   = 20;
static const ap_uint<32> TIME_30s   = 30;
static const ap_uint<32> TIME_60s   = 60;
static const ap_uint<32> TIME_120s  = 120;
#else
static const ap_uint<32> TIME_64us  = (64.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_128us = (128.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_1ms   = (1000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_5ms   = (5000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_25ms  = (25000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_50ms  = (50000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_100ms = (100000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_250ms = (250000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_500ms = (500000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_1s    = (1000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_5s    = (5000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_7s    = (7000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_10s   = (10000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_15s   = (15000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_20s   = (20000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_30s   = (30000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_60s   = (60000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
static const ap_uint<32> TIME_120s  = (120000000.0 / CLOCK_PERIOD / TCP_MAX_SESSIONS) + 1;
#endif

// TCP STATE
/*
 * There is no explicit LISTEN state
 * CLOSE-WAIT state is not used, since the FIN is sent out immediately after we receive a FIN, the
 * application is simply notified
 * FIN_WAIT_2 is also unused
 * LISTEN is merged into CLOSED
 */
enum SessionState {
  CLOSED,
  SYN_SENT,
  SYN_RECEIVED,
  ESTABLISHED,
  FIN_WAIT_1,
  FIN_WAIT_2,
  CLOSING,
  TIME_WAIT,
  LAST_ACK
};
#ifndef __SYNTHESIS__
INLINE std::string state_to_string(SessionState state) {
  switch (state) {
    case CLOSED:
      return "CLOSED";
    case SYN_SENT:
      return "SYN_SENT";
    case SYN_RECEIVED:
      return "SYN_RECEIVED";
    case ESTABLISHED:
      return "ESTABLISHED";
    case FIN_WAIT_1:
      return "FIN_WAIT_1";
    case FIN_WAIT_2:
      return "FIN_WAIT_2";
    case CLOSING:
      return "CLOSING";
    case TIME_WAIT:
      return "TIME_WAIT";
    case LAST_ACK:
      return "LAST_ACK";
    default:
      return "[Unknown Session State]";
  }
}
#else
INLINE char             *state_to_string(SessionState state) {
  _AP_UNUSED_PARAM(state);
  return 0;
}
#endif

struct PortTableEntry {
  bool        is_open;
  NetAXISDest role_id;
};
// TOE interface
typedef hls::axis<TcpSessionID, 0, 0, NET_TDEST_WIDTH> NetAXISListenPortReq;

// for inner connection
struct ListenPortReq {
  TcpPortNumber data;
  NetAXISDest   dest;
  ListenPortReq() {}
  ListenPortReq(const NetAXISListenPortReq &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISListenPortReq to_net_axis() {
#pragma HLS INLINE
    NetAXISListenPortReq new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "Data: Port: " << data.to_string(16) << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// for inner connection
struct ListenPortRspNoTDEST {
  ap_uint<16> port_number;
  bool        open_successfully;
  bool        wrong_port_number;
  // already open may open by current role, or other role
  bool already_open;
  ListenPortRspNoTDEST() {}
  ListenPortRspNoTDEST(bool          open_successfully,
                       bool          wrong_port_number,
                       bool          already_open,
                       TcpPortNumber port_number)
      : open_successfully(open_successfully)
      , wrong_port_number(wrong_port_number)
      , already_open(already_open)
      , port_number(port_number) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Port number: " << std::dec << port_number.to_string(16) << " \t"
            << "Opened successfully? " << open_successfully << " \t"
            << "Wrong number? " << wrong_port_number << " \t"
            << "Already open? " << already_open;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// TOE interface
typedef hls::axis<ListenPortRspNoTDEST, 0, 0, NET_TDEST_WIDTH> NetAXISListenPortRsp;
// inner connection
struct ListenPortRsp {
  ListenPortRspNoTDEST data;
  NetAXISDest          dest;
  ListenPortRsp() {}
  ListenPortRsp(const NetAXISListenPortRsp &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISListenPortRsp to_net_axis() {
#pragma HLS INLINE
    NetAXISListenPortRsp new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "Listen Port Rsp: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct PtableToRxEngRsp {
  bool        is_open;
  NetAXISDest role_id;
  PtableToRxEngRsp() : is_open(false), role_id(0) {}
  PtableToRxEngRsp(bool is_open, NetAXISDest dest) : is_open(is_open), role_id(dest) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "Opened?: " << (this->is_open ? "Yes." : "No.") << "\t";
    sstream << "Role ID: " << this->role_id;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct OpenConnRspNoTDEST {
  TcpSessionID session_id;
  bool         success;
  OpenConnRspNoTDEST() : session_id(0), success(0) {}
  OpenConnRspNoTDEST(TcpSessionID id, bool success) : session_id(id), success(success) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "Session ID: " << session_id.to_string(16) << "\t";
    sstream << "Success: " << (success ? "Yes." : "No.") << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// When a session established, notify Tx app, active open
typedef hls::axis<OpenConnRspNoTDEST, 0, 0, NET_TDEST_WIDTH> NetAXISAppOpenConnRsp;

struct AppOpenConnRsp {
  OpenConnRspNoTDEST data;
  NetAXISDest        dest;
  AppOpenConnRsp() : data(), dest(0) {}
  AppOpenConnRsp(OpenConnRspNoTDEST rsp, NetAXISDest dest) : data(rsp), dest(dest) {}
  AppOpenConnRsp(const NetAXISAppOpenConnRsp &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISAppOpenConnRsp to_net_axis() {
#pragma HLS INLINE
    NetAXISAppOpenConnRsp new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "OpenSessionSts: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct NewClientNotificationNoTDEST {
  TcpSessionID session_id;  // Tells to the tx application the ID
  /*
   * not used for the time being, tells to the app the negotiated buffer size.
   * 65536 * (buffersize+1)
   */
  ap_uint<8>  buffersize;
  ap_uint<11> max_segment_size;  // max 2048
  /*
   * Tells the application that the design uses TCP_NODELAY which means that the
   * data transfers has to be done in chunks of a maximum size
   */
  bool tcp_nodelay;
  bool tcp_buffer_is_empty;  // Tells when the Tx buffer is empty. It is also used as a
                             // way to indicate that a new connection was opened.
  NewClientNotificationNoTDEST()
      : session_id(0), max_segment_size(0), tcp_nodelay(0), tcp_buffer_is_empty(0) {}
  NewClientNotificationNoTDEST(
      TcpSessionID id, ap_uint<8> buffersize, ap_uint<11> mss, bool tcp_nodelay, bool buf_empty)
      : session_id(id)
      , buffersize(buffersize)
      , max_segment_size(mss)
      , tcp_nodelay(tcp_nodelay)
      , tcp_buffer_is_empty(buf_empty) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Session ID: " << session_id.to_string(16) << "\t";
    sstream << "MaxSegSize: " << max_segment_size.to_string(16) << "\t";
    sstream << "TcpNoDelay?: " << tcp_nodelay << "\t";
    sstream << "TcpBufferEmpty?: " << tcp_buffer_is_empty;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// When a session established, notify app new client info
typedef hls::axis<NewClientNotificationNoTDEST, 0, 0, NET_TDEST_WIDTH> NetAXISNewClientNotification;

struct NewClientNotification {
  NewClientNotificationNoTDEST data;
  NetAXISDest                  dest;
  NewClientNotification() {}
  NewClientNotification(NewClientNotificationNoTDEST data, NetAXISDest dest)
      : data(data), dest(dest) {}
  NewClientNotification(const NetAXISNewClientNotification &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISNewClientNotification to_net_axis() {
#pragma HLS INLINE
    NetAXISNewClientNotification new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "PassiveOpenSessionSts: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// Tx app use session id to close session
typedef hls::axis<TcpSessionID, 0, 0, NET_TDEST_WIDTH> NetAXISAppCloseConnReq;

struct AppCloseConnReq {
  TcpSessionID data;
  NetAXISDest  dest;
  AppCloseConnReq() : data(0), dest(0) {}
  AppCloseConnReq(TcpSessionID id, NetAXISDest dest) : data(id), dest(dest) {}
  AppCloseConnReq(const NetAXISAppCloseConnReq &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISAppCloseConnReq to_net_axis() {
#pragma HLS INLINE
    NetAXISAppCloseConnReq new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << data.to_string(16) << "\t";
    sstream << "TDEST: " << dest.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// net app want to sending data, notify the tx app intf the data length and session id
// data = length, id = session id, dest = role id
typedef hls::axis<TcpSessionBuffer, 0, 16, NET_TDEST_WIDTH> NetAXISAppTransDataReq;

struct AppTransDataReq {
  TcpSessionBuffer data;  // length
  TcpSessionID     id;    // session id
  NetAXISDest      dest;  // role id
  AppTransDataReq() {}
  AppTransDataReq(TcpSessionBuffer length, TcpSessionID id, NetAXISDest dest)
      : data(length), id(id), dest(dest) {}

  AppTransDataReq(const NetAXISAppTransDataReq &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    id   = net_axis.id;
    dest = net_axis.dest;
  }
  NetAXISAppTransDataReq to_net_axis() {
#pragma HLS INLINE
    NetAXISAppTransDataReq new_axis;
    new_axis.data = data;
    new_axis.id   = id;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Length: " << data.to_string(16) << "\t";
    sstream << "SessionID: " << id.to_string(16) << "\t";
    sstream << "TDEST: " << dest.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

enum TxAppTransDataError { NO_ERROR, ERROR_NOCONNECTION, ERROR_NOSPACE, ERROR_WINDOW };
struct AppTransDataRspNoTDEST {
  TxAppTransDataError error;
  TcpSessionBuffer    remaining_space;
  ap_uint<16>         length;
  AppTransDataRspNoTDEST() {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TransError: " << error << "\t";
    sstream << "RemainSpace: " << remaining_space.to_string(16) << "\t";
    sstream << "TransLength: " << length.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// tx app interface check the net app trans data request, and response to net app
typedef hls::axis<AppTransDataRspNoTDEST, 0, 0, NET_TDEST_WIDTH> NetAXISAppTransDataRsp;

struct AppTransDataRsp {
  AppTransDataRspNoTDEST data;
  NetAXISDest            dest;
  AppTransDataRsp() {}
  AppTransDataRsp(AppTransDataRspNoTDEST rsp, NetAXISDest role_id) : data(rsp), dest(role_id) {}

  AppTransDataRsp(const NetAXISAppTransDataRsp &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISAppTransDataRsp to_axis() {
    NetAXISAppTransDataRsp new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TransRsp " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};
// ip + port are all in big endian
struct FourTuple {
  // big endian
  IpAddr src_ip_addr;
  IpAddr dst_ip_addr;
  // big endian
  TcpPortNumber src_tcp_port;
  TcpPortNumber dst_tcp_port;
  FourTuple() : src_ip_addr(0), dst_ip_addr(0), src_tcp_port(0), dst_tcp_port(0) {}
  FourTuple(IpAddr        src_ip_addr,
            IpAddr        dst_ip_addr,
            TcpPortNumber src_tcp_port,
            TcpPortNumber dst_tcp_port)
      : src_ip_addr(src_ip_addr)
      , dst_ip_addr(dst_ip_addr)
      , src_tcp_port(src_tcp_port)
      , dst_tcp_port(dst_tcp_port) {}
  bool operator<(const FourTuple &other) const {
    if (src_ip_addr < other.src_ip_addr) {
      return true;
    } else if (src_ip_addr == other.src_ip_addr) {
      if (src_tcp_port < other.src_tcp_port) {
        return true;
      } else if (src_tcp_port == other.src_tcp_port) {
        if (dst_ip_addr < other.dst_ip_addr) {
          return true;
        } else if (dst_ip_addr == other.dst_ip_addr) {
          if (dst_tcp_port < other.dst_tcp_port) {
            return true;
          }
        }
      }
    }
    return false;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Src IP:Port " << SwapByte(src_ip_addr).to_string(16) << ":"
            << SwapByte(src_tcp_port).to_string(16) << "\t";
    sstream << "Dst IP:Port " << SwapByte(dst_ip_addr).to_string(16) << ":"
            << SwapByte(dst_tcp_port).to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// ip + port are all in big endian
struct OpenConnTuple {
#if MULTI_IP_ADDR
  IpAddr my_ip_addr;
#else
#endif
  // remote IP:Addr
  // big endian
  IpAddr ip_addr;
  // big endian
  TcpPortNumber tcp_port;
  OpenConnTuple() {}
#if MULTI_IP_ADDR
  OpenConnTuple(IpAddr my_ip, IpAddr ip, TcpPortNumber port)
      : my_ip_addr(my_ip), ip_addr(ip), tcp_port(port) {}
#else
  OpenConnTuple(IpAddr ip, TcpPortNumber port) : ip_addr(ip), tcp_port(port) {}
#endif
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
#if MULTI_IP_ADDR
    sstream << "my IP " << SwapByte(my_ip_addr).to_string(16) << "\t";
#else
#endif
    sstream << "remote IP:Port " << SwapByte(ip_addr).to_string(16) << ":"
            << SwapByte(tcp_port).to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct StateTableReq {
  TcpSessionID session_id;
  SessionState state;
  ap_uint<1>   write;
  StateTableReq() : session_id(0), state(CLOSED), write(0) {}
  // Read queset
  StateTableReq(TcpSessionID id) : session_id(id), state(CLOSED), write(0) {}
  // write quest
  StateTableReq(TcpSessionID id, SessionState state, ap_uint<1> write)
      : session_id(id), state(state), write(write) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "State: " << state_to_string(state) << "\t";
    sstream << "Write?: " << write << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// RX SAR Table
/** @ingroup rx_sar_table
 *  @ingroup rx_engine
 *  @ingroup tx_engine
 *  This struct defines an entry of the @ref rx_sar_table
 */

/*	A gap caused by out-of-order packet:
 * out of order is not supported yet
 *
 *	                v- app_read     v- recvd    v- offset        v- head
 *	|  <- undef ->  |  <buffered>  |   <gap>   |  <pre-mature>  |  <- undef ->  |
 */
struct RxSarTableEntry {
  // received seq number
  ap_uint<32>           recvd;
  TcpSessionBuffer      app_read;
  TcpSessionBufferScale win_shift;
  // TcpSessionBuffer      offset;
  // TcpSessionBuffer      head;
  // bool                  gap;
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Recved Seq No: " << (recvd).to_string(16) << "\t";
    sstream << "App read: " << app_read.to_string(16) << "\t";
    sstream << "WinScaleOpt: " << win_shift.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct RxSarLookupRsp {
  ap_uint<32>           recvd;
  TcpSessionBufferScale win_shift;
  // win_size is 16 bit, in tcp header win
  // usually it is actual_win_size >> win_shift
  ap_uint<16> win_size;
  RxSarLookupRsp() {}
  RxSarLookupRsp(ap_uint<32> recvd, TcpSessionBufferScale win_shift)
      : recvd(recvd), win_size(0), win_shift(win_shift) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Recved Seq No: " << recvd.to_string(16) << "\t";
    sstream << "Win Size: " << win_size.to_string(16) << "\t";
    sstream << "WinScaleOpt: " << win_shift.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// Lookup or Update Rx SAR table when received new segment, or want to send new segment
struct RxEngToRxSarReq {
  ap_uint<32>           recvd;
  TcpSessionID          session_id;
  TcpSessionBufferScale win_shift;
  ap_uint<1>            write;
  ap_uint<1>            init;
  // bool                  gap;
  // TcpSessionBuffer head;
  // TcpSessionBuffer offset;

  RxEngToRxSarReq() : recvd(0), session_id(0), win_shift(0), write(0), init(0) {}
  RxEngToRxSarReq(TcpSessionID id) : session_id(id), recvd(0), write(0), init(0) {}
  RxEngToRxSarReq(TcpSessionID id, ap_uint<32> recvd, ap_uint<1> write)
      : session_id(id), recvd(recvd), write(write), init(0) {}
  RxEngToRxSarReq(TcpSessionID id, ap_uint<32> recvd, ap_uint<1> write, ap_uint<1> init)
      : session_id(id), recvd(recvd), write(write), init(init) {}
  RxEngToRxSarReq(
      TcpSessionID id, ap_uint<32> recvd, ap_uint<1> write, ap_uint<1> init, ap_uint<4> wsopt)
      : session_id(id), recvd(recvd), write(write), init(init), win_shift(wsopt) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Recved Seq No: " << recvd.to_string(16) << "\t";
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "WinScaleOpt: " << win_shift.to_string(16) << "\t";
    sstream << "Write?: " << write << "\t";
    sstream << "Init?: " << init;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// Update Rx SAR table when Rx APP has been read new data, or want to read new data
struct RxSarAppReqRsp {
  TcpSessionID     session_id;
  TcpSessionBuffer app_read;
  ap_uint<1>       write;
  RxSarAppReqRsp() : session_id(0), app_read(0), write(0) {}
  RxSarAppReqRsp(TcpSessionID id) : session_id(id), app_read(0), write(0) {}
  RxSarAppReqRsp(TcpSessionID id, TcpSessionBuffer appd)
      : session_id(id), app_read(appd), write(1) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "App Read " << app_read.to_string(16) << "\t";
    sstream << "Write?: " << write;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// Tx SAR Table
/** @ingroup tx_sar_table
 *  @ingroup rx_engine
 *  @ingroup tx_engine
 *  This struct defines an entry of the @ref tx_sar_table
 */
/*
 *	                v-acked             v-tx'ed/notacked  v-app_written/mempt
 *	|  <- undef ->  |  <tx'ed not ack>  |   <not tx'ed>   |  <- undef ->  |
 */
struct TxSarTableEntry {
  ap_uint<32> ackd;
  ap_uint<32> not_ackd;
  // in received tcp segment header
  ap_uint<16>           recv_window;
  TcpSessionBuffer      app_written;
  TcpSessionBuffer      cong_window;
  TcpSessionBuffer      slowstart_threshold;
  TcpSessionBufferScale win_shift;
  ap_uint<2>            retrans_count;
  bool                  fast_retrans;
  bool                  fin_is_ready;
  bool                  fin_is_sent;
  bool                  use_cong_window;
};

// Rx engine to Tx Sar update req, from new arrived ack number
struct RxEngToTxSarReq {
  TcpSessionID session_id;
  ap_uint<32>  ackd;
  ap_uint<16>  recv_window;
  ap_uint<2>   retrans_count;
  bool         fast_retrans;
  // win shift
  TcpSessionBufferScale win_shift;
  bool                  win_shift_write;
  TcpSessionBuffer      cong_window;
  ap_uint<1>            write;
  RxEngToTxSarReq()
      : session_id(0)
      , ackd(0)
      , recv_window(0)
      , retrans_count(0)
      , fast_retrans(0)
      , win_shift(0)
      , win_shift_write(0)
      , cong_window(0)
      , write(0) {}
  // lookup req
  RxEngToTxSarReq(TcpSessionID id)
      : session_id(id)
      , ackd(0)
      , recv_window(0)
      , cong_window(0)
      , retrans_count(0)
      , fast_retrans(false)
      , write(0) {}
  // update req
  RxEngToTxSarReq(TcpSessionID         id,
                  ap_uint<32>          ackd,
                  ap_uint<16>          recv_win,
                  ap_uint<WINDOW_BITS> cong_win,
                  ap_uint<2>           count,
                  bool                 fast_retrans)
      : session_id(id)
      , ackd(ackd)
      , recv_window(recv_win)
      , cong_window(cong_win)
      , retrans_count(count)
      , fast_retrans(fast_retrans)
      , write(1) {}
  RxEngToTxSarReq(TcpSessionID          id,
                  ap_uint<32>           ackd,
                  ap_uint<16>           recv_win,
                  TcpSessionBuffer      cong_win,
                  ap_uint<2>            count,
                  bool                  fast_retrans,
                  TcpSessionBufferScale win_shift)
      : session_id(id)
      , ackd(ackd)
      , recv_window(recv_win)
      , cong_window(cong_win)
      , retrans_count(count)
      , fast_retrans(fast_retrans)
      , win_shift(win_shift)
      , write(1)
      , win_shift_write(0) {}
  RxEngToTxSarReq(TcpSessionID         id,
                  ap_uint<32>          ackd,
                  ap_uint<16>          recv_win,
                  ap_uint<WINDOW_BITS> cong_win,
                  ap_uint<2>           count,
                  bool                 fastRetransmitted,
                  bool                 win_shift_write,
                  ap_uint<4>           win_shift)
      : session_id(id)
      , ackd(ackd)
      , recv_window(recv_win)
      , cong_window(cong_win)
      , retrans_count(count)
      , fast_retrans(fastRetransmitted)
      , write(1)
      , win_shift_write(win_shift_write)
      , win_shift(win_shift) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << '\t';
    sstream << "AckNo: " << ackd.to_string(16) << '\t';
    sstream << "RecvWin: " << recv_window.to_string(16) << '\t';
    sstream << "RetransCnt: " << retrans_count.to_string() << '\t';
    sstream << "WinShift: " << win_shift.to_string(16) << '\t';
    sstream << "WinShiftUpd?: " << win_shift_write << '\t';
    sstream << "CongWin: " << cong_window.to_string(16) << '\t';
    sstream << "Upd?: " << write;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct TxSarToRxEngRsp {
  ap_uint<32>           perv_ack;
  ap_uint<32>           next_byte;
  TcpSessionBuffer      cong_window;
  TcpSessionBuffer      slowstart_threshold;
  ap_uint<2>            retrans_count;
  bool                  fast_retrans;
  TcpSessionBufferScale win_shift;
  TxSarToRxEngRsp()
      : perv_ack(0)
      , next_byte(0)
      , cong_window(0)
      , slowstart_threshold(0)
      , retrans_count(0)
      , fast_retrans(0)
      , win_shift(0) {}
  TxSarToRxEngRsp(ap_uint<32>           ack,
                  ap_uint<32>           next,
                  TcpSessionBuffer      cong_win,
                  TcpSessionBuffer      sstresh,
                  ap_uint<2>            retrans_count,
                  bool                  fast_retrans,
                  TcpSessionBufferScale win_shift_scale)
      : perv_ack(ack)
      , next_byte(next)
      , cong_window(cong_win)
      , slowstart_threshold(sstresh)
      , retrans_count(retrans_count)
      , fast_retrans(fast_retrans)
      , win_shift(win_shift_scale) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Prev AckNo: " << perv_ack.to_string(16) << "\t";
    sstream << "Next Byte: " << next_byte.to_string(16) << "\t";
    sstream << "Cong Win: " << cong_window.to_string(16) << "\t";
    sstream << "Slow thres: " << slowstart_threshold.to_string(16) << "\t";
    sstream << "Retrans Count: " << retrans_count.to_string(16) << "\t";
    sstream << "Fast Rt? : " << fast_retrans << "\t";
    sstream << "WinScaleOpt: " << win_shift.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};
struct TxEngToTxSarReq {
  TcpSessionID session_id;
  ap_uint<32>  not_ackd;
  ap_uint<1>   write;
  ap_uint<1>   init;
  bool         fin_is_ready;
  bool         fin_is_sent;
  bool         retrans_req;
  TxEngToTxSarReq() {}
  TxEngToTxSarReq(ap_uint<16> id)
      : session_id(id)
      , not_ackd(0)
      , write(0)
      , init(0)
      , fin_is_ready(false)
      , fin_is_sent(false)
      , retrans_req(false) {}
  TxEngToTxSarReq(ap_uint<16> id, ap_uint<32> not_ackd, ap_uint<1> write)
      : session_id(id)
      , not_ackd(not_ackd)
      , write(write)
      , init(0)
      , fin_is_ready(false)
      , fin_is_sent(false)
      , retrans_req(false) {}
  TxEngToTxSarReq(ap_uint<16> id, ap_uint<32> not_ackd, ap_uint<1> write, ap_uint<1> init)
      : session_id(id)
      , not_ackd(not_ackd)
      , write(write)
      , init(init)
      , fin_is_ready(false)
      , fin_is_sent(false)
      , retrans_req(false) {}
  TxEngToTxSarReq(ap_uint<16> id,
                  ap_uint<32> not_ackd,
                  ap_uint<1>  write,
                  ap_uint<1>  init,
                  bool        fin_is_ready,
                  bool        fin_is_sent)
      : session_id(id)
      , not_ackd(not_ackd)
      , write(write)
      , init(init)
      , fin_is_ready(fin_is_ready)
      , fin_is_sent(fin_is_sent)
      , retrans_req(false) {}
  TxEngToTxSarReq(ap_uint<16> id,
                  ap_uint<32> not_ackd,
                  ap_uint<1>  write,
                  ap_uint<1>  init,
                  bool        fin_is_ready,
                  bool        fin_is_sent,
                  bool        isRt)
      : session_id(id)
      , not_ackd(not_ackd)
      , write(write)
      , init(init)
      , fin_is_ready(fin_is_ready)
      , fin_is_sent(fin_is_sent)
      , retrans_req(isRt) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "NotAcked: " << not_ackd.to_string(16) << "\t";
    sstream << "Write?: " << write << "\t";
    sstream << "Init?: " << init << "\t";
    sstream << "FinReady?: " << fin_is_ready << "\t";
    sstream << "FinSent?: " << fin_is_sent << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct TxEngToTxSarRetransReq : public TxEngToTxSarReq {
  TxEngToTxSarRetransReq() {}
  TxEngToTxSarRetransReq(const TxEngToTxSarReq &req)
      : TxEngToTxSarReq(req.session_id,
                        req.not_ackd,
                        req.write,
                        req.init,
                        req.fin_is_ready,
                        req.fin_is_sent,
                        req.retrans_req) {}
  TxEngToTxSarRetransReq(ap_uint<16> id, TcpSessionBuffer ssthresh)
      : TxEngToTxSarReq(id, ssthresh, 1, 0, false, false, true) {}
  TcpSessionBuffer get_threshold() { return not_ackd(WINDOW_BITS - 1, 0); }
};

struct TxSarToTxEngRsp {
  ap_uint<32>      ackd;
  ap_uint<32>      not_ackd;
  TcpSessionBuffer min_window;
  TcpSessionBuffer app_written;
  bool             fin_is_ready;
  bool             fin_is_sent;
  TcpSessionBuffer curr_length;
  TcpSessionBuffer used_length;
  TcpSessionBuffer used_length_rst;
  TcpSessionBuffer usable_window;

  bool        ackd_eq_not_ackd;
  ap_uint<32> not_ackd_short;
  bool        usablewindow_b_mss;
  ap_uint<32> not_ackd_plus_mss;
  ap_uint<4>  win_shift;

  // ap_uint<16> Send_Window;
  TxSarToTxEngRsp()
      : ackd(0)
      , not_ackd(0)
      , min_window(0)
      , app_written(0)
      , fin_is_ready(0)
      , fin_is_sent(0)
      , curr_length(0)
      , used_length(0)
      , used_length_rst(0)
      , usable_window(0)
      , ackd_eq_not_ackd(0)
      , not_ackd_short(0)
      , usablewindow_b_mss(0)
      , not_ackd_plus_mss(0)
      , win_shift(0) {}
  TxSarToTxEngRsp(ap_uint<32>      ack,
                  ap_uint<32>      nack,
                  TcpSessionBuffer min_window,
                  TcpSessionBuffer app,
                  bool             fin_is_ready,
                  bool             fin_is_sent)
      : ackd(ack)
      , not_ackd(nack)
      , min_window(min_window)
      , app_written(app)
      , fin_is_ready(fin_is_ready)
      , fin_is_sent(fin_is_sent)
      , curr_length(0)
      , used_length(0)
      , used_length_rst(0)
      , usable_window(0)
      , ackd_eq_not_ackd(0)
      , not_ackd_short(0)
      , usablewindow_b_mss(0)
      , not_ackd_plus_mss(0)
      , win_shift(0) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "AckNo: " << ackd.to_string(16) << "\t";
    sstream << "Not acked: " << not_ackd.to_string(16) << "\t";
    sstream << "Min window " << min_window.to_string(16) << "\t";
    sstream << "App written: " << app_written.to_string(16) << "\t";
    sstream << "Fin is ready?: " << fin_is_ready << "\t";
    sstream << "Fin is sent?: " << fin_is_sent << "\t";
    sstream << "Cur Length: " << curr_length.to_string(16) << "\t";
    sstream << "Used Length: " << used_length.to_string(16) << "\t";
    sstream << "Used Length Rst: " << used_length_rst.to_string(16) << "\t";
    sstream << "Usable Win: " << usable_window.to_string(16) << "\t";
    sstream << "WinScaleOpt: " << win_shift.to_string(16) << "\t";
    sstream << "Acked equal not acked" << ackd_eq_not_ackd ? "1" : "0";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// when app write new data to session buffer, update the Tx SAR Table, no response for this request
struct TxAppToTxSarReq {
  TcpSessionID     session_id;
  TcpSessionBuffer app_written;
  TxAppToTxSarReq() {}
  TxAppToTxSarReq(TcpSessionID id, TcpSessionBuffer app) : session_id(id), app_written(app) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "App written: " << app_written.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// update Tx App Table
struct TxSarToTxAppReq {
  TcpSessionID     session_id;
  TcpSessionBuffer ackd;
#if (TCP_NODELAY)
  TcpSessionBuffer min_window;
#endif
  ap_uint<1> init;
  TxSarToTxAppReq() {}
#if !(TCP_NODELAY)
  TxSarToTxAppReq(TcpSessionID id, TcpSessionBuffer ackd) : session_id(id), ackd(ackd), init(0) {}
  TxSarToTxAppReq(TcpSessionID id, TcpSessionBuffer ackd, ap_uint<1> init)
      : session_id(id), ackd(ackd), init(init) {}
#else
  TxSarToTxAppReq(TcpSessionID id, TcpSessionBuffer ackd, TcpSessionBuffer min_window)
      : session_id(id), ackd(ackd), min_window(min_window), init(0) {}
  TxSarToTxAppReq(TcpSessionID     id,
                  TcpSessionBuffer ackd,
                  TcpSessionBuffer min_window,
                  ap_uint<1>       init)
      : session_id(id), ackd(ackd), min_window(min_window), init(init) {}
#endif

#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "AckNo: " << ackd.to_string(16) << "\t";
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
#if (TCP_NODELAY)
    sstream << "Min window " << min_window.to_string(16) << "\t";
#endif
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// notify the app from rx app intf
// 1. the session is receiving the new data from rx_engine
// 2. the session is closed caused by reaching the max retransmit attempt count
struct AppNotificationNoTDEST {
  TcpSessionID  session_id;
  ap_uint<16>   length;
  IpAddr        ip_addr;
  TcpPortNumber dst_tcp_port;
  bool          closed;
  AppNotificationNoTDEST() : session_id(0), length(0), ip_addr(0), dst_tcp_port(0), closed(0) {}
  AppNotificationNoTDEST(TcpSessionID id, bool closed)
      : session_id(id), length(0), ip_addr(0), dst_tcp_port(0), closed(closed) {}
  AppNotificationNoTDEST(TcpSessionID id, ap_uint<16> len, IpAddr addr, ap_uint<16> port)
      : session_id(id), length(len), ip_addr(addr), dst_tcp_port(port), closed(false) {}
  AppNotificationNoTDEST(TcpSessionID id, IpAddr addr, ap_uint<16> port, bool closed)
      : session_id(id), length(0), ip_addr(addr), dst_tcp_port(port), closed(closed) {}
  AppNotificationNoTDEST(
      TcpSessionID id, ap_uint<16> len, IpAddr addr, ap_uint<16> port, bool closed)
      : session_id(id), length(len), ip_addr(addr), dst_tcp_port(port), closed(closed) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID " << (session_id).to_string(16) << "\t";
    sstream << "Length : " << length << "\t";
    sstream << "IP:Port " << SwapByte(ip_addr).to_string(16) << ":"
            << SwapByte<16>(dst_tcp_port).to_string(16) << "\t";
    sstream << "Closed " << (closed ? "Yes." : "No.");
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

typedef hls::axis<AppNotificationNoTDEST, 0, 0, NET_TDEST_WIDTH> NetAXISAppNotification;

struct AppNotification {
  AppNotificationNoTDEST data;
  NetAXISDest            dest;
  AppNotification() {}
  AppNotification(const NetAXISAppNotification &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISAppNotification to_net_axis() {
#pragma HLS INLINE
    NetAXISAppNotification new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "AppNotify: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// Tx app use ip:port to create session with other side
typedef hls::axis<OpenConnTuple, 0, 0, NET_TDEST_WIDTH> NetAXISAppOpenConnReq;

struct AppOpenConnReq {
  OpenConnTuple data;
  NetAXISDest   dest;
  AppOpenConnReq() {}
  AppOpenConnReq(const NetAXISAppOpenConnReq &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISAppOpenConnReq to_net_axis() {
#pragma HLS INLINE
    NetAXISAppOpenConnReq new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "OpenConn: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

/***
 * David ETHz phd thesis
 * Table 4.1  Event types defined in the TCP module.
 *
 * TX: Regular transmission of payload.
 * RT: Retransmission for a specific connection.
 * ACK: Acknowledgment, might be delayed by Ack Delay module.
 * SYN: Generates a SYN packet to setup a new connection.
 * SYNACK: Generates a SYN-ACK packet to setup a connection initiated by the other end.
 * FIN: Generates a FIN packet to tear down the connection.
 * RST: Generates a RST packet, e.g. when an invalid packet was re- ceived or a SYN packet was
 * received on a closed port.
 * ACKNODELAY: Acknowledgment that cannot be delayed; this event is used
 * during connection establishment instead of ACK.
 *
 */
enum EventType { TX, RT, ACK, SYN, SYN_ACK, FIN, RST, ACK_NODELAY, RT_CONT };
#ifndef __SYNTHESIS__
INLINE std::string event_type_to_string(EventType type) {
  switch (type) {
    case TX:
      return "TX";
    case RT:
      return "RT";
    case ACK:
      return "ACK";
    case SYN:
      return "SYN";
    case SYN_ACK:
      return "SYN_ACK";
    case FIN:
      return "FIN";
    case RST:
      return "RST";
    case ACK_NODELAY:
      return "ACK_NODELAY";
    case RT_CONT:
      return "RT_CONT";
    default:
      return "[Unknown Event]";
  }
}
#else
INLINE char *event_type_to_string(EventType type) {
  _AP_UNUSED_PARAM(type);
  return 0;
}
#endif

struct Event {
  EventType        type;
  TcpSessionID     session_id;
  TcpSessionBuffer buf_addr;
  ap_uint<16>      length;
  ap_uint<3>       retrans_cnt;
  Event() {}
  // Event(const Event&) {}
  Event(EventType type, TcpSessionID id)
      : type(type), session_id(id), buf_addr(0), length(0), retrans_cnt(0) {}
  Event(EventType type, TcpSessionID id, ap_uint<3> retrans_cnt)
      : type(type), session_id(id), buf_addr(0), length(0), retrans_cnt(retrans_cnt) {}
  Event(EventType type, TcpSessionID id, TcpSessionBuffer addr, ap_uint<16> len)
      : type(type), session_id(id), buf_addr(addr), length(len), retrans_cnt(0) {}
  Event(EventType        type,
        TcpSessionID     id,
        TcpSessionBuffer addr,
        ap_uint<16>      len,
        ap_uint<3>       retrans_cnt)
      : type(type), session_id(id), buf_addr(addr), length(len), retrans_cnt(retrans_cnt) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "EventType: " << event_type_to_string(type) << "\t";
    sstream << "SessionID: " << this->session_id << "\t";
    sstream << "BufferAddr: " << this->buf_addr.to_string(16) << "\t";
    sstream << "Length: " << this->length.to_string(16) << "\t";
    sstream << "RetransCnt: " << this->retrans_cnt << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct EventWithTuple : public Event {
  FourTuple four_tuple;
  EventWithTuple() {}
  EventWithTuple(const Event &ev)
      : Event(ev.type, ev.session_id, ev.buf_addr, ev.length, ev.retrans_cnt) {
    four_tuple = FourTuple();
  }
  EventWithTuple(const Event &ev, FourTuple tuple)
      : Event(ev.type, ev.session_id, ev.buf_addr, ev.length, ev.retrans_cnt), four_tuple(tuple) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "EventType: " << event_type_to_string(type) << "\t";
    sstream << "SessionID: " << this->session_id << "\t";
    sstream << "BufferAddr: " << this->buf_addr << "\t";
    sstream << "Length: " << this->length << "\t";
    sstream << "RetransCnt: " << this->retrans_cnt << "\t";
    sstream << "RetransCnt: " << this->retrans_cnt << "\t";
    sstream << std::hex << four_tuple.to_string();
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct RstEvent : public Event {
  RstEvent() {}
  RstEvent(const Event &ev)
      : Event(ev.type, ev.session_id, ev.buf_addr, ev.length, ev.retrans_cnt) {}
  RstEvent(ap_uint<32> seq) : Event(RST, 0, seq(31, 16), seq(15, 0), 0) {}
  RstEvent(TcpSessionID id, ap_uint<32> seq) : Event(RST, id, seq(31, 16), seq(15, 0), 1) {}
  RstEvent(TcpSessionID id, ap_uint<32> seq, bool has_session_id)
      : Event(RST, id, seq(31, 16), seq(15, 0), has_session_id) {}
  ap_uint<32> get_ack_number() {
    ap_uint<32> seq;  // TODO FIXME REVIEW
    seq(31, 16) = buf_addr;
    seq(15, 0)  = length;
    return seq;
  }
  bool has_session_id() { return (retrans_cnt != 0); }
};

struct RxEngToRetransTimerReq {
  TcpSessionID session_id;
  bool         stop;
  RxEngToRetransTimerReq() {}
  RxEngToRetransTimerReq(TcpSessionID id) : session_id(id), stop(false) {}
  RxEngToRetransTimerReq(TcpSessionID id, bool stop) : session_id(id), stop(stop) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t"
            << "Stop?: " << stop;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct TxEngToRetransTimerReq {
  TcpSessionID session_id;
  EventType    type;
  TxEngToRetransTimerReq() : session_id(0), type(RT) {}
  TxEngToRetransTimerReq(TcpSessionID id) : session_id(id), type(RT) {}
  TxEngToRetransTimerReq(TcpSessionID id, EventType type) : session_id(id), type(type) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t"
            << "Event Type: " << event_type_to_string(type);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct RxEngToSlookupReq {
  FourTuple   four_tuple;
  bool        allow_creation;
  NetAXISDest role_id;
  RxEngToSlookupReq() {}
  RxEngToSlookupReq(FourTuple tuple, bool allow_creation, NetAXISDest role_id)
      : four_tuple(tuple), allow_creation(allow_creation), role_id(role_id) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "FourTuple: " << four_tuple.to_string() << "\t"
            << "Allow Create?: " << allow_creation << "\t"
            << "RoleID: " << std::hex << role_id.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct TxAppToSlookupReq {
  FourTuple   four_tuple;
  NetAXISDest role_id;
  TxAppToSlookupReq() : four_tuple(), role_id(0) {}
  TxAppToSlookupReq(FourTuple tuple, NetAXISDest role_id) : four_tuple(tuple), role_id(role_id) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "FourTuple: " << this->four_tuple.to_string() << "\t";
    sstream << "RoleID: " << std::hex << role_id.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct ReverseTableToTxEngRsp {
  FourTuple   four_tuple;
  NetAXISDest role_id;
  ReverseTableToTxEngRsp() {}
  ReverseTableToTxEngRsp(FourTuple tuple, NetAXISDest role) : four_tuple(tuple), role_id(role) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "FourTuple: " << this->four_tuple.to_string() << "\t";
    sstream << "RoleID: " << std::hex << role_id.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct SessionLookupRsp {
  TcpSessionID session_id;
  bool         hit;
  NetAXISDest  role_id;
  SessionLookupRsp() : session_id(0), hit(0), role_id(0) {}
  SessionLookupRsp(TcpSessionID session_id, bool hit, NetAXISDest role_id)
      : session_id(session_id), hit(hit), role_id(role_id) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << this->session_id << "\t";
    sstream << "Hit? " << (hit ? "Yes." : "No.") << "\t";
    sstream << "RoleID: " << std::hex << role_id.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct AppReadReqNoTEST {
  TcpSessionID session_id;
  ap_uint<16>  read_length;
  AppReadReqNoTEST() {}
  AppReadReqNoTEST(TcpSessionID id, ap_uint<16> len) : session_id(id), read_length(len) {}
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << " ";
    sstream << "Read Length: " << read_length.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// TOE interface
typedef hls::axis<AppReadReqNoTEST, 0, 0, NET_TDEST_WIDTH> NetAXISAppReadReq;

// APP inner connection
struct AppReadReq {
  AppReadReqNoTEST data;
  NetAXISDest      dest;
  AppReadReq() {}
  AppReadReq(const NetAXISAppReadReq &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISAppReadReq to_net_axis() {
#pragma HLS INLINE
    NetAXISAppReadReq new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  INLINE std::string to_string() {
    std::stringstream sstream;
    sstream << "Data: " << data.to_string() << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// TOE interface
typedef hls::axis<TcpSessionID, 0, 0, NET_TDEST_WIDTH> NetAXISAppReadRsp;

// inner connection
struct AppReadRsp {
  TcpSessionID data;
  NetAXISDest  dest;
  AppReadRsp() {}
  AppReadRsp(TcpSessionID id) : data(id) {}
  AppReadRsp(const NetAXISAppReadRsp &net_axis) {
#pragma HLS INLINE
    data = net_axis.data;
    dest = net_axis.dest;
  }
  NetAXISAppReadRsp to_net_axis() {
#pragma HLS INLINE
    NetAXISAppReadRsp new_axis;
    new_axis.data = data;
    new_axis.dest = dest;
    return new_axis;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "App read data rsp: SessionID: " << data.to_string(16) << "\t";
    sstream << "Dest: " << dest.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

// read/ write DDR buffer internal command
struct MemBufferRWCmd {
  ap_uint<32> addr;
  ap_uint<16> length;
  // next_addr(WINDOW_BITS) == 1 means the buffer is overflow, the cmd need to breakdown
  ap_uint<WINDOW_BITS + 1> next_addr;

  MemBufferRWCmd() : addr(0), length(0), next_addr(0) {}
  MemBufferRWCmd(ap_uint<32> addr, ap_uint<16> length)
      : addr(addr), length(length), next_addr(addr(WINDOW_BITS - 1, 0) + length) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Addr: " << addr.to_string(16) << "\t";
    sstream << "Len: " << length.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

//
struct MemBufferRWCmdDoubleAccess {
  bool       double_access;
  ap_uint<6> byte_offset;
  MemBufferRWCmdDoubleAccess() : double_access(false), byte_offset(0) {}
  MemBufferRWCmdDoubleAccess(bool double_access, ap_uint<6> offset)
      : double_access(double_access), byte_offset(offset) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "DoubleAccess?: " << this->double_access << "\t";
    sstream << "ByteOffset: " << byte_offset.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

/** @ingroup session_lookup_controller
 *  This struct defines the internal storage format of the IP tuple instead of destiantion and
 * source, my and their is used. When a tuple is sent or received from the tx/rx path it is mapped
 * to the fourTuple struct. The < operator is necessary for the c++ dummy memory implementation
 * which uses an std::map
 */
struct ThreeTuple {
  IpAddr        there_ip_addr;
  TcpPortNumber here_tcp_port;
  TcpPortNumber there_tcp_port;
  ThreeTuple() : there_ip_addr(0), here_tcp_port(0), there_tcp_port(0) {}
  ThreeTuple(IpAddr there_ip_addr, TcpPortNumber here_tcp_port, TcpPortNumber there_tcp_port)
      : there_ip_addr(there_ip_addr)
      , here_tcp_port(here_tcp_port)
      , there_tcp_port(there_tcp_port) {}

  bool operator<(const ThreeTuple &other) const {
    if (there_ip_addr < other.there_ip_addr) {
      return true;
    } else if (there_ip_addr == other.there_ip_addr) {
      if (here_tcp_port < other.here_tcp_port) {
        return true;
      } else if (here_tcp_port == other.here_tcp_port) {
        if (there_tcp_port < other.there_tcp_port) {
          return true;
        }
      }
    }
    return false;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Here Port " << SwapByte(here_tcp_port).to_string(16) << "\t";
    sstream << "There IP:Port " << SwapByte(there_ip_addr).to_string(16) << ":"
            << SwapByte(there_tcp_port).to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

/***
 * @brief DataMoverCmd
 * Refenrence:
 * https://www.xilinx.com/support/documentation/ip_documentation/axi_datamover/v5_1/pg022_axi_datamover.pdf
 * Table 2???7: Command Word Description
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
  DataMoverCmd() : bbt(0), type(0), dsa(0), eof(0), drr(0), saddr(0), tag(0), rsvd(0) {}
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
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct DataMoverStatus {
  ap_uint<4> tag;
  ap_uint<1> interr;
  ap_uint<1> decerr;
  ap_uint<1> slverr;
  ap_uint<1> okay;
  DataMoverStatus() : tag(0), interr(0), decerr(0), slverr(0), okay(0) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Tag: " << tag.to_string(16) << "\t";
    sstream << "Okay: " << okay.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

/** @ingroup session_lookup_controller
 *
 */
enum SlookupSource { RX, TX_APP };

/** @ingroup session_lookup_controller
 *
 */
enum SlookupOp { INSERT, DELETE };

/** @ingroup session_lookup_controller
 *
 */
struct RtlSLookupToCamLupReq {
  CamTuple      key;
  SlookupSource source;
  RtlSLookupToCamLupReq() : key(), source(RX) {}
  RtlSLookupToCamLupReq(CamTuple tuple, SlookupSource src) : key(tuple), source(src) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Key: " << key.to_string() << "Source: " << source;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlSlookupToCamUpdReq {
  SlookupOp     op;
  CamTuple      key;
  TcpSessionID  value;
  SlookupSource source;
  RtlSlookupToCamUpdReq() {}
  RtlSlookupToCamUpdReq(CamTuple key, TcpSessionID value, SlookupOp op, SlookupSource src)
      : key(key), value(value), op(op), source(src) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Key: " << key.to_string() << "\t"
            << "Source: " << source << "\t"
            << "Update Op Ins/Del: " << op << "\t"
            << "Update Val: " << value.to_string(16);
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlCamToSlookupLupRsp {
  CamTuple      key;
  TcpSessionID  session_id;
  bool          hit;
  SlookupSource source;
  RtlCamToSlookupLupRsp() {}
  RtlCamToSlookupLupRsp(bool hit, SlookupSource src) : hit(hit), session_id(0), source(src) {}
  RtlCamToSlookupLupRsp(bool hit, TcpSessionID id, SlookupSource src)
      : hit(hit), session_id(id), source(src) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Key: " << key.to_string() << "\t"
            << "Val: " << session_id.to_string(16) << "\t"
            << "Hit:? " << hit << "\t"
            << "Source: " << source;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlCamToSlookupUpdRsp {
  SlookupOp     op;
  CamTuple      key;
  TcpSessionID  session_id;
  bool          success;
  SlookupSource source;
  RtlCamToSlookupUpdRsp() {}
  RtlCamToSlookupUpdRsp(SlookupOp op, SlookupSource src) : op(op), source(src) {}
  RtlCamToSlookupUpdRsp(TcpSessionID id, SlookupOp op, SlookupSource src)
      : session_id(id), op(op), source(src) {}
  RtlCamToSlookupUpdRsp(
      SlookupOp op, CamTuple tuple, TcpSessionID id, bool success, SlookupSource src)
      : op(op), key(tuple), session_id(id), success(success), source(src) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Key: " << key.to_string() << "\t"
            << "Update Val: " << session_id.to_string(16) << "\t"
            << "Update Source: " << source << "\t"
            << "Update Op Ins/Del: " << op << "\t"
            << "Success:? " << success << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};
