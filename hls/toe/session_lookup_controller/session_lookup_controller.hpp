
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

struct ReverseTableEntry {
  ThreeTuple  three_tuple;
  NetAXISDest role_id;
  ReverseTableEntry() {}
  ReverseTableEntry(ThreeTuple tuple, NetAXISDest role) : three_tuple(tuple), role_id(role) {}
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
struct RtlSLookupToCamReq {
  ThreeTuple    key;
  SlookupSource source;
  RtlSLookupToCamReq() {}
  RtlSLookupToCamReq(ThreeTuple tuple, SlookupSource src) : key(tuple), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlSlookupToCamUpdateReq {
  SlookupOp     op;
  ThreeTuple    key;
  TcpSessionID  value;
  SlookupSource source;
  RtlSlookupToCamUpdateReq() {}
  RtlSlookupToCamUpdateReq(ThreeTuple key, TcpSessionID value, SlookupOp op, SlookupSource src)
      : key(key), value(value), op(op), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlCamToSlookupRsp {
  ThreeTuple    key;
  TcpSessionID  session_id;
  bool          hit;
  SlookupSource source;
  RtlCamToSlookupRsp() {}
  RtlCamToSlookupRsp(bool hit, SlookupSource src) : hit(hit), session_id(0), source(src) {}
  RtlCamToSlookupRsp(bool hit, TcpSessionID id, SlookupSource src)
      : hit(hit), session_id(id), source(src) {}
};

/** @ingroup session_lookup_controller
 *
 */
struct RtlCamToSlookupUpdateRsp {
  SlookupOp     op;
  ThreeTuple    key;
  TcpSessionID  session_id;
  bool          success;
  SlookupSource source;
  RtlCamToSlookupUpdateRsp() {}
  RtlCamToSlookupUpdateRsp(SlookupOp op, SlookupSource src) : op(op), source(src) {}
  RtlCamToSlookupUpdateRsp(TcpSessionID id, SlookupOp op, SlookupSource src)
      : session_id(id), op(op), source(src) {}
};

struct SlookupReverseTableInsertReq {
  TcpSessionID key;
  ThreeTuple   tuple_value;
  NetAXISDest  role_value;
  SlookupReverseTableInsertReq(){};
  SlookupReverseTableInsertReq(TcpSessionID key, ThreeTuple tuple, NetAXISDest role)
      : key(key), tuple_value(tuple), role_value(role) {}
};

/** @defgroup session_lookup_controller Session Lookup Controller
 *  @ingroup tcp_module
 */
void session_lookup_controller(stream<RxEngToSlookupReq> &       rxEng2sLookup_req,
                               stream<SessionLookupRsp> &        sLookup2rxEng_rsp,
                               stream<ap_uint<16> > &            stateTable2sLookup_releaseSession,
                               stream<ap_uint<16> > &            sLookup2portTable_releasePort,
                               stream<FourTuple> &               txApp2sLookup_req,
                               stream<SessionLookupRsp> &        sLookup2txApp_rsp,
                               stream<ap_uint<16> > &            txEng2sLookup_rev_req,
                               stream<FourTuple> &               sLookup2txEng_rev_rsp,
                               stream<RtlSLookupToCamReq> &      sessionLookup_req,
                               stream<RtlCamToSlookupRsp> &      sessionLookup_rsp,
                               stream<RtlSlookupToCamUpdateReq> &sessionUpdate_req,
                               stream<RtlCamToSlookupUpdateRsp> &sessionUpdate_rsp,
                               // ap_uint<16>&						relSessionCount,
                               ap_uint<16> &regSessionCount,
                               ap_uint<32>  myIpAddress);
