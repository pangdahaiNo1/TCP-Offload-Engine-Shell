#include "iperf2.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_memory.hpp"
#include "toe/mock/mock_net_app.hpp"
// DONOT change the header files sequence
#include "toe/mock/mock_cam.hpp"
#include "toe/mock/mock_toe.hpp"
#include "toe/toe.hpp"
#include "utils/axi_utils_test.hpp"

using namespace hls;
MockLogger logger("./iperf2_inner.log", NET_APP);

// unit test
void EmptyIperf2Fifos(MockLogger                     &logger,
                      stream<NetAXISListenPortReq>   &net_app_listen_port_req,
                      stream<NetAXISAppReadReq>      &net_app_read_data_req,
                      stream<NetAXISAppOpenConnReq>  &net_app_open_conn_req,
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

// unit test
void TestIperf2() {
  // iperf config
  IperfRegs       iperf_reg;
  stream<NetAXIS> rand_data;
  stream<NetAXIS> rand_data_copy;
  NetAppIntf      app(0x2, "_unit_test");

  MockLogger top_logger("./iperf2_top.log", NET_APP);

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
        listen_port_rsp.data.port_number       = 5011;
        listen_port_rsp.data.open_successfully = true;
        listen_port_rsp.dest                   = app.tdest_const;
        app.net_app_listen_port_rsp.write(listen_port_rsp.to_net_axis());
        break;
      case 2:
        app_notify.data = AppNotificationNoTDEST(0x1, payload_len, mock_src_ip_addr, 5011, false);
        app_notify.dest = app.tdest_const;
        app.net_app_notification.write(app_notify.to_net_axis());
        break;
      case 3:
        read_rsp.data = 0x1;
        read_rsp.dest = app.tdest_const;
        app.net_app_recv_data_rsp.write(read_rsp.to_net_axis());
        break;
      case 4:
        // GenRandStream(payload_len, rand_data);
        // do {
        //   cur_word = rand_data.read();
        //   rand_data_copy.write(cur_word.to_net_axis());
        //   app.net_app_recv_data.write(cur_word.to_net_axis());
        // } while (cur_word.last != 1);
        break;
      case 5:
        trans_rsp.data.error           = NO_ERROR;
        trans_rsp.data.length          = payload_len;
        trans_rsp.data.remaining_space = 0xFFFF;
        trans_rsp.dest                 = app.tdest_const;
        app.net_app_trans_data_rsp.write(trans_rsp.to_axis());
        break;

      default:
        break;
    }
    iperf2(iperf_reg,
           app.net_app_listen_port_req,
           app.net_app_listen_port_rsp,
           app.net_app_new_client_notification,
           app.net_app_notification,
           app.net_app_recv_data_req,
           app.net_app_recv_data_rsp,
           app.net_app_recv_data,
           app.net_app_open_conn_req,
           app.net_app_open_conn_rsp,
           app.net_app_close_conn_req,
           app.net_app_trans_data_req,
           app.net_app_trans_data_rsp,
           app.net_app_trans_data,
           app.tdest_const);
    EmptyIperf2Fifos(top_logger,
                     app.net_app_listen_port_req,
                     app.net_app_recv_data_req,
                     app.net_app_open_conn_req,
                     app.net_app_close_conn_req,
                     app.net_app_trans_data_req);
    sim_cycle++;
    logger.SetSimCycle(sim_cycle);
    top_logger.SetSimCycle(sim_cycle);
  }
}

/**
 * @brief Integration Test
 */
void TestIperf2ServerWithToe(stream<NetAXIS> &client_golden_pkt,
                             stream<NetAXIS> &server_golden_pkt) {
  // output stream
  stream<NetAXIS> output_tcp_packet;

  // iperf2 server ip in big endian
  IperfRegs iperf_reg;
  IpAddr    my_ip_addr = 0x29131e0a;  // 10.19.0.41
  ToeIntf   toe_intf(my_ip_addr, "_iperf_test");
  // mock cam
  MockCam mock_cam;
  // mock mem
  MockMem    tx_mock_mem;
  MockMem    rx_mock_mem;
  MockLogger top_logger("toe_top_with_echo.log", TOE_TOP);
  // current tdest
  NetAXISDest mock_tdest = 0x2;

  logger.SetSimCycle(0);
  logger.NewLine("TOE with Iperf");

  NetAXIS cur_word;
  int     sim_cycle = 0;
  while (sim_cycle < 20000) {
    if ((sim_cycle % 100) == 0 && !client_golden_pkt.empty()) {
      do {
        cur_word = client_golden_pkt.read();

        toe_intf.rx_ip_pkt_in.write(cur_word);
        output_tcp_packet.write(cur_word);

      } while (cur_word.last != 1);
    }
    toe_intf.ConnectToeIntfWithToe();
    toe_intf.ConnectToeIntfWithMockCam(top_logger, mock_cam);
#if !(TCP_RX_DDR_BYPASS)
    toe_intf.ConnectToeRxIntfWithMockMem(top_logger, rx_mock_mem);
#endif
    toe_intf.ConnectToeTxIntfWithMockMem(top_logger, tx_mock_mem);

    iperf2(iperf_reg,
           toe_intf.net_app_to_rx_app_listen_port_req,
           toe_intf.rx_app_to_net_app_listen_port_rsp,
           toe_intf.net_app_new_client_notification,
           toe_intf.net_app_notification,
           toe_intf.net_app_to_rx_app_recv_data_req,
           toe_intf.rx_app_to_net_app_recv_data_rsp,
           toe_intf.net_app_recv_data,
           toe_intf.net_app_to_tx_app_open_conn_req,
           toe_intf.tx_app_to_net_app_open_conn_rsp,
           toe_intf.net_app_to_tx_app_close_conn_req,
           toe_intf.net_app_to_tx_app_trans_data_req,
           toe_intf.tx_app_to_net_app_trans_data_rsp,
           toe_intf.net_app_trans_data,
           mock_tdest);
    while (!toe_intf.tx_ip_pkt_out.empty()) {
      output_tcp_packet.write(toe_intf.tx_ip_pkt_out.read());
    }

    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  StreamToPcap("iperf_server_integration.pcap", true, true, output_tcp_packet, true);
}

int main(int argc, char **argv) {
  if (argc < 3) {
    cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE> <GOLDEN_PCAP_FILE> "
         << endl;
    return -1;
  }
  char           *client_golden_file = argv[1];
  char           *server_golden_file = argv[2];
  stream<NetAXIS> client_golden_tcp_pkt("client_golden_tcp_pkt");
  stream<NetAXIS> server_golden_tcp_pkt("server_golden_tcp_pkt");
  PcapToStream(client_golden_file, true, client_golden_tcp_pkt);
  PcapToStream(server_golden_file, true, server_golden_tcp_pkt);
  // TestIperf2();
  TestIperf2ServerWithToe(client_golden_tcp_pkt, server_golden_tcp_pkt);
}