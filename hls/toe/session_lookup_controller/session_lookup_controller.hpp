
#include "toe/tcp_conn.hpp"

using namespace hls;

/** @ingroup session_lookup_controller
 *
 */
enum SlookupSource { RX, TX_APP };

/** @ingroup session_lookup_controller
 *
 */
enum SlookupOp { INSERT, DELETE };

/** @ingroup session_lookup_controller
 *  This struct defines the internal storage format of the IP tuple instead of destiantion and
 * source, my and their is used. When a tuple is sent or received from the tx/rx path it is mapped
 * to the fourTuple struct. The < operator is necessary for the c++ dummy memory implementation
 * which uses an std::map
 */
struct ThreeTuple {
  IpAddr        their_ip_addr;
  TcpPortNumber here_tcp_port;
  TcpPortNumber there_tcp_port;
  ThreeTuple() {}
  ThreeTuple(IpAddr their_ip_addr, TcpPortNumber here_tcp_port, TcpPortNumber there_tcp_port)
      : their_ip_addr(their_ip_addr)
      , here_tcp_port(here_tcp_port)
      , there_tcp_port(there_tcp_port) {}

  bool operator<(const ThreeTuple &other) const {
    if (their_ip_addr < other.their_ip_addr) {
      return true;
    } else if (their_ip_addr == other.their_ip_addr) {
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
};

struct RevTableEntry {
  ThreeTuple  three_tuple;
  NetAXISDest role_id;
  RevTableEntry() {}
  RevTableEntry(ThreeTuple tuple, NetAXISDest role) : three_tuple(tuple), role_id(role) {}
};
/** @ingroup session_lookup_controller
 *
 */
struct SlookupReqInternal {
  ThreeTuple    tuple;
  bool          allow_creation;
  SlookupSource source;
  NetAXISDest   role_id;
  SlookupReqInternal() {}
  SlookupReqInternal(ThreeTuple tuple, bool allow_creation, SlookupSource src, NetAXISDest role_id)
      : tuple(tuple), allow_creation(allow_creation), source(src), role_id(role_id) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlSLookupToCamLupReq {
  ThreeTuple    key;
  SlookupSource source;
  RtlSLookupToCamLupReq() {}
  RtlSLookupToCamLupReq(ThreeTuple tuple, SlookupSource src) : key(tuple), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlSlookupToCamUpdReq {
  SlookupOp     op;
  ThreeTuple    key;
  TcpSessionID  value;
  SlookupSource source;
  RtlSlookupToCamUpdReq() {}
  RtlSlookupToCamUpdReq(ThreeTuple key, TcpSessionID value, SlookupOp op, SlookupSource src)
      : key(key), value(value), op(op), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlCamToSlookupLupRsp {
  ThreeTuple    key;
  TcpSessionID  session_id;
  bool          hit;
  SlookupSource source;
  RtlCamToSlookupLupRsp() {}
  RtlCamToSlookupLupRsp(bool hit, SlookupSource src) : hit(hit), session_id(0), source(src) {}
  RtlCamToSlookupLupRsp(bool hit, TcpSessionID id, SlookupSource src)
      : hit(hit), session_id(id), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlCamToSlookupUpdRsp {
  SlookupOp     op;
  ThreeTuple    key;
  TcpSessionID  session_id;
  bool          success;
  SlookupSource source;
  RtlCamToSlookupUpdRsp() {}
  RtlCamToSlookupUpdRsp(SlookupOp op, SlookupSource src) : op(op), source(src) {}
  RtlCamToSlookupUpdRsp(TcpSessionID id, SlookupOp op, SlookupSource src)
      : session_id(id), op(op), source(src) {}
};

struct SlookupToRevTableUpdReq {
  TcpSessionID key;
  ThreeTuple   tuple_value;
  NetAXISDest  role_value;
  SlookupToRevTableUpdReq(){};
  SlookupToRevTableUpdReq(TcpSessionID key, ThreeTuple tuple, NetAXISDest role)
      : key(key), tuple_value(tuple), role_value(role) {}
};

/** @defgroup session_lookup_controller Session Lookup Controller
 *  @ingroup tcp_module
 */
void session_lookup_controller(
    // from sttable
    stream<TcpSessionID> &sttable_to_slookup_release_session_req,
    // rx app
    stream<TcpSessionID> &rx_app_to_slookup_req,
    stream<NetAXISDest> & slookup_to_rx_app_rsp,
    // rx eng
    stream<RxEngToSlookupReq> &rx_eng_to_slookup_req,
    stream<SessionLookupRsp> & slookup_to_rx_eng_rsp,
    // tx app
    stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
    stream<SessionLookupRsp> & slookup_to_tx_app_rsp,
    stream<TcpSessionID> &     tx_app_to_slookup_check_tdest_req,
    stream<NetAXISDest> &      slookup_to_tx_app_check_tdest_rsp,
    // tx eng
    stream<ap_uint<16> > &          tx_eng_to_slookup_rev_table_req,
    stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
    // CAM
    stream<RtlSLookupToCamLupReq> &rtl_slookup_to_cam_lookup_req,
    stream<RtlCamToSlookupLupRsp> &rtl_cam_to_slookup_lookup_rsp,
    stream<RtlSlookupToCamUpdReq> &rtl_slookup_to_cam_update_req,
    stream<RtlCamToSlookupUpdRsp> &rtl_cam_to_slookup_update_rsp,
    // to ptable
    stream<TcpPortNumber> &slookup_to_ptable_release_port_req,
    // registers
    ap_uint<16> &reg_session_cnt,
    // in big endian
    IpAddr &my_ip_addr);
