#ifndef _MOCK_NET_APP_HPP_
#define _MOCK_NET_APP_HPP_

#include "toe/toe_conn.hpp"

#include <string>

using std::string;
struct NetAppIntf {
#if MULTI_IP_ADDR
  IpAddr my_ip_addr;
#else
#endif
  // tdest constant
  NetAXISDest tdest_const;
  // listen port
  stream<NetAXISListenPortReq> net_app_listen_port_req;
  stream<NetAXISListenPortRsp> net_app_listen_port_rsp;
  // rx client notify
  stream<NetAXISNewClientNotification> net_app_new_client_notification;
  // rx app notify
  stream<NetAXISAppNotification> net_app_notification;
  // read data req/rsp
  stream<NetAXISAppReadReq> net_app_recv_data_req;
  stream<NetAXISAppReadRsp> net_app_recv_data_rsp;
  // read data
  stream<NetAXIS> net_app_recv_data;
  // open/close conn
  stream<NetAXISAppOpenConnReq>  net_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>  net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq> net_app_close_conn_req;
  // transmit data
  stream<NetAXISAppTransDataReq> net_app_trans_data_req;
  stream<NetAXISAppTransDataRsp> net_app_trans_data_rsp;
  stream<NetAXIS>                net_app_trans_data;
#if MULTI_IP_ADDR
  NetAppIntf(IpAddr ip_addr, NetAXISDest tdest, string fifo_suffix) {
    my_ip_addr = ip_addr;
#else
  NetAppIntf(NetAXISDest tdest, string fifo_suffix) {
#endif
    tdest_const = tdest;
    // set fifo name
    net_app_listen_port_req.set_name(("net_app_listen_port_req" + fifo_suffix).c_str());
    net_app_listen_port_rsp.set_name(("net_app_listen_port_rsp" + fifo_suffix).c_str());
    net_app_new_client_notification.set_name(
        ("net_app_new_client_notification" + fifo_suffix).c_str());
    net_app_notification.set_name(("net_app_notification" + fifo_suffix).c_str());
    net_app_recv_data_req.set_name(("net_app_recv_data_req" + fifo_suffix).c_str());
    net_app_recv_data_rsp.set_name(("net_app_recv_data_rsp" + fifo_suffix).c_str());
    net_app_open_conn_req.set_name(("net_app_open_conn_req" + fifo_suffix).c_str());
    net_app_open_conn_rsp.set_name(("net_app_open_conn_rsp" + fifo_suffix).c_str());
    net_app_close_conn_req.set_name(("net_app_close_conn_req" + fifo_suffix).c_str());
    net_app_recv_data.set_name(("net_app_recv_data" + fifo_suffix).c_str());
    net_app_trans_data_req.set_name(("net_app_trans_data_req" + fifo_suffix).c_str());
    net_app_trans_data_rsp.set_name(("net_app_trans_data_rsp" + fifo_suffix).c_str());
    net_app_trans_data.set_name(("net_app_trans_data" + fifo_suffix).c_str());
  }
};

void MergeStreamNetAppToToe(
    // listen port req
    stream<NetAXISListenPortReq> &net_app_listen_port_req0,
    stream<NetAXISListenPortReq> &net_app_listen_port_req1,
    stream<NetAXISListenPortReq> &net_app_to_toe_listen_port_req,
    // read data req
    stream<NetAXISAppReadReq> &net_app_recv_data_req0,
    stream<NetAXISAppReadReq> &net_app_recv_data_req1,
    stream<NetAXISAppReadReq> &net_app_recv_data_req,
    // open conn req
    stream<NetAXISAppOpenConnReq> &net_app_open_conn_req0,
    stream<NetAXISAppOpenConnReq> &net_app_open_conn_req1,
    stream<NetAXISAppOpenConnReq> &net_app_to_toe_open_conn_req,
    // close conn req
    stream<NetAXISAppCloseConnReq> &net_app_close_conn_req0,
    stream<NetAXISAppCloseConnReq> &net_app_close_conn_req1,
    stream<NetAXISAppCloseConnReq> &net_app_to_toe_close_conn_req,
    // trans data req
    stream<NetAXISAppTransDataReq> &net_app_trans_data_req0,
    stream<NetAXISAppTransDataReq> &net_app_trans_data_req1,
    stream<NetAXISAppTransDataReq> &net_app_to_toe_trans_data_req,
    // trans data
    stream<NetAXIS> &net_app_trans_data0,
    stream<NetAXIS> &net_app_trans_data1,
    stream<NetAXIS> &net_app_to_toe_trans_data) {
  AxiStreamMerger(
      net_app_listen_port_req0, net_app_listen_port_req1, net_app_to_toe_listen_port_req);
  AxiStreamMerger(net_app_recv_data_req0, net_app_recv_data_req1, net_app_recv_data_req);
  AxiStreamMerger(net_app_open_conn_req0, net_app_open_conn_req1, net_app_to_toe_open_conn_req);
  AxiStreamMerger(net_app_close_conn_req0, net_app_close_conn_req1, net_app_to_toe_close_conn_req);
  AxiStreamMerger(net_app_trans_data_req0, net_app_trans_data_req1, net_app_to_toe_trans_data_req);
  AxiStreamMerger(net_app_trans_data0, net_app_trans_data1, net_app_to_toe_trans_data);
}

void SwitchStreamToeToNetApp(
    // listen port rsp
    stream<NetAXISListenPortRsp> &toe_listen_port_rsp,
    stream<NetAXISListenPortRsp> &net_app_listen_port_rsp0,
    stream<NetAXISListenPortRsp> &net_app_listen_port_rsp1,
    // rx client notify
    stream<NetAXISNewClientNotification> &toe_new_client_notification,
    stream<NetAXISNewClientNotification> &net_app_new_client_notification0,
    stream<NetAXISNewClientNotification> &net_app_new_client_notification1,
    // rx data notify
    stream<NetAXISAppNotification> &toe_notification,
    stream<NetAXISAppNotification> &net_app_notification0,
    stream<NetAXISAppNotification> &net_app_notification1,
    // open conn rsp
    stream<NetAXISAppOpenConnRsp> &toe_open_conn_rsp,
    stream<NetAXISAppOpenConnRsp> &net_app_open_conn_rsp0,
    stream<NetAXISAppOpenConnRsp> &net_app_open_conn_rsp1,
    // recv data
    stream<NetAXIS> &toe_recv_data,
    stream<NetAXIS> &net_app_recv_data0,
    stream<NetAXIS> &net_app_recv_data1,

    // trans data rsp
    stream<NetAXISAppTransDataRsp> &toe_trans_data_rsp,
    stream<NetAXISAppTransDataRsp> &net_app_trans_data_rsp0,
    stream<NetAXISAppTransDataRsp> &net_app_trans_data_rsp1,
    // net app 0 tdest
    NetAXISDest tdest_const0,
    // net app 1 tdest
    NetAXISDest tdest_const1) {
  AxiStreamSwitch(toe_listen_port_rsp,
                  net_app_listen_port_rsp0,
                  net_app_listen_port_rsp1,
                  tdest_const0,
                  tdest_const1);
  AxiStreamSwitch(toe_new_client_notification,
                  net_app_new_client_notification0,
                  net_app_new_client_notification1,
                  tdest_const0,
                  tdest_const1);
  AxiStreamSwitch(
      toe_notification, net_app_notification0, net_app_notification1, tdest_const0, tdest_const1);
  AxiStreamSwitch(toe_open_conn_rsp,
                  net_app_open_conn_rsp0,
                  net_app_open_conn_rsp1,
                  tdest_const0,
                  tdest_const1);
  AxiStreamSwitch(
      toe_recv_data, net_app_recv_data0, net_app_recv_data1, tdest_const0, tdest_const1);
  AxiStreamSwitch(toe_trans_data_rsp,
                  net_app_trans_data_rsp0,
                  net_app_trans_data_rsp1,
                  tdest_const0,
                  tdest_const1);
}

#endif