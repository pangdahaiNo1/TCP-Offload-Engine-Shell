#include "echo_server.hpp"
#include "toe/mock/mock_toe.hpp"
// DONOT change the header files sequence
#include "toe/mock/mock_logger.hpp"
#include "utils/axi_utils_test.hpp"

using namespace hls;
MockLogger logger("./echo_server_inner.log", NET_APP);

void EmptyEchoFifos(MockLogger &                    logger,
                    stream<NetAXISListenPortReq> &  net_app_listen_port_req,
                    stream<NetAXISAppReadReq> &     net_app_read_data_req,
                    stream<NetAXISAppOpenConnReq> & net_app_open_conn_req,
                    stream<NetAXISAppCloseConnReq> &net_app_close_conn_req,
                    stream<NetAXISAppTransDataReq> &net_app_trans_data_req) {
  while (!net_app_listen_port_req.empty()) {
    ListenPortReq req = net_app_listen_port_req.read();
    logger.Info(NET_APP, RX_APP_IF, "ListenPort Req", req.to_string());
  }
  while (!net_app_read_data_req.empty()) {
    AppReadReq req = net_app_read_data_req.read();
    logger.Info(NET_APP, RX_APP_IF, "ReadData Req", req.to_string());
  }
  while (!net_app_open_conn_req.empty()) {
    AppOpenConnReq req = net_app_open_conn_req.read();
    logger.Info(NET_APP, RX_APP_IF, "OpenConn Req", req.to_string());
  }
  while (!net_app_close_conn_req.empty()) {
    AppCloseConnReq req = net_app_close_conn_req.read();
    logger.Info(NET_APP, RX_APP_IF, "CloseOpen Req", req.to_string());
  }
  while (!net_app_trans_data_req.empty()) {
    AppTransDataReq req = net_app_trans_data_req.read();
    logger.Info(NET_APP, RX_APP_IF, "TransData Req", req.to_string());
  }
}

void TestEchoServer() {
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

  stream<NetAXIS> rand_data;
  stream<NetAXIS> rand_data_copy;

  MockLogger top_logger("./echo_server_top.log", NET_APP);

  ListenPortRsp   listen_port_rsp;
  AppNotification app_notify;
  AppReadRsp      read_rsp;
  AppTransDataRsp trans_rsp;
  NetAXISWord     cur_word;
  ap_uint<16>     payload_len = 0xFFF;
  int             sim_cycle   = 0;
  while (sim_cycle < 200) {
    switch (sim_cycle) {
      case 1:
        listen_port_rsp.data.port_number       = 15000;
        listen_port_rsp.data.open_successfully = true;
        listen_port_rsp.dest                   = kTDEST;
        net_app_listen_port_rsp.write(listen_port_rsp.to_net_axis());
        break;
      case 2:
        app_notify.data = AppNotificationNoTDEST(0x1, payload_len, mock_src_ip_addr, 15000, false);
        app_notify.dest = kTDEST;
        net_app_notification.write(app_notify.to_net_axis());
        break;
      case 3:
        read_rsp.data = 0x1;
        read_rsp.dest = kTDEST;
        net_app_read_data_rsp.write(read_rsp.to_net_axis());
        break;
      case 4:
        GenRandStream(payload_len, rand_data);
        do {
          cur_word = rand_data.read();
          rand_data_copy.write(cur_word.to_net_axis());
          net_app_rx_data_in.write(cur_word.to_net_axis());
        } while (cur_word.last != 1);
        break;
      case 5:
        trans_rsp.data.error           = NO_ERROR;
        trans_rsp.data.length          = payload_len;
        trans_rsp.data.remaining_space = 0xFFFF;
        trans_rsp.dest                 = kTDEST;
        net_app_trans_data_rsp.write(trans_rsp.to_axis());
        break;

      default:
        break;
    }
    echo_server(net_app_listen_port_req,
                net_app_listen_port_rsp,
                net_app_new_client_notification,
                net_app_notification,
                net_app_read_data_req,
                net_app_read_data_rsp,
                net_app_open_conn_req,
                net_app_open_conn_rsp,
                net_app_close_conn_req,
                net_app_rx_data_in,
                net_app_trans_data_req,
                net_app_trans_data_rsp,
                net_app_tx_data_out);
    EmptyEchoFifos(top_logger,
                   net_app_listen_port_req,
                   net_app_read_data_req,
                   net_app_open_conn_req,
                   net_app_close_conn_req,
                   net_app_trans_data_req);
    sim_cycle++;
    logger.SetSimCycle(sim_cycle);
    top_logger.SetSimCycle(sim_cycle);
  }
  ComparePacpPacketsWithGolden(rand_data_copy, net_app_tx_data_out, false);
}

int main() { TestEchoServer(); }
