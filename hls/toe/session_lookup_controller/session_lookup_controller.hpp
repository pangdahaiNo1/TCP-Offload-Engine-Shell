
#include "toe/toe_conn.hpp"

using namespace hls;

struct RevTableEntry {
  ThreeTuple  three_tuple;
  NetAXISDest role_id;
  RevTableEntry() : three_tuple(), role_id(INVALID_TDEST) {}
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
  SlookupReqInternal() : tuple(), allow_creation(0), source(RX), role_id(INVALID_TDEST) {}
  SlookupReqInternal(ThreeTuple tuple, bool allow_creation, SlookupSource src, NetAXISDest role_id)
      : tuple(tuple), allow_creation(allow_creation), source(src), role_id(role_id) {}
};

struct SlookupToRevTableUpdReq {
  TcpSessionID key;
  ThreeTuple   tuple_value;
  NetAXISDest  role_value;
  SlookupToRevTableUpdReq(){};
  SlookupToRevTableUpdReq(TcpSessionID key, ThreeTuple tuple, NetAXISDest role)
      : key(key), tuple_value(tuple), role_value(role) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Key " << key.to_string(16) << "\t"
            << "ThreeTuple " << tuple_value.to_string() << "\t"
            << "Role " << role_value << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

/** @defgroup session_lookup_controller Session Lookup Controller
 *  @ingroup tcp_module
 */
void session_lookup_controller(
    // from sttable
    stream<TcpSessionID> &sttable_to_slookup_release_req,
    // rx app
    stream<TcpSessionID> &rx_app_to_slookup_tdest_lookup_req,
    stream<NetAXISDest> & slookup_to_rx_app_tdest_lookup_rsp,
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
    IpAddr &     my_ip_addr);