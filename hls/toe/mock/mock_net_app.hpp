#ifndef _MOCK_NET_APP_HPP_
#define _MOCK_NET_APP_HPP_

#include "toe/toe_conn.hpp"

#include <string>

using std::string;
struct NetAppIntf {
  // listen port
  stream<NetAXISListenPortReq> net_app_listen_port_req;
  stream<NetAXISListenPortRsp> net_app_listen_port_rsp;
  // rx client notify
  stream<NetAXISNewClientNotification> net_app_new_client_notification;
  // rx app notify
  stream<NetAXISAppNotification> net_app_notification;
  // read data req/rsp
  stream<NetAXISAppReadReq> net_app_read_data_req;
  stream<NetAXISAppReadRsp> net_app_read_data_rsp;
  // open/close conn
  stream<NetAXISAppOpenConnReq>  net_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>  net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq> net_app_close_conn_req;
  // read data
  stream<NetAXIS> net_app_rx_data_in;
  // transmit data
  stream<NetAXISAppTransDataReq> net_app_trans_data_req;
  stream<NetAXISAppTransDataRsp> net_app_trans_data_rsp;
  stream<NetAXIS>                net_app_tx_data_out;
  NetAXISDest                    tdest_const;

  NetAppIntf(NetAXISDest tdest, string fifo_suffix) {
    tdest_const = tdest;
    // set fifo name
    net_app_listen_port_req.set_name(("net_app_listen_port_req" + fifo_suffix).c_str());
    net_app_listen_port_rsp.set_name(("net_app_listen_port_rsp" + fifo_suffix).c_str());
    net_app_new_client_notification.set_name(
        ("net_app_new_client_notification" + fifo_suffix).c_str());
    net_app_notification.set_name(("net_app_notification" + fifo_suffix).c_str());
    net_app_read_data_req.set_name(("net_app_read_data_req" + fifo_suffix).c_str());
    net_app_read_data_rsp.set_name(("net_app_read_data_rsp" + fifo_suffix).c_str());
    net_app_open_conn_req.set_name(("net_app_open_conn_req" + fifo_suffix).c_str());
    net_app_open_conn_rsp.set_name(("net_app_open_conn_rsp" + fifo_suffix).c_str());
    net_app_close_conn_req.set_name(("net_app_close_conn_req" + fifo_suffix).c_str());
    net_app_rx_data_in.set_name(("net_app_rx_data_in" + fifo_suffix).c_str());
    net_app_trans_data_req.set_name(("net_app_trans_data_req" + fifo_suffix).c_str());
    net_app_trans_data_rsp.set_name(("net_app_trans_data_rsp" + fifo_suffix).c_str());
    net_app_tx_data_out.set_name(("net_app_tx_data_out" + fifo_suffix).c_str());
  }
};

#endif