#ifndef _TCP_CONN_
#define _TCP_CONN_

#include "toe/toe_config.hpp"
#include "utils/axi_utils.hpp"

typedef ap_uint<16>                TcpSessionID;
typedef ap_uint<16>                TcpPortNumber;
typedef ap_uint<WINDOW_SCALE_BITS> TcpSessionBufferScale;
typedef ap_uint<WINDOW_BITS>       TcpSessionBuffer;
typedef ap_uint<32>                IpAddr;

#define CLOCK_PERIOD 0.003103
#ifndef __SYNTHESIS__
static const ap_uint<32> TIME_64us  = 1;
static const ap_uint<32> TIME_128us = 1;
static const ap_uint<32> TIME_1ms   = 1;
static const ap_uint<32> TIME_5ms   = 1;
static const ap_uint<32> TIME_25ms  = 1;
static const ap_uint<32> TIME_50ms  = 1;
static const ap_uint<32> TIME_100ms = 1;
static const ap_uint<32> TIME_250ms = 1;
static const ap_uint<32> TIME_500ms = 1;
static const ap_uint<32> TIME_1s    = 1;
static const ap_uint<32> TIME_5s    = 1;
static const ap_uint<32> TIME_7s    = 2;
static const ap_uint<32> TIME_10s   = 3;
static const ap_uint<32> TIME_15s   = 4;
static const ap_uint<32> TIME_20s   = 5;
static const ap_uint<32> TIME_30s   = 6;
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
  bool          open_successfully;
  bool          wrong_port_number;
  bool          already_open;
  TcpPortNumber port_number;
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

struct NetAXISListenPortReq {
  TcpPortNumber data;
  NetAXISDest   dest;
};

struct NetAXISListenPortRsp {
  ListenPortRsp data;
  NetAXISDest   dest;
};

struct OpenSessionStatus {
  TcpSessionID session_id;
  bool         success;
  OpenSessionStatus() {}
  OpenSessionStatus(TcpSessionID id, bool success) : session_id(id), success(success) {}
};

struct FourTuple {
  IpAddr        src_ip_addr;
  IpAddr        dst_ip_addr;
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
};

// RX SAR Table
/** @ingroup rx_sar_table
 *  @ingroup rx_engine
 *  @ingroup tx_engine
 *  This struct defines an entry of the @ref rx_sar_table
 */

/*	A gap caused by out-of-order packet:
 *	                v- app_read     v- recvd    v- offset        v- head
 *	|  <- undef ->  |  <committed>  |   <gap>   |  <pre-mature>  |  <- undef ->  |
 */
struct RxSarTableEntry {
  TcpSessionBufferScale win_shift;
  TcpSessionBuffer      app_read;
  TcpSessionBuffer      recvd;
  TcpSessionBuffer      offset;
  TcpSessionBuffer      head;
  bool                  gap;
};

struct RxSarLookupRsp {
  TcpSessionBuffer      recvd;
  TcpSessionBufferScale win_shift;
  // win_size is 16 bit, in tcp header win
  ap_uint<16> win_size;
  // TcpSessionBuffer used_length;
  RxSarLookupRsp() {}
  RxSarLookupRsp(TcpSessionBuffer recd, TcpSessionBufferScale win_shift)
      : recvd(recvd), win_size(0), win_shift(win_shift) {}
};

// Lookup or Update Rx SAR table when received new segment, or want to send new segment
struct RxSarSegReq {
  TcpSessionID          session_id;
  TcpSessionBufferScale win_shift;
  ap_uint<1>            write;
  ap_uint<1>            init;
  bool                  gap;

  TcpSessionBuffer recvd;
  TcpSessionBuffer head;
  TcpSessionBuffer offset;
  RxSarSegReq() {}
  RxSarSegReq(TcpSessionID id) : session_id(id), recvd(0), write(0), init(0) {}

  RxSarSegReq(TcpSessionID     id,
              TcpSessionBuffer recvd,
              TcpSessionBuffer head,
              TcpSessionBuffer offset,
              bool             gap)
      : session_id(id), recvd(recvd), head(head), offset(offset), gap(gap), write(1), init(0) {}
  RxSarSegReq(TcpSessionID          id,
              TcpSessionBuffer      recvd,
              TcpSessionBuffer      head,
              TcpSessionBuffer      offset,
              bool                  gap,
              TcpSessionBufferScale win_shift)
      : session_id(id)
      , recvd(recvd)
      , head(head)
      , offset(offset)
      , gap(gap)
      , win_shift(win_shift)
      , write(1)
      , init(1) {}
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
};

// Tx SAR Table
/** @ingroup tx_sar_table
 *  @ingroup rx_engine
 *  @ingroup tx_engine
 *  This struct defines an entry of the @ref tx_sar_table
 */
/*
 *	                v-acked             v-tx'ed/notacked  v-app_written
 *	|  <- undef ->  |  <tx'ed not ack>  |   <not tx'ed>   |  <- undef ->  |
 */
struct TxSarTableEntry {
  TcpSessionBuffer ackd;
  TcpSessionBuffer not_ackd;
  TcpSessionBuffer app_written;

  ap_uint<16>           recv_window;
  TcpSessionBufferScale win_shift;

  TcpSessionBuffer cong_window;
  TcpSessionBuffer slowstart_threshold;
  ap_uint<2>       count;
  bool             fast_retrans;
  bool             fin_is_ready;
  bool             fin_is_sent;
};

struct TxSarUpdateFromAckedSegReq {
  TcpSessionID          session_id;
  TcpSessionBuffer      ackd;
  ap_uint<16>           recv_window;
  TcpSessionBuffer      cong_window;
  ap_uint<2>            count;
  bool                  fast_retrans;
  TcpSessionBufferScale win_shift;
  ap_uint<1>            write;
  ap_uint<1>            init;
  TxSarUpdateFromAckedSegReq() {}
  TxSarUpdateFromAckedSegReq(TcpSessionID id)
      : session_id(id)
      , ackd(0)
      , recv_window(0)
      , cong_window(0)
      , count(0)
      , fast_retrans(false)
      , write(0)
      , init(0) {}
  TxSarUpdateFromAckedSegReq(TcpSessionID         id,
                             TcpSessionBuffer     ackd,
                             ap_uint<16>          recv_win,
                             ap_uint<WINDOW_BITS> cong_win,
                             ap_uint<2>           count,
                             bool                 fast_retrans)
      : session_id(id)
      , ackd(ackd)
      , recv_window(recv_win)
      , cong_window(cong_win)
      , count(count)
      , fast_retrans(fast_retrans)
      , write(1)
      , init(0) {}
  TxSarUpdateFromAckedSegReq(TcpSessionID     id,
                             TcpSessionBuffer ackd,
                             ap_uint<16>      recv_win,
                             TcpSessionBuffer cong_win,
                             ap_uint<2>       count,
                             bool             fast_retrans,
                             ap_uint<4>       win_shift)
      : session_id(id)
      , ackd(ackd)
      , recv_window(recv_win)
      , cong_window(cong_win)
      , count(count)
      , fast_retrans(fast_retrans)
      , win_shift(win_shift)
      , write(1)
      , init(1) {}
};

struct TxEngToTxSarReq {
  TcpSessionID sessionID;
  ap_uint<32>  not_ackd;
  ap_uint<1>   write;
  ap_uint<1>   init;
  bool         fin_is_ready;
  bool         fin_is_sent;
  bool         isRtQuery;
  TxEngToTxSarReq() {}
  TxEngToTxSarReq(ap_uint<16> id)
      : sessionID(id)
      , not_ackd(0)
      , write(0)
      , init(0)
      , fin_is_ready(false)
      , fin_is_sent(false)
      , isRtQuery(false) {}
  TxEngToTxSarReq(ap_uint<16> id, ap_uint<32> not_ackd, ap_uint<1> write)
      : sessionID(id)
      , not_ackd(not_ackd)
      , write(write)
      , init(0)
      , fin_is_ready(false)
      , fin_is_sent(false)
      , isRtQuery(false) {}
  TxEngToTxSarReq(ap_uint<16> id, ap_uint<32> not_ackd, ap_uint<1> write, ap_uint<1> init)
      : sessionID(id)
      , not_ackd(not_ackd)
      , write(write)
      , init(init)
      , fin_is_ready(false)
      , fin_is_sent(false)
      , isRtQuery(false) {}
  TxEngToTxSarReq(ap_uint<16> id,
                  ap_uint<32> not_ackd,
                  ap_uint<1>  write,
                  ap_uint<1>  init,
                  bool        fin_is_ready,
                  bool        fin_is_sent)
      : sessionID(id)
      , not_ackd(not_ackd)
      , write(write)
      , init(init)
      , fin_is_ready(fin_is_ready)
      , fin_is_sent(fin_is_sent)
      , isRtQuery(false) {}
  TxEngToTxSarReq(ap_uint<16> id,
                  ap_uint<32> not_ackd,
                  ap_uint<1>  write,
                  ap_uint<1>  init,
                  bool        fin_is_ready,
                  bool        fin_is_sent,
                  bool        isRt)
      : sessionID(id)
      , not_ackd(not_ackd)
      , write(write)
      , init(init)
      , fin_is_ready(fin_is_ready)
      , fin_is_sent(fin_is_sent)
      , isRtQuery(isRt) {}
};

struct TxSarToRxEngRsp {
  ap_uint<32>          prevAck;
  ap_uint<32>          nextByte;
  ap_uint<WINDOW_BITS> cong_window;
  ap_uint<WINDOW_BITS> slowstart_threshold;
  ap_uint<2>           count;
  bool                 fast_retrans;
  TxSarToRxEngRsp() {}
  TxSarToRxEngRsp(ap_uint<32>          ack,
                  ap_uint<32>          next,
                  ap_uint<WINDOW_BITS> cong_win,
                  ap_uint<WINDOW_BITS> sstresh,
                  ap_uint<2>           count,
                  bool                 fast_retrans)
      : prevAck(ack)
      , nextByte(next)
      , cong_window(cong_win)
      , slowstart_threshold(sstresh)
      , count(count)
      , fast_retrans(fast_retrans) {}
};

// when app write new data to session buffer, update the Tx SAR Table, no response for this request
struct TxSarUpdateAppReq {
  TcpSessionID     session_id;
  TcpSessionBuffer app_written;
  TxSarUpdateAppReq() {}
  TxSarUpdateAppReq(TcpSessionID id, TcpSessionBuffer app) : session_id(id), app_written(app) {}
};

struct TxSarToAppRsp {
  ap_uint<16>      sessionID;
  TcpSessionBuffer ackd;
#if (TCP_NODELAY)
  TcpSessionBuffer min_window;
#endif
  ap_uint<1> init;
  TxSarToAppRsp() {}
#if !(TCP_NODELAY)
  TxSarToAppRsp(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd) : sessionID(id), ackd(ackd), init(0) {}
  TxSarToAppRsp(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd, ap_uint<1> init)
      : sessionID(id), ackd(ackd), init(init) {}
#else
  TxSarToAppRsp(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd, ap_uint<WINDOW_BITS> min_window)
      : sessionID(id), ackd(ackd), min_window(min_window), init(0) {}
  TxSarToAppRsp(ap_uint<16>          id,
                ap_uint<WINDOW_BITS> ackd,
                ap_uint<WINDOW_BITS> min_window,
                ap_uint<1>           init)
      : sessionID(id), ackd(ackd), min_window(min_window), init(init) {}
#endif
};

struct TxSarToTxEngRsp {
  ap_uint<32>          ackd;
  ap_uint<32>          not_ackd;
  ap_uint<WINDOW_BITS> usableWindow;
  ap_uint<WINDOW_BITS> app;
  ap_uint<WINDOW_BITS> usedLength;
  bool                 fin_is_ready;
  bool                 fin_is_sent;
  //#if (WINDOW_SCALE)
  ap_uint<4> win_shift;
  //#endif
  TxSarToTxEngRsp() {}
  TxSarToTxEngRsp(ap_uint<32>          ack,
                  ap_uint<32>          nack,
                  ap_uint<WINDOW_BITS> usableWindow,
                  ap_uint<WINDOW_BITS> app,
                  ap_uint<WINDOW_BITS> usedLength,
                  bool                 fin_is_ready,
                  bool                 fin_is_sent)
      : ackd(ack)
      , not_ackd(nack)
      , usableWindow(usableWindow)
      , app(app)
      , usedLength(usedLength)
      , fin_is_ready(fin_is_ready)
      , fin_is_sent(fin_is_sent) {}
};

struct TxSarRtReq : public TxEngToTxSarReq {
  TxSarRtReq() {}
  TxSarRtReq(const TxEngToTxSarReq &q)
      : TxEngToTxSarReq(
            q.sessionID, q.not_ackd, q.write, q.init, q.fin_is_ready, q.fin_is_sent, q.isRtQuery) {}
  TxSarRtReq(ap_uint<16> id, ap_uint<WINDOW_BITS> ssthresh)
      : TxEngToTxSarReq(id, ssthresh, 1, 0, false, false, true) {}
  ap_uint<WINDOW_BITS> getThreshold() { return not_ackd(WINDOW_BITS - 1, 0); }
};

struct TcpConnMetaData {
  // ap_uint<16> session_id;
  ap_uint<32> seqNumb;
  ap_uint<32> ackNumb;
  ap_uint<16> winSize;
  //#if (WINDOW_SCALE)
  ap_uint<4> winScale;
  //#endif
  ap_uint<16> length;
  ap_uint<1>  ack;
  ap_uint<1>  rst;
  ap_uint<1>  syn;
  ap_uint<1>  fin;
  ap_uint<4>  dataOffset;
  // ap_uint<16> dst_tcp_port;
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
enum EventType { TX, RT, ACK, SYN, SYN_ACK, FIN, RST, ACK_NODELAY };
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
};

struct EventWithTuple : public Event {
  FourTuple four_tuple;
  EventWithTuple() {}
  EventWithTuple(const Event &ev)
      : Event(ev.type, ev.session_id, ev.buf_addr, ev.length, ev.retrans_cnt) {}
  EventWithTuple(const Event &ev, FourTuple tuple)
      : Event(ev.type, ev.session_id, ev.buf_addr, ev.length, ev.retrans_cnt), four_tuple(tuple) {}
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
  TxEngToRetransTimerReq(TcpSessionID id) : session_id(id), type(RT) {}  // FIXME??
  TxEngToRetransTimerReq(TcpSessionID id, EventType type) : session_id(id), type(type) {}
};

struct SessionLookupReq {
  FourTuple four_tuple;
  bool      allow_creation;
  SessionLookupReq() {}
  SessionLookupReq(FourTuple tuple, bool allow_creation)
      : four_tuple(tuple), allow_creation(allow_creation) {}
};

struct SessionLookupRsp {
  TcpSessionID session_id;
  bool         hit;
  SessionLookupRsp() {}
  SessionLookupRsp(TcpSessionID session_id, bool hit) : session_id(session_id), hit(hit) {}
};

#endif