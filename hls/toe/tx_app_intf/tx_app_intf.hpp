#ifndef _TX_APP_INTF_HPP_
#define _TX_APP_INTF_HPP_

#include "toe/memory_access/memory_access.hpp"
#include "toe/tcp_conn.hpp"
#include "toe/toe_intf.hpp"

struct TxAppTableEntry {
  ap_uint<WINDOW_BITS> ackd;
  // the tx buffer memory write pointer, should equals to app_written
  // but app_written will update by DATAMOVER STATS
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
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "Mempt: " << mempt.to_string(16) << "\t";
    sstream << "Write?: " << write << "\t";
    return sstream.str();
  }
#endif
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
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "Ackd: " << ackd.to_string(16) << "\t";
    sstream << "Mempt: " << mempt.to_string(16) << "\t";
    return sstream.str();
  }
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

void TxAppDataHandler(
    // net app
    stream<NetAXISAppTransDataReq> &net_app_to_tx_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_tans_data_rsp,
    stream<NetAXIS> &               net_app_trans_data,
    // tx app table
    stream<TxAppToTxAppTableReq> &tx_app_to_tx_app_table_req,
    stream<TxAppTableToTxAppRsp> &tx_app_table_to_tx_app_rsp,
    // state table
    stream<TcpSessionID> &tx_app_to_sttable_lup_req,
    stream<SessionState> &sttable_to_tx_app_lup_rsp,
    // to event eng
    stream<Event> &tx_app_to_event_eng_set_event,
    // to datamover
    stream<DataMoverCmd> &tx_app_to_mem_write_cmd,
    stream<NetAXIS> &     tx_app_to_mem_write_data);

void tx_app_intf(
    // net app connection request
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
    stream<TcpPortNumber> &ptable_to_tx_app_rsp,
    // state table read/write req/rsp
    stream<StateTableReq> &tx_app_to_sttable_req,
    stream<SessionState> & sttable_to_tx_app_rsp,

    // net app data request
    stream<NetAXISAppTransDataReq> &net_app_to_tx_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_tans_data_rsp,
    stream<NetAXIS> &               net_app_trans_data,
    // tx sar req/rsp
    stream<TxAppToTxSarReq> &tx_app_to_tx_sar_req,
    stream<TxSarToTxAppRsp> &tx_sar_to_tx_app_rsp,
    // to event eng
    stream<Event> &tx_app_to_event_eng_set_event,
    // state table
    stream<TcpSessionID> &tx_app_to_sttable_lup_req,
    stream<SessionState> &sttable_to_tx_app_lup_rsp,
    // datamover req/rsp
    stream<DataMoverCmd> &   tx_app_to_mem_write_cmd,
    stream<NetAXIS> &        tx_app_to_mem_write_data,
    stream<DataMoverStatus> &mem_to_tx_app_write_status,
    // in big endian
    IpAddr &my_ip_addr);

#endif
