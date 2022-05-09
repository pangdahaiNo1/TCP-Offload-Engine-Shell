#ifndef _TX_APP_INTF_HPP_
#define _TX_APP_INTF_HPP_

#include "toe/memory_access/memory_access.hpp"
#include "toe/tcp_conn.hpp"
#include "toe/toe_intf.hpp"

struct TxAppTableEntry {
  ap_uint<WINDOW_BITS> ackd;
  ap_uint<WINDOW_BITS> mempt;
#if (TCP_NODELAY)
  ap_uint<WINDOW_BITS> min_window;
#endif
};

struct TxAppToTxAppTableReq {
  ap_uint<16> session_id;
  // ap_uint<16> ackd;
  ap_uint<WINDOW_BITS> mempt;
  bool                 write;
  TxAppToTxAppTableReq() {}
  TxAppToTxAppTableReq(ap_uint<16> id) : session_id(id), mempt(0), write(false) {}
  TxAppToTxAppTableReq(ap_uint<16> id, ap_uint<WINDOW_BITS> pt)
      : session_id(id), mempt(pt), write(true) {}
};

struct TxAppTableToTxAppRsp {
  ap_uint<16>          session_id;
  ap_uint<WINDOW_BITS> ackd;
  ap_uint<WINDOW_BITS> mempt;
#if (TCP_NODELAY)
  ap_uint<WINDOW_BITS> min_window;
#endif
  TxAppTableToTxAppRsp() {}
  TxAppTableToTxAppRsp(ap_uint<16> id, ap_uint<WINDOW_BITS> ackd, ap_uint<WINDOW_BITS> pt)
      : session_id(id), ackd(ackd), mempt(pt) {}
#if (TCP_NODELAY)
  TxAppTableToTxAppRsp(ap_uint<16>          id,
                       ap_uint<WINDOW_BITS> ackd,
                       ap_uint<WINDOW_BITS> pt,
                       ap_uint<WINDOW_BITS> min_window)
      : session_id(id), ackd(ackd), mempt(pt), min_window(min_window) {}
#endif
};

void TxAppConnectionHandler(
    // net app
    stream<NetAXISAppOpenConnReq> & net_app_to_tx_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp> & tx_app_to_net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_to_tx_app_close_conn_req,
    // rx eng
    stream<OpenSessionStatus> &rx_eng_to_tx_app_notification,
    // retrans timer
    stream<OpenSessionStatus> &rtimer_to_tx_app_notification,
    // session lookup, also for TDEST
    stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
    stream<SessionLookupRsp> & slookup_to_tx_app_rsp,
    stream<TcpSessionID> &     tx_app_to_slookup_check_tdest_req,
    stream<NetAXISDest> &      slookup_to_tx_app_check_tdest_rsp,
    // port table req/rsp
    stream<NetAXISDest> &  tx_app_to_ptable_req,
    stream<TcpPortNumber> &ptable_to_tx_app_port_rsp,
    // state table read/write req/rsp
    stream<StateTableReq> &tx_app_to_sttable_req,
    stream<SessionState> & sttable_to_tx_app_rsp,
    // event engine
    stream<Event> &tx_app_to_event_eng_set_event,
    IpAddr &       my_ip_addr);

#endif
