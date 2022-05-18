#ifndef _TCP_CONN_
#define _TCP_CONN_

#include "toe/toe_config.hpp"
#include "utils/axi_utils.hpp"

typedef ap_uint<16> TcpSessionID;
// only use in little endian
typedef ap_uint<16>                TcpPortNumber;
typedef ap_uint<WINDOW_SCALE_BITS> TcpSessionBufferScale;
typedef ap_uint<WINDOW_BITS>       TcpSessionBuffer;
typedef ap_uint<32>                IpAddr;

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

struct PortTableEntry {
  bool        is_open;
  NetAXISDest role_id;
};

struct ListenPortRsp {
  ap_uint<16> port_number;
  bool        open_successfully;
  bool        wrong_port_number;
  // already open may open by current role, or other role
  bool already_open;
  ListenPortRsp() {}
  ListenPortRsp(bool          open_successfully,
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
    sstream << "Open Port Response: " << std::dec << this->port_number << " \tOpened successfully "
            << ((this->open_successfully) ? "Yes." : "No.") << "\tWrong port number "
            << ((this->wrong_port_number) ? "Yes." : "No.") << "\tThe port is already open "
            << ((this->already_open) ? "Yes." : "No.");
    return sstream.str();
  }
#endif
};

struct PtableToRxEngRsp {
  bool        is_open;
  NetAXISDest role_id;
  PtableToRxEngRsp() : is_open(false), role_id(0) {}
  PtableToRxEngRsp(bool is_open, NetAXISDest dest) : is_open(is_open), role_id(dest) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Opened: " << (this->is_open ? "Yes." : "No.") << "\t";
    sstream << "Role ID: " << this->role_id;
    return sstream.str();
  }
#endif
};

struct OpenSessionStatus {
  TcpSessionID session_id;
  bool         success;
  OpenSessionStatus() {}
  OpenSessionStatus(TcpSessionID id, bool success) : session_id(id), success(success) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Session ID: " << session_id.to_string(16) << "\t";
    sstream << "Success: " << (success ? "Yes." : "No.") << "\t";
    return sstream.str();
  }
#endif
};

struct NewClientNotification {
  TcpSessionID session_id;  // Tells to the tx application the ID
  /*
   * not used for the time being, tells to the app the negotiated buffer size.
   * 65536 * (buffersize+1)
   */
  // ap_uint<8>  buffersize;
  ap_uint<11> max_segment_size;  // max 2048
  /*
   * Tells the application that the design uses TCP_NODELAY which means that the
   * data transfers has to be done in chunks of a maximum size
   */
  bool tcp_nodelay;
  bool tcp_buffer_is_empty;  // Tells when the Tx buffer is empty. It is also used as a
                             // way to indicate that a new connection was opened.
  NewClientNotification() {}
  NewClientNotification(
      TcpSessionID id, ap_uint<8> buffersize, ap_uint<11> mss, bool tcp_nodelay, bool buf_empty)
      : session_id(id)
      , max_segment_size(mss)
      , tcp_nodelay(tcp_nodelay)
      , tcp_buffer_is_empty(buf_empty) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Session ID: " << session_id.to_string(16) << "\t";
    sstream << "MaxSegSize: " << max_segment_size.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

enum TxAppTransDataError { NO_ERROR, ERROR_NOCONNECTION, ERROR_NOSPACE, ERROR_WINDOW };
struct AppTransDataRsp {
  TxAppTransDataError error;
  TcpSessionBuffer    remaining_space;
  ap_uint<16>         length;
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "TransError: " << error << "\t";
    sstream << "RemainSpace: " << remaining_space.to_string(16) << "\t";
    sstream << "TransLength: " << length.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

struct FourTuple {
  // big endian
  IpAddr src_ip_addr;
  IpAddr dst_ip_addr;
  // big endian
  TcpPortNumber src_tcp_port;
  TcpPortNumber dst_tcp_port;
  FourTuple() {}
  FourTuple(IpAddr        src_ip_addr,
            IpAddr        dst_ip_addr,
            TcpPortNumber src_tcp_port,
            TcpPortNumber dst_tcp_port)
      : src_ip_addr(src_ip_addr)
      , dst_ip_addr(dst_ip_addr)
      , src_tcp_port(src_tcp_port)
      , dst_tcp_port(dst_tcp_port) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Src IP:Port " << SwapByte(src_ip_addr).to_string(16) << ":"
            << SwapByte(src_tcp_port).to_string(16) << "\t";
    sstream << "Dst IP:Port " << SwapByte(dst_ip_addr).to_string(16) << ":"
            << SwapByte(dst_tcp_port).to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

struct IpPortTuple {
  IpAddr        ip_addr;
  TcpPortNumber tcp_port;
  IpPortTuple() {}
  IpPortTuple(IpAddr ip, TcpPortNumber port) : ip_addr(ip), tcp_port(port) {}
};

struct StateTableReq {
  TcpSessionID session_id;
  SessionState state;
  ap_uint<1>   write;
  StateTableReq() {}
  // Read queset
  StateTableReq(TcpSessionID id) : session_id(id), state(CLOSED), write(0) {}
  // write quest
  StateTableReq(TcpSessionID id, SessionState state, ap_uint<1> write)
      : session_id(id), state(state), write(write) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "State: " << state << "\t";
    sstream << "Write?: " << write << "\t";
    return sstream.str();
  }
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
#endif
};

struct RxSarLookupRsp {
  ap_uint<32>           recvd;
  TcpSessionBufferScale win_shift;
  // win_size is 16 bit, in tcp header win
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

  RxEngToRxSarReq() {}
  RxEngToRxSarReq(TcpSessionID id) : session_id(id), recvd(0), write(0), init(0) {}
  RxEngToRxSarReq(TcpSessionID id, ap_uint<32> recvd, ap_uint<1> write)
      : session_id(id), recvd(recvd), write(write), init(0) {}
  RxEngToRxSarReq(TcpSessionID id, ap_uint<32> recvd, ap_uint<1> write, ap_uint<1> init)
      : session_id(id), recvd(recvd), write(write), init(init) {}
  RxEngToRxSarReq(
      TcpSessionID id, ap_uint<32> recvd, ap_uint<1> write, ap_uint<1> init, ap_uint<4> wsopt)
      : session_id(id), recvd(recvd), write(write), init(init), win_shift(wsopt) {}
};

// Update Rx SAR table when Rx APP has been read new data, or want to read new data
struct RxSarAppReqRsp {
  TcpSessionID     session_id;
  TcpSessionBuffer app_read;
  ap_uint<1>       write;
  RxSarAppReqRsp() {}
  RxSarAppReqRsp(TcpSessionID id) : session_id(id), app_read(0), write(0) {}
  RxSarAppReqRsp(TcpSessionID id, TcpSessionBuffer appd)
      : session_id(id), app_read(appd), write(1) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Recved SessionID No: " << session_id.to_string(16) << "\t";
    sstream << "App Read " << app_read.to_string(16) << "\t";
    sstream << "Write?: " << write.to_string(16);
    return sstream.str();
  }
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
  RxEngToTxSarReq() {}
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
                  TcpSessionBuffer     ackd,
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
                  TcpSessionBuffer      ackd,
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
};

struct TxSarToRxEngRsp {
  ap_uint<32>           perv_ack;
  ap_uint<32>           next_byte;
  TcpSessionBuffer      cong_window;
  TcpSessionBuffer      slowstart_threshold;
  ap_uint<2>            retrans_count;
  bool                  fast_retrans;
  TcpSessionBufferScale win_shift;
  TxSarToRxEngRsp() {}
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
};

// TODO: unnecessary structure. zelin 22-05-07
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
  TxSarToTxEngRsp() {}
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
      , fin_is_sent(fin_is_sent) {}
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
    sstream << "WinScaleOpt: " << win_shift.to_string(16);
    return sstream.str();
  }
#endif
};

// when app write new data to session buffer, update the Tx SAR Table, no response for this request
struct TxAppToTxSarReq {
  TcpSessionID     session_id;
  TcpSessionBuffer app_written;
  TxAppToTxSarReq() {}
  TxAppToTxSarReq(TcpSessionID id, TcpSessionBuffer app) : session_id(id), app_written(app) {}
};

// TODO: Need comments here!
struct TxSarToTxAppRsp {
  TcpSessionID     session_id;
  TcpSessionBuffer ackd;
#if (TCP_NODELAY)
  TcpSessionBuffer min_window;
#endif
  ap_uint<1> init;
  TxSarToTxAppRsp() {}
#if !(TCP_NODELAY)
  TxSarToTxAppRsp(TcpSessionID id, TcpSessionBuffer ackd) : session_id(id), ackd(ackd), init(0) {}
  TxSarToTxAppRsp(TcpSessionID id, TcpSessionBuffer ackd, ap_uint<1> init)
      : session_id(id), ackd(ackd), init(init) {}
#else
  TxSarToTxAppRsp(TcpSessionID id, TcpSessionBuffer ackd, TcpSessionBuffer min_window)
      : session_id(id), ackd(ackd), min_window(min_window), init(0) {}
  TxSarToTxAppRsp(TcpSessionID     id,
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
#endif
};

struct AppNotification {
  TcpSessionID  session_id;
  ap_uint<16>   length;
  IpAddr        ip_addr;
  TcpPortNumber dst_tcp_port;
  bool          closed;
  AppNotification() {}
  AppNotification(TcpSessionID id, bool closed)
      : session_id(id), length(0), ip_addr(0), dst_tcp_port(0), closed(closed) {}
  AppNotification(TcpSessionID id, ap_uint<16> len, IpAddr addr, ap_uint<16> port)
      : session_id(id), length(len), ip_addr(addr), dst_tcp_port(port), closed(false) {}
  AppNotification(TcpSessionID id, IpAddr addr, ap_uint<16> port, bool closed)
      : session_id(id), length(0), ip_addr(addr), dst_tcp_port(port), closed(closed) {}
  AppNotification(TcpSessionID id, ap_uint<16> len, IpAddr addr, ap_uint<16> port, bool closed)
      : session_id(id), length(len), ip_addr(addr), dst_tcp_port(port), closed(closed) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID " << (session_id).to_string(16) << "\t";
    sstream << "Length : " << length << "\t";
    sstream << "IP:Port " << SwapByte(ip_addr).to_string(16) << ":"
            << SwapByte<16>(dst_tcp_port).to_string(16) << "\t";
    sstream << "Closed " << (closed ? "Yes." : "No.");
    return sstream.str();
  }
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
  std::string to_string() {
    std::stringstream sstream;
    sstream << "EventType: " << this->type << "\t";
    sstream << "SessionID: " << this->session_id << "\t";
    sstream << "BufferAddr: " << this->buf_addr.to_string(16) << "\t";
    sstream << "Length: " << this->length.to_string(16) << "\t";
    sstream << "RetransCnt: " << this->retrans_cnt << "\t";
    return sstream.str();
  }
#endif
};

struct EventWithTuple : public Event {
  FourTuple four_tuple;
  EventWithTuple() {}
  EventWithTuple(const Event &ev)
      : Event(ev.type, ev.session_id, ev.buf_addr, ev.length, ev.retrans_cnt) {}
  EventWithTuple(const Event &ev, FourTuple tuple)
      : Event(ev.type, ev.session_id, ev.buf_addr, ev.length, ev.retrans_cnt), four_tuple(tuple) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "EventType: " << this->type << "\t";
    sstream << "SessionID: " << this->session_id << "\t";
    sstream << "BufferAddr: " << this->buf_addr << "\t";
    sstream << "Length: " << this->length << "\t";
    sstream << "RetransCnt: " << this->retrans_cnt << "\t";
    sstream << "RetransCnt: " << this->retrans_cnt << "\t";
    sstream << std::hex << four_tuple.to_string();
    return sstream.str();
  }
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
  ap_uint<32> getAckNumb() {
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
};

struct TxEngToRetransTimerReq {
  TcpSessionID session_id;
  EventType    type;
  TxEngToRetransTimerReq() {}
  TxEngToRetransTimerReq(TcpSessionID id) : session_id(id), type(RT) {}
  TxEngToRetransTimerReq(TcpSessionID id, EventType type) : session_id(id), type(type) {}
};

struct RxEngToSlookupReq {
  FourTuple   four_tuple;
  bool        allow_creation;
  NetAXISDest role_id;
  RxEngToSlookupReq() {}
  RxEngToSlookupReq(FourTuple tuple, bool allow_creation, NetAXISDest role_id)
      : four_tuple(tuple), allow_creation(allow_creation), role_id(role_id) {}
};

struct TxAppToSlookupReq {
  FourTuple   four_tuple;
  NetAXISDest role_id;
  TxAppToSlookupReq() {}
  TxAppToSlookupReq(FourTuple tuple, NetAXISDest role_id) : four_tuple(tuple), role_id(role_id) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "FourTuple: " << this->four_tuple.to_string() << "\t";
    sstream << "RoleID: " << std::hex << role_id.to_string(16);
    return sstream.str();
  }
#endif
};

struct ReverseTableToTxEngRsp {
  FourTuple   four_tuple;
  NetAXISDest role_id;
  ReverseTableToTxEngRsp() {}
  ReverseTableToTxEngRsp(FourTuple tuple, NetAXISDest role) : four_tuple(tuple), role_id(role) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "FourTuple: " << this->four_tuple.to_string() << "\t";
    sstream << "RoleID: " << std::hex << role_id.to_string(16);
    return sstream.str();
  }
#endif
};

struct SessionLookupRsp {
  TcpSessionID session_id;
  bool         hit;
  NetAXISDest  role_id;
  SessionLookupRsp() {}
  SessionLookupRsp(TcpSessionID session_id, bool hit, NetAXISDest role_id)
      : session_id(session_id), hit(hit), role_id(role_id) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << this->session_id << "\t";
    sstream << "Hit? " << (hit ? "Yes." : "No.") << "\t";
    sstream << "RoleID: " << std::hex << role_id.to_string(16);
    return sstream.str();
  }
#endif
};

// APP inner connection
struct AppReadReq {
  TcpSessionID session_id;
  // ap_uint<16> address;
  ap_uint<16> read_length;
  AppReadReq() {}
  AppReadReq(TcpSessionID id, ap_uint<16> len) : session_id(id), read_length(len) {}
};

struct AppReadRsp {
  TcpSessionID session_id;
  AppReadRsp() {}
  AppReadRsp(TcpSessionID id) : session_id(id) {}
};

// read/ write DDR buffer internal command
struct MemBufferRWCmd {
  ap_uint<32> addr;
  ap_uint<16> length;
  // next_addr(WINDOW_BITS) == 1 means the buffer is overflow, the cmd need to breakdown
  ap_uint<WINDOW_BITS + 1> next_addr;

  MemBufferRWCmd() {}
  MemBufferRWCmd(ap_uint<32> addr, ap_uint<16> length)
      : addr(addr), length(length), next_addr(addr(WINDOW_BITS - 1, 0) + length) {}
};

//
struct MemBufferRWCmdDoubleAccess {
  bool       double_access;
  ap_uint<6> byte_offset;
  MemBufferRWCmdDoubleAccess() {}
  MemBufferRWCmdDoubleAccess(bool double_access, ap_uint<6> offset)
      : double_access(double_access), byte_offset(offset) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "DoubleAccess?: " << this->double_access << "\t";
    sstream << "ByteOffset: " << byte_offset.to_string(16);
    return sstream.str();
  }
#endif
};

#endif