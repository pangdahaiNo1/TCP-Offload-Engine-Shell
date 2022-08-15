#ifndef _TX_APP_INTF_HPP_
#define _TX_APP_INTF_HPP_

#include "toe/memory_access/memory_access.hpp"
#include "toe/toe_conn.hpp"

struct TxAppTableEntry {
  TcpSessionBuffer ackd;
  // the tx buffer memory write pointer, should equals to app_written
  // but app_written will update by DATAMOVER STATS
  TcpSessionBuffer app_written_ideal;
#if (TCP_NODELAY)
  TcpSessionBuffer min_window;
#endif
};

struct TxAppToTxAppTableReq {
  ap_uint<16>      session_id;
  TcpSessionBuffer app_written_ideal;
  bool             write;
  TxAppToTxAppTableReq() : session_id(0), app_written_ideal(0), write(0) {}
  // read req
  TxAppToTxAppTableReq(ap_uint<16> id) : session_id(id), app_written_ideal(0), write(false) {}
  // write req
  TxAppToTxAppTableReq(ap_uint<16> id, TcpSessionBuffer app_written_ideal)
      : session_id(id), app_written_ideal(app_written_ideal), write(true) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "AppWriteIdeal: " << app_written_ideal.to_string(16) << "\t";
    sstream << "Write?: " << write << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

struct TxAppTableToTxAppRsp {
  ap_uint<16>      session_id;
  TcpSessionBuffer ackd;
  TcpSessionBuffer app_written_ideal;
#if (TCP_NODELAY)
  TcpSessionBuffer min_window;
#endif
  TxAppTableToTxAppRsp() : session_id(0), ackd(0), app_written_ideal(0) {}
  TxAppTableToTxAppRsp(ap_uint<16> id, TcpSessionBuffer ackd, TcpSessionBuffer app_written_ideal)
      : session_id(id), ackd(ackd), app_written_ideal(app_written_ideal) {}
#if (TCP_NODELAY)
  TxAppTableToTxAppRsp(ap_uint<16>      id,
                       TcpSessionBuffer ackd,
                       TcpSessionBuffer app_written_ideal,
                       TcpSessionBuffer min_window)
      : session_id(id), ackd(ackd), app_written_ideal(app_written_ideal), min_window(min_window) {}
#endif
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SessionID: " << session_id.to_string(16) << "\t";
    sstream << "Ackd: " << ackd.to_string(16) << "\t";
    sstream << "AppWriteIdeal: " << app_written_ideal.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

void TxAppConnectionHandler(
    // net app
    stream<NetAXISAppOpenConnReq>  &net_app_to_tx_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp>  &tx_app_to_net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_to_tx_app_close_conn_req,
    // rx eng -> net app
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    // rx eng
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // retrans timer
    stream<OpenConnRspNoTDEST> &rtimer_to_tx_app_notification,
    // session lookup, also for TDEST
    stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
    stream<SessionLookupRsp>  &slookup_to_tx_app_rsp,
    stream<TcpSessionID>      &tx_app_to_slookup_check_tdest_req,
    stream<NetAXISDest>       &slookup_to_tx_app_check_tdest_rsp,
    // port table req/rsp
    stream<NetAXISDest>   &tx_app_to_ptable_req,
    stream<TcpPortNumber> &ptable_to_tx_app_rsp,
    // state table read/write req/rsp
    stream<StateTableReq> &tx_app_to_sttable_req,
    stream<SessionState>  &sttable_to_tx_app_rsp,
    // event engine
    stream<Event> &tx_app_conn_handler_to_event_engine,
    IpAddr        &my_ip_addr);

void TxAppDataHandler(
    // net app
    stream<NetAXISAppTransDataReq> &net_app_to_tx_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_trans_data_rsp,
    stream<NetAXIS>                &net_app_trans_data,
    // tx app table
    stream<TxAppToTxAppTableReq> &tx_app_to_tx_app_table_req,
    stream<TxAppTableToTxAppRsp> &tx_app_table_to_tx_app_rsp,
    // state table
    stream<TcpSessionID> &tx_app_to_sttable_lup_req,
    stream<SessionState> &sttable_to_tx_app_lup_rsp,
    // to event eng
    stream<Event> &tx_app_to_event_eng_set_event,
    // to datamover
    stream<MemBufferRWCmd> &tx_app_to_mover_write_cmd,
    stream<NetAXISWord>    &tx_app_to_mover_write_data);

void TxAppRspHandler(stream<ap_uint<1> >     &mem_buffer_double_access_flag,
                     stream<DataMoverStatus> &mover_to_tx_app_write_status,
                     stream<Event>           &tx_app_to_event_eng_cache,
                     stream<Event>           &tx_app_to_event_eng_set_event,
                     stream<TxAppToTxSarReq> &tx_app_to_tx_sar_upd_req);

void TxAppTableInterface(
    // tx sar update tx app table
    stream<TxSarToTxAppReq>      &tx_sar_to_tx_app_upd_req,
    stream<TxAppToTxAppTableReq> &tx_app_to_tx_app_table_req,
    stream<TxAppTableToTxAppRsp> &tx_app_table_to_tx_app_rsp);

void tx_app_intf(
    // net app connection request
    stream<NetAXISAppOpenConnReq>  &net_app_to_tx_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp>  &tx_app_to_net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_to_tx_app_close_conn_req,
    // rx eng -> net app
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    // rx eng
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // retrans timer
    stream<OpenConnRspNoTDEST> &rtimer_to_tx_app_notification,
    // session lookup, also for TDEST
    stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
    stream<SessionLookupRsp>  &slookup_to_tx_app_rsp,
    stream<TcpSessionID>      &tx_app_to_slookup_check_tdest_req,
    stream<NetAXISDest>       &slookup_to_tx_app_check_tdest_rsp,
    // port table req/rsp
    stream<NetAXISDest>   &tx_app_to_ptable_req,
    stream<TcpPortNumber> &ptable_to_tx_app_rsp,
    // state table read/write req/rsp
    stream<StateTableReq> &tx_app_to_sttable_req,
    stream<SessionState>  &sttable_to_tx_app_rsp,

    // net app data request
    stream<NetAXISAppTransDataReq> &net_app_to_tx_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_trans_data_rsp,
    stream<NetAXIS>                &net_app_trans_data,
    // to/from tx sar upd req
    stream<TxAppToTxSarReq> &tx_app_to_tx_sar_upd_req,
    stream<TxSarToTxAppReq> &tx_sar_to_tx_app_upd_req,
    // to event eng
    stream<Event> &tx_app_to_event_eng_set_event,
    // state table
    stream<TcpSessionID> &tx_app_to_sttable_lup_req,
    stream<SessionState> &sttable_to_tx_app_lup_rsp,
    // datamover req/rsp
    stream<DataMoverCmd>    &tx_app_to_mover_write_cmd,
    stream<NetAXIS>         &tx_app_to_mover_write_data,
    stream<DataMoverStatus> &mover_to_tx_app_write_status,
    // in big endian
    IpAddr &my_ip_addr);

#endif
