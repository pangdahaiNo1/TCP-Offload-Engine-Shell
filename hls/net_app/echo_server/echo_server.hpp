#ifndef _ECHO_SERVER_HPP_
#define _ECHO_SERVER_HPP_
#include "toe/toe_conn.hpp"
using namespace hls;

struct EchoServerMeta {
  TcpSessionID session_id;
  ap_uint<16>  length;  // data length
};
/** @defgroup echo_server_application Echo Server Application
 *
 */
void echo_server(
    // listen port
    stream<NetAXISListenPortReq> &net_app_listen_port_req,
    stream<NetAXISListenPortRsp> &net_app_listen_port_rsp,
    // rx client notify
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    // rx app notify
    stream<NetAXISAppNotification> &net_app_notification,
    // read data req/rsp
    stream<NetAXISAppReadReq> &net_app_recv_data_req,
    stream<NetAXISAppReadRsp> &net_app_recv_data_rsp,
    // read data
    stream<NetAXIS> &net_app_recv_data,
    // open/close conn
    stream<NetAXISAppOpenConnReq> & net_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp> & net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_close_conn_req,
    // transmit data
    stream<NetAXISAppTransDataReq> &net_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &net_app_trans_data_rsp,
    stream<NetAXIS> &               net_app_trans_data,
    // tdest constant
    NetAXISDest &tdest_const);
#endif
