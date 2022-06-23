#include "mock/mock_cam.hpp"
#include "mock/mock_logger.hpp"
#include "mock/mock_memory.hpp"
#include "mock/mock_net_app.hpp"
#include "mock/mock_toe.hpp"
#include "net_app/echo_server/echo_server.hpp"
#include "toe/toe.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

void EmptyToeRxAppFifo(MockLogger &logger,
                       // rx app
                       stream<NetAXISListenPortRsp> &  rx_app_to_net_app_listen_port_rsp,
                       stream<NetAXISAppReadRsp> &     net_app_read_data_rsp,
                       stream<NetAXISAppNotification> &net_app_notification) {
  ListenPortRsp   listen_port_rsp;
  AppReadRsp      read_data_rsp;
  AppNotification app_notify;
  while (!(rx_app_to_net_app_listen_port_rsp.empty())) {
    listen_port_rsp = rx_app_to_net_app_listen_port_rsp.read();
    logger.Info(TOE_TOP, NET_APP, "ListenPort Rsp", listen_port_rsp.to_string());
  }

  while (!(net_app_read_data_rsp.empty())) {
    read_data_rsp = net_app_read_data_rsp.read();
    logger.Info(TOE_TOP, NET_APP, "ReadData Rsp", read_data_rsp.to_string());
  }
  while (!(net_app_notification.empty())) {
    app_notify = net_app_notification.read();
    logger.Info(TOE_TOP, NET_APP, "Notify", app_notify.to_string());
  }
}

void EmptyToeTxAppFifo(MockLogger &logger,
                       // tx app
                       stream<NetAXISAppOpenConnRsp> &       tx_app_to_net_app_open_conn_rsp,
                       stream<NetAXISNewClientNotification> &net_app_new_client_notification,
                       stream<NetAXISAppTransDataRsp> &      tx_app_to_net_app_trans_data_rsp) {
  AppOpenConnRsp        open_conn_rsp;
  NewClientNotification new_client_notify;
  AppTransDataRsp       trans_data_rsp;
  while (!(tx_app_to_net_app_open_conn_rsp.empty())) {
    open_conn_rsp = tx_app_to_net_app_open_conn_rsp.read();
    logger.Info(TOE_TOP, NET_APP, "OpenConn Rsp", open_conn_rsp.to_string());
  }

  while (!(net_app_new_client_notification.empty())) {
    new_client_notify = net_app_new_client_notification.read();
    logger.Info(TOE_TOP, NET_APP, "NewClient Rsp", new_client_notify.to_string());
  }
  while (!(tx_app_to_net_app_trans_data_rsp.empty())) {
    trans_data_rsp = tx_app_to_net_app_trans_data_rsp.read();
    logger.Info(TOE_TOP, NET_APP, "TransData Rsp", trans_data_rsp.to_string());
  }
}
MockLogger logger("toe_inner.log", TOE_TOP);

/**
 * @brief Test the SYN-ACK retransmission
 *
 * @details When the client sends a SYN packet, the server returns SYN-ACK, but the client does not
 * respond to the server; the server notifies the app of the failure after retransmitting it four
 * times.
 * After that, the server side does not respond when it receives another ACK packet from the
 * client
 */
void TestToeSYNRetrans(stream<NetAXIS> &input_tcp_packet) {
  // output stream
  stream<NetAXIS> output_tcp_packet;
  // in big endian
  IpAddr my_ip_addr = 0x24131e0a;  // 10.19.0.36
  // TOE INTF
  ToeIntf toe_intf(my_ip_addr, "_SYN_Retrans");
  // mock cam
  MockCam mock_cam;
  // mock mem
  MockMem    tx_mock_mem;
  MockMem    rx_mock_mem;
  MockLogger top_logger("toe_top_retrans_SYN_failed.log", TOE_TOP);

  // test variblaes
  ListenPortReq listen_req;
  NetAXIS       cur_word;

  int sim_cycle = 0;
  while (sim_cycle < 20000) {
    switch (sim_cycle) {
      case 1:
        listen_req.data = 0x302d;
        listen_req.dest = 0x1;
        toe_intf.net_app_to_rx_app_listen_port_req.write(listen_req.to_net_axis());
        break;
      case 2:
        listen_req.data = 0x81;
        listen_req.dest = 0x1;
        toe_intf.net_app_to_rx_app_listen_port_req.write(listen_req.to_net_axis());
        break;
      case 3:
        do {
          cur_word = input_tcp_packet.read();
          toe_intf.rx_ip_pkt_in.write(cur_word);
          output_tcp_packet.write(cur_word);
        } while (cur_word.last != 1);
        break;
      case 9000:
        do {
          cur_word = input_tcp_packet.read();
          toe_intf.rx_ip_pkt_in.write(cur_word);
          output_tcp_packet.write(cur_word);
        } while (cur_word.last != 1);
        break;

      default:
        break;
    }
    ConnectToeIntfWithToe(toe_intf);
    ConnectToeIntfWithMockCam(top_logger, toe_intf, mock_cam);
#if !(TCP_RX_DDR_BYPASS)
    ConnectToeRxIntfWithMockMem(top_logger, toe_intf, rx_mock_mem);
#endif
    ConnectToeTxIntfWithMockMem(top_logger, toe_intf, tx_mock_mem);
    EmptyToeRxAppFifo(top_logger,
                      toe_intf.rx_app_to_net_app_listen_port_rsp,
                      toe_intf.rx_app_to_net_app_recv_data_rsp,
                      toe_intf.net_app_notification);
    EmptyToeTxAppFifo(top_logger,
                      toe_intf.tx_app_to_net_app_open_conn_rsp,
                      toe_intf.net_app_new_client_notification,
                      toe_intf.tx_app_to_net_app_trans_data_rsp);
    while (!toe_intf.tx_ip_pkt_out.empty()) {
      output_tcp_packet.write(toe_intf.tx_ip_pkt_out.read());
    }

    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
  StreamToPcap("toe.pcap", true, true, output_tcp_packet, true);
}

/**
 * @brief Test the SYN-ACK retransmission
 *
 * @details When the client sends a SYN packet, the server returns SYN-ACK, the client respond to
 * server before reaching the MAX RT count
 */
void TestToeSYNRetransThenACK(stream<NetAXIS> &input_tcp_packet) {
  // output stream
  stream<NetAXIS> output_tcp_packet;
  // in big endian
  IpAddr my_ip_addr = 0x24131e0a;  // 10.19.0.36

  ToeIntf toe_intf(my_ip_addr, "_SYN_Retrans_then_ack");
  // mock cam
  MockCam mock_cam;
  // mock mem
  MockMem tx_mock_mem;
  MockMem rx_mock_mem;

  MockLogger top_logger("toe_top_retrans_SYN.log", TOE_TOP);
  logger.SetSimCycle(0);
  logger.NewLine("Test Retrans SYN-ACK");
  // test variblaes
  ListenPortReq listen_req;
  NetAXIS       cur_word;

  int sim_cycle = 0;
  while (sim_cycle < 10000) {
    switch (sim_cycle) {
      case 1:
        listen_req.data = 0x302d;
        listen_req.dest = 0x1;
        toe_intf.net_app_to_rx_app_listen_port_req.write(listen_req.to_net_axis());
        break;
      case 2:
        listen_req.data = 0x81;
        listen_req.dest = 0x1;
        toe_intf.net_app_to_rx_app_listen_port_req.write(listen_req.to_net_axis());
        break;
      case 3:
        do {
          cur_word = input_tcp_packet.read();
          toe_intf.rx_ip_pkt_in.write(cur_word);
          output_tcp_packet.write(cur_word);
        } while (cur_word.last != 1);
        break;
      case 5000:
        do {
          cur_word = input_tcp_packet.read();
          toe_intf.rx_ip_pkt_in.write(cur_word);
          output_tcp_packet.write(cur_word);
        } while (cur_word.last != 1);
        break;

      default:
        break;
    }
    ConnectToeIntfWithToe(toe_intf);
    ConnectToeIntfWithMockCam(top_logger, toe_intf, mock_cam);
#if !(TCP_RX_DDR_BYPASS)
    ConnectToeRxIntfWithMockMem(top_logger, toe_intf, rx_mock_mem);
#endif
    ConnectToeTxIntfWithMockMem(top_logger, toe_intf, tx_mock_mem);
    EmptyToeRxAppFifo(top_logger,
                      toe_intf.rx_app_to_net_app_listen_port_rsp,
                      toe_intf.rx_app_to_net_app_recv_data_rsp,
                      toe_intf.net_app_notification);
    EmptyToeTxAppFifo(top_logger,
                      toe_intf.tx_app_to_net_app_open_conn_rsp,
                      toe_intf.net_app_new_client_notification,
                      toe_intf.tx_app_to_net_app_trans_data_rsp);
    while (!toe_intf.tx_ip_pkt_out.empty()) {
      output_tcp_packet.write(toe_intf.tx_ip_pkt_out.read());
    }

    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
  StreamToPcap("toe.pcap", true, true, output_tcp_packet, true);
}

/**
 * @brief Test
 */
void TestToeWithEcho(stream<NetAXIS> &input_tcp_packet) {
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
    ConnectToeIntfWithToe(toe_intf);
    ConnectToeIntfWithMockCam(top_logger, toe_intf, mock_cam);
#if !(TCP_RX_DDR_BYPASS)
    ConnectToeRxIntfWithMockMem(top_logger, toe_intf, rx_mock_mem);
#endif
    ConnectToeTxIntfWithMockMem(top_logger, toe_intf, tx_mock_mem);

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
    ConnectToeIntfWithToe(toe);
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
    ConnectToeIntfWithMockCam(top_logger, toe, mock_cam);
#if !(TCP_RX_DDR_BYPASS)
    ConnectToeRxIntfWithMockMem(top_logger, toe, rx_mock_mem);
#endif
    ConnectToeTxIntfWithMockMem(top_logger, toe, tx_mock_mem);
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
  if (argc < 3) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>*" << endl;
    return -1;
  }
  char *          input_tcp_pcap_file        = argv[1];
  char *          input_syn_and_ack_pkt_file = argv[2];
  stream<NetAXIS> input_tcp_ip_pkt("input_tcp_ip_pkt");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_ip_pkt);

  // contains SYN pkt + ACK pkt, no SYN-ACK
  stream<NetAXIS> input_syn_and_ack_pkt("syn_and_ack_pkt");
  stream<NetAXIS> input_syn_and_ack_pkt2("syn_and_ack_pkt2");
  PcapToStream(input_syn_and_ack_pkt_file, true, input_syn_and_ack_pkt);
  PcapToStream(input_syn_and_ack_pkt_file, true, input_syn_and_ack_pkt2);

  // TestToeSYNRetrans(input_syn_and_ack_pkt);
  // TestToeSYNRetransThenACK(input_syn_and_ack_pkt2);
  TestToeWithEcho(input_tcp_ip_pkt);
  // TestToeWithTwoEcho(input_syn_and_ack_pkt2);
  return 0;
}