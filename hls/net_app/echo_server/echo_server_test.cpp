#include "echo_server.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_memory.hpp"
#include "toe/mock/mock_net_app.hpp"
// DONOT change the header files sequence
#include "toe/mock/mock_cam.hpp"
#include "toe/mock/mock_toe.hpp"
#include "toe/toe.hpp"
#include "utils/axi_utils_test.hpp"

using namespace hls;
MockLogger logger("./echo_server_inner.log", NET_APP);

void EmptyEchoFifos(MockLogger                     &logger,
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

// Unit test
void TestEchoServer() {
  stream<NetAXIS> rand_data;
  stream<NetAXIS> rand_data_copy;
  NetAppIntf      app(0x1, "_unit_test");

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
        listen_port_rsp.dest                   = app.tdest_const;
        app.net_app_listen_port_rsp.write(listen_port_rsp.to_net_axis());
        break;
      case 2:
        app_notify.data = AppNotificationNoTDEST(0x1, payload_len, mock_src_ip_addr, 15000, false);
        app_notify.dest = app.tdest_const;
        app.net_app_notification.write(app_notify.to_net_axis());
        break;
      case 3:
        read_rsp.data = 0x1;
        read_rsp.dest = app.tdest_const;
        app.net_app_recv_data_rsp.write(read_rsp.to_net_axis());
        break;
      case 4:
        GenRandStream(payload_len, rand_data);
        do {
          cur_word = rand_data.read();
          rand_data_copy.write(cur_word.to_net_axis());
          app.net_app_recv_data.write(cur_word.to_net_axis());
        } while (cur_word.last != 1);
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
    echo_server(app.net_app_listen_port_req,
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
    EmptyEchoFifos(top_logger,
                   app.net_app_listen_port_req,
                   app.net_app_recv_data_req,
                   app.net_app_open_conn_req,
                   app.net_app_close_conn_req,
                   app.net_app_trans_data_req);
    sim_cycle++;
    logger.SetSimCycle(sim_cycle);
    top_logger.SetSimCycle(sim_cycle);
  }
  ComparePacpPacketsWithGolden(rand_data_copy, app.net_app_trans_data, false);
}

/**
 * @brief Integration Test
 */
void TestEchoServerWithToe(stream<NetAXIS> &input_tcp_packet) {
  // output stream
  stream<NetAXIS> output_tcp_packet;

  // in big endian
  IpAddr  my_ip_addr = 0x24131e0a;  // 10.19.0.36
  ToeIntf toe_intf(my_ip_addr, "_with_echo");
  // mock cam
  MockCam mock_cam;
  // mock mem
  MockMem    tx_mock_mem;
  MockMem    rx_mock_mem;
  MockLogger top_logger("toe_top_with_echo.log", TOE_TOP);
  logger.SetSimCycle(0);
  logger.NewLine("Test with echo-server");
  // current tdest
  NetAXISDest mock_tdest = 0x2;
  // test variblaes
  ListenPortReq listen_req;
  NetAXIS       cur_word;

  int sim_cycle = 0;
  while (sim_cycle < 20000) {
    switch (sim_cycle) {
      case 1:
        listen_req.data = 0x302d;
        listen_req.dest = mock_tdest;
        toe_intf.net_app_to_rx_app_listen_port_req.write(listen_req.to_net_axis());
        break;
      // client send a SYN packet
      case 3:
      // client send a ACK packet for ESATABLISED a connection(3-way handshake done)
      case 3000:
      // client send a DATA packet, length=692Bytes = 0x2b4
      case 5000:
      // client send a ACK for receiving all data, seq.no = 692, ack.no = 693
      case 5500:
      //  client send a FIN
      case 6000:
      // client send a ACK, for CLOSE this connection
      case 6500:
        do {
          cur_word = input_tcp_packet.read();
          toe_intf.rx_ip_pkt_in.write(cur_word);
          output_tcp_packet.write(cur_word);
        } while (cur_word.last != 1);
        break;

      default:
        break;
    }
    toe_intf.ConnectToeIntfWithToe();
    toe_intf.ConnectToeIntfWithMockCam(top_logger, mock_cam);
#if !(TCP_RX_DDR_BYPASS)
    toe_intf.ConnectToeRxIntfWithMockMem(top_logger, rx_mock_mem);
#endif
    toe_intf.ConnectToeTxIntfWithMockMem(top_logger, tx_mock_mem);

    echo_server(toe_intf.net_app_to_rx_app_listen_port_req,
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
  StreamToPcap("toe.pcap", true, true, output_tcp_packet, true);
}

void TestToeWithTwoEcho(stream<NetAXIS> &input_tcp_packet) {
  // output stream
  stream<NetAXIS> output_tcp_packet;

  // toe intf
  IpAddr  my_ip_addr = 0x24131e0a;  // 10.19.0.36
  ToeIntf toe(my_ip_addr, "_with_two_echo");
  // echo0 intf
  NetAppIntf app0(0x1, "_echo1");
  // echo 1 intf
  NetAppIntf app1(0x2, "_echo2");

  MockCam    mock_cam;
  MockMem    tx_mock_mem;
  MockMem    rx_mock_mem;
  MockLogger top_logger("toe_top_with_two_echo.log", TOE_TOP);
  logger.SetSimCycle(0);
  logger.NewLine("Test with two echo-servers");

  // test variblaes
  ListenPortReq listen_req;
  NetAXIS       cur_word;

  int sim_cycle = 0;
  while (sim_cycle < 20000) {
    switch (sim_cycle) {
      case 1:
        // listen_req.data = 0x302d;
        // listen_req.dest = app0.tdest_const;
        // app0.net_app_listen_port_req.write(listen_req.to_net_axis());
        break;
      // client send a SYN packet
      case 3:
      // client send a ACK packet for ESATABLISED a connection(3-way handshake done)
      case 3000:
      // client send a DATA packet, length=692Bytes = 0x2b4
      case 5000:
      // client send a ACK for receiving all data, seq.no = 692, ack.no = 693
      case 5500:
      //  client send a FIN
      case 6000:
      // client send a ACK, for CLOSE this connection
      case 6500:
        // do {
        //   cur_word = input_tcp_packet.read();
        //   toe.rx_ip_pkt_in.write(cur_word);
        //   output_tcp_packet.write(cur_word);
        // } while (cur_word.last != 1);
        break;

      default:
        break;
    }
    MergeStreamNetAppToToe(app0.net_app_listen_port_req,
                           app1.net_app_listen_port_req,
                           toe.net_app_to_rx_app_listen_port_req,
                           app0.net_app_recv_data_req,
                           app1.net_app_recv_data_req,
                           toe.net_app_to_rx_app_recv_data_req,
                           app0.net_app_open_conn_req,
                           app1.net_app_open_conn_req,
                           toe.net_app_to_tx_app_open_conn_req,
                           app0.net_app_close_conn_req,
                           app1.net_app_close_conn_req,
                           toe.net_app_to_tx_app_close_conn_req,
                           app0.net_app_trans_data_req,
                           app1.net_app_trans_data_req,
                           toe.net_app_to_tx_app_trans_data_req,
                           app0.net_app_trans_data,
                           app1.net_app_trans_data,
                           toe.net_app_trans_data);
    toe.ConnectToeIntfWithToe();
    SwitchStreamToeToNetApp(toe.rx_app_to_net_app_listen_port_rsp,
                            app0.net_app_listen_port_rsp,
                            app1.net_app_listen_port_rsp,
                            toe.net_app_new_client_notification,
                            app0.net_app_new_client_notification,
                            app1.net_app_new_client_notification,
                            toe.net_app_notification,
                            app0.net_app_notification,
                            app1.net_app_notification,
                            toe.tx_app_to_net_app_open_conn_rsp,
                            app0.net_app_open_conn_rsp,
                            app1.net_app_open_conn_rsp,
                            toe.net_app_recv_data,
                            app0.net_app_recv_data,
                            app1.net_app_recv_data,
                            toe.tx_app_to_net_app_trans_data_rsp,
                            app0.net_app_trans_data_rsp,
                            app1.net_app_trans_data_rsp,
                            app0.tdest_const,
                            app1.tdest_const);
    toe.ConnectToeIntfWithMockCam(top_logger, mock_cam);
#if !(TCP_RX_DDR_BYPASS)
    toe.ConnectToeRxIntfWithMockMem(top_logger, rx_mock_mem);
#endif
    toe.ConnectToeTxIntfWithMockMem(top_logger, tx_mock_mem);
    echo_server(app0.net_app_listen_port_req,
                app0.net_app_listen_port_rsp,
                app0.net_app_new_client_notification,
                app0.net_app_notification,
                app0.net_app_recv_data_req,
                app0.net_app_recv_data_rsp,
                app0.net_app_recv_data,
                app0.net_app_open_conn_req,
                app0.net_app_open_conn_rsp,
                app0.net_app_close_conn_req,
                app0.net_app_trans_data_req,
                app0.net_app_trans_data_rsp,
                app0.net_app_trans_data,
                app0.tdest_const);

    echo_server(app1.net_app_listen_port_req,
                app1.net_app_listen_port_rsp,
                app1.net_app_new_client_notification,
                app1.net_app_notification,
                app1.net_app_recv_data_req,
                app1.net_app_recv_data_rsp,
                app1.net_app_recv_data,
                app1.net_app_open_conn_req,
                app1.net_app_open_conn_rsp,
                app1.net_app_close_conn_req,
                app1.net_app_trans_data_req,
                app1.net_app_trans_data_rsp,
                app1.net_app_trans_data,
                app1.tdest_const);
    while (!toe.tx_ip_pkt_out.empty()) {
      output_tcp_packet.write(toe.tx_ip_pkt_out.read());
    }

    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
  StreamToPcap("toe.pcap", true, true, output_tcp_packet, true);
}

int main(int argc, char **argv) {
  char           *input_tcp_pcap_file = argv[1];
  stream<NetAXIS> input_tcp_ip_pkt("input_tcp_ip_pkt");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_ip_pkt);

  TestEchoServer();
  TestEchoServerWithToe(input_tcp_ip_pkt);
}
