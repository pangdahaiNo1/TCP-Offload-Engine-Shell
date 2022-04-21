#ifndef _TCP_CONN_
#define _TCP_CONN_

#include "toe/toe_config.hpp"
#include "utils/axi_utils.hpp"

typedef ap_uint<16>          TcpSessionID;
typedef ap_uint<16>          TcpPortNumber;
typedef ap_uint<WINDOW_BITS> TcpSessionBuffer;
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
  TcpSessionBuffer recvd;
  TcpSessionBuffer app_read;
  ap_uint<4>       win_shift;
  TcpSessionBuffer head;
  TcpSessionBuffer offset;
  bool             gap;
};

struct RxSarRsp {
  TcpSessionBuffer recvd;
  ap_uint<4>       win_shift;
  TcpSessionBuffer win_size;
  TcpSessionBuffer used_length;
  RxSarRsp() {}
  RxSarRsp(RxSarTableEntry entry) : recvd(entry.recvd), win_shift(entry.win_shift) {}
};

struct RxSarReq {
  TcpSessionID     session_id;
  TcpSessionBuffer recvd;
  ap_uint<4>       win_shift;
  ap_uint<1>       write;
  ap_uint<1>       init;
  TcpSessionBuffer head;
  TcpSessionBuffer offset;
  bool             gap;
  RxSarReq() {}
  RxSarReq(ap_uint<16> id) : session_id(id), recvd(0), write(0), init(0) {}

  RxSarReq(ap_uint<16> id, ap_uint<32> recvd, ap_uint<32> head, ap_uint<32> offset, bool gap)
      : session_id(id), recvd(recvd), head(head), offset(offset), gap(gap), write(1), init(0) {}
  RxSarReq(ap_uint<16> id,
           ap_uint<32> recvd,
           ap_uint<32> head,
           ap_uint<32> offset,
           bool        gap,
           ap_uint<4>  win_shift)
      : session_id(id)
      , recvd(recvd)
      , head(head)
      , offset(offset)
      , gap(gap)
      , win_shift(win_shift)
      , write(1)
      , init(1) {}
};

struct RxSarTableAppReq {
  TcpSessionID     session_id;
  TcpSessionBuffer app_read;
  ap_uint<1>       write;
  RxSarTableAppReq() {}
  RxSarTableAppReq(TcpSessionID id) : session_id(id), app_read(0), write(0) {}
  RxSarTableAppReq(TcpSessionID id, TcpSessionBuffer appd)
      : session_id(id), app_read(appd), write(1) {}
};

struct RxSarTableAppRsp {
  TcpSessionID     session_id;
  TcpSessionBuffer app_read;
  ap_uint<1>       write;
  RxSarTableAppRsp() {}
  RxSarTableAppRsp(TcpSessionID id) : session_id(id), app_read(0), write(0) {}
  RxSarTableAppRsp(TcpSessionID id, TcpSessionBuffer app_read)
      : session_id(id), app_read(app_read), write(1) {}
};

// Tx SAR Table

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
  // ap_uint<16> dstPort;
};

#endif