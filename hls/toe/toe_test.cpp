#include "mock/mock_cam.hpp"
#include "mock/mock_logger.hpp"
#include "mock/mock_memory.hpp"
#include "mock/mock_toe.hpp"
#include "net_app/echo_server/echo_server.hpp"
#include "toe/toe.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

struct ToeIntf {
  // rx engine
  stream<NetAXIS> rx_ip_pkt_in;
  // tx engine
  stream<DataMoverCmd> mover_read_mem_cmd_out;
  stream<NetAXIS>      mover_read_mem_data_in;
  stream<NetAXIS>      tx_app_to_tx_eng_data;
  stream<NetAXIS>      tx_ip_pkt_out;
  // rx app
  stream<NetAXISListenPortReq>   net_app_to_rx_app_listen_port_req;
  stream<NetAXISListenPortRsp>   rx_app_to_net_app_listen_port_rsp;
  stream<NetAXISAppReadReq>      net_app_read_data_req;
  stream<NetAXISAppReadRsp>      net_app_read_data_rsp;
  stream<NetAXIS>                rx_app_to_net_app_data;
  stream<NetAXISAppNotification> net_app_notification;
  // tx app
  stream<NetAXISAppOpenConnReq>        net_app_to_tx_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>        tx_app_to_net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq>       net_app_to_tx_app_close_conn_req;
  stream<NetAXISNewClientNotification> net_app_new_client_notification;
  stream<NetAXISAppTransDataReq>       net_app_to_tx_app_trans_data_req;
  stream<NetAXISAppTransDataRsp>       tx_app_to_net_app_trans_data_rsp;
  stream<NetAXIS>                      net_app_trans_data;
  stream<DataMoverCmd>                 tx_app_to_mem_write_cmd;
  stream<NetAXIS>                      tx_app_to_mem_write_data;
  stream<DataMoverStatus>              mem_to_tx_app_write_status;
  // CAM
  stream<RtlSLookupToCamLupReq> rtl_slookup_to_cam_lookup_req;
  stream<RtlCamToSlookupLupRsp> rtl_cam_to_slookup_lookup_rsp;
  stream<RtlSlookupToCamUpdReq> rtl_slookup_to_cam_update_req;
  stream<RtlCamToSlookupUpdRsp> rtl_cam_to_slookup_update_rsp;

  // registers
  ap_uint<16> reg_session_cnt;
  // in big endian
  IpAddr my_ip_addr;
  ToeIntf(IpAddr ip_addr, string fifo_suffix) {
    my_ip_addr = ip_addr;
    // set fifo name
    rx_ip_pkt_in.set_name(("rx_ip_pkt_in" + fifo_suffix).c_str());
    mover_read_mem_cmd_out.set_name(("mover_read_mem_cmd_out" + fifo_suffix).c_str());
    mover_read_mem_data_in.set_name(("mover_read_mem_data_in" + fifo_suffix).c_str());
    tx_app_to_tx_eng_data.set_name(("tx_app_to_tx_eng_data" + fifo_suffix).c_str());
    tx_ip_pkt_out.set_name(("tx_ip_pkt_out" + fifo_suffix).c_str());
    net_app_to_rx_app_listen_port_req.set_name(
        ("net_app_to_rx_app_listen_port_req" + fifo_suffix).c_str());
    rx_app_to_net_app_listen_port_rsp.set_name(
        ("rx_app_to_net_app_listen_port_rsp" + fifo_suffix).c_str());
    net_app_read_data_req.set_name(("net_app_read_data_req" + fifo_suffix).c_str());
    net_app_read_data_rsp.set_name(("net_app_read_data_rsp" + fifo_suffix).c_str());
    rx_app_to_net_app_data.set_name(("rx_app_to_net_app_data" + fifo_suffix).c_str());
    net_app_notification.set_name(("net_app_notification" + fifo_suffix).c_str());
    net_app_to_tx_app_open_conn_req.set_name(
        ("net_app_to_tx_app_open_conn_req" + fifo_suffix).c_str());
    tx_app_to_net_app_open_conn_rsp.set_name(
        ("tx_app_to_net_app_open_conn_rsp" + fifo_suffix).c_str());
    net_app_to_tx_app_close_conn_req.set_name(
        ("net_app_to_tx_app_close_conn_req" + fifo_suffix).c_str());
    net_app_new_client_notification.set_name(
        ("net_app_new_client_notification" + fifo_suffix).c_str());
    net_app_to_tx_app_trans_data_req.set_name(
        ("net_app_to_tx_app_trans_data_req" + fifo_suffix).c_str());
    tx_app_to_net_app_trans_data_rsp.set_name(
        ("tx_app_to_net_app_trans_data_rsp" + fifo_suffix).c_str());
    net_app_trans_data.set_name(("net_app_trans_data" + fifo_suffix).c_str());
    tx_app_to_mem_write_cmd.set_name(("tx_app_to_mem_write_cmd" + fifo_suffix).c_str());
    tx_app_to_mem_write_data.set_name(("tx_app_to_mem_write_data" + fifo_suffix).c_str());
    mem_to_tx_app_write_status.set_name(("mem_to_tx_app_write_status" + fifo_suffix).c_str());
    rtl_slookup_to_cam_lookup_req.set_name(("rtl_slookup_to_cam_lookup_req" + fifo_suffix).c_str());
    rtl_cam_to_slookup_lookup_rsp.set_name(("rtl_cam_to_slookup_lookup_rsp" + fifo_suffix).c_str());
    rtl_slookup_to_cam_update_req.set_name(("rtl_slookup_to_cam_update_req" + fifo_suffix).c_str());
    rtl_cam_to_slookup_update_rsp.set_name(("rtl_cam_to_slookup_update_rsp" + fifo_suffix).c_str());
  }
};

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
  MockMem    mock_mem;
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
    toe_top(toe_intf.rx_ip_pkt_in,
            toe_intf.mover_read_mem_cmd_out,
            toe_intf.mover_read_mem_data_in,
            toe_intf.tx_ip_pkt_out,
            toe_intf.net_app_to_rx_app_listen_port_req,
            toe_intf.rx_app_to_net_app_listen_port_rsp,
            toe_intf.net_app_read_data_req,
            toe_intf.net_app_read_data_rsp,
            toe_intf.rx_app_to_net_app_data,
            toe_intf.net_app_notification,
            toe_intf.net_app_to_tx_app_open_conn_req,
            toe_intf.tx_app_to_net_app_open_conn_rsp,
            toe_intf.net_app_to_tx_app_close_conn_req,
            toe_intf.net_app_new_client_notification,
            toe_intf.net_app_to_tx_app_trans_data_req,
            toe_intf.tx_app_to_net_app_trans_data_rsp,
            toe_intf.net_app_trans_data,
            toe_intf.tx_app_to_mem_write_cmd,
            toe_intf.tx_app_to_mem_write_data,
            toe_intf.mem_to_tx_app_write_status,
            toe_intf.rtl_slookup_to_cam_lookup_req,
            toe_intf.rtl_cam_to_slookup_lookup_rsp,
            toe_intf.rtl_slookup_to_cam_update_req,
            toe_intf.rtl_cam_to_slookup_update_rsp,
            toe_intf.reg_session_cnt,
            toe_intf.my_ip_addr);
    mock_cam.MockCamIntf(top_logger,
                         toe_intf.rtl_slookup_to_cam_lookup_req,
                         toe_intf.rtl_cam_to_slookup_lookup_rsp,
                         toe_intf.rtl_slookup_to_cam_update_req,
                         toe_intf.rtl_cam_to_slookup_update_rsp);
    mock_mem.MockMemIntf(top_logger,
                         toe_intf.mover_read_mem_cmd_out,
                         toe_intf.mover_read_mem_data_in,
                         toe_intf.tx_app_to_mem_write_cmd,
                         toe_intf.tx_app_to_mem_write_data,
                         toe_intf.mem_to_tx_app_write_status);
    EmptyToeRxAppFifo(top_logger,
                      toe_intf.rx_app_to_net_app_listen_port_rsp,
                      toe_intf.net_app_read_data_rsp,
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
  MockMem    mock_mem;
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
    toe_top(toe_intf.rx_ip_pkt_in,
            toe_intf.mover_read_mem_cmd_out,
            toe_intf.mover_read_mem_data_in,
            toe_intf.tx_ip_pkt_out,
            toe_intf.net_app_to_rx_app_listen_port_req,
            toe_intf.rx_app_to_net_app_listen_port_rsp,
            toe_intf.net_app_read_data_req,
            toe_intf.net_app_read_data_rsp,
            toe_intf.rx_app_to_net_app_data,
            toe_intf.net_app_notification,
            toe_intf.net_app_to_tx_app_open_conn_req,
            toe_intf.tx_app_to_net_app_open_conn_rsp,
            toe_intf.net_app_to_tx_app_close_conn_req,
            toe_intf.net_app_new_client_notification,
            toe_intf.net_app_to_tx_app_trans_data_req,
            toe_intf.tx_app_to_net_app_trans_data_rsp,
            toe_intf.net_app_trans_data,
            toe_intf.tx_app_to_mem_write_cmd,
            toe_intf.tx_app_to_mem_write_data,
            toe_intf.mem_to_tx_app_write_status,
            toe_intf.rtl_slookup_to_cam_lookup_req,
            toe_intf.rtl_cam_to_slookup_lookup_rsp,
            toe_intf.rtl_slookup_to_cam_update_req,
            toe_intf.rtl_cam_to_slookup_update_rsp,
            toe_intf.reg_session_cnt,
            toe_intf.my_ip_addr);
    mock_cam.MockCamIntf(top_logger,
                         toe_intf.rtl_slookup_to_cam_lookup_req,
                         toe_intf.rtl_cam_to_slookup_lookup_rsp,
                         toe_intf.rtl_slookup_to_cam_update_req,
                         toe_intf.rtl_cam_to_slookup_update_rsp);
    mock_mem.MockMemIntf(top_logger,
                         toe_intf.mover_read_mem_cmd_out,
                         toe_intf.mover_read_mem_data_in,
                         toe_intf.tx_app_to_mem_write_cmd,
                         toe_intf.tx_app_to_mem_write_data,
                         toe_intf.mem_to_tx_app_write_status);
    EmptyToeRxAppFifo(top_logger,
                      toe_intf.rx_app_to_net_app_listen_port_rsp,
                      toe_intf.net_app_read_data_rsp,
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
  MockMem    mock_mem;
  MockLogger top_logger("toe_top_with_echo.log", TOE_TOP);
  logger.SetSimCycle(0);
  logger.NewLine("Test with echo-server");
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
    toe_top(toe_intf.rx_ip_pkt_in,
            toe_intf.mover_read_mem_cmd_out,
            toe_intf.mover_read_mem_data_in,
            toe_intf.tx_ip_pkt_out,
            toe_intf.net_app_to_rx_app_listen_port_req,
            toe_intf.rx_app_to_net_app_listen_port_rsp,
            toe_intf.net_app_read_data_req,
            toe_intf.net_app_read_data_rsp,
            toe_intf.rx_app_to_net_app_data,
            toe_intf.net_app_notification,
            toe_intf.net_app_to_tx_app_open_conn_req,
            toe_intf.tx_app_to_net_app_open_conn_rsp,
            toe_intf.net_app_to_tx_app_close_conn_req,
            toe_intf.net_app_new_client_notification,
            toe_intf.net_app_to_tx_app_trans_data_req,
            toe_intf.tx_app_to_net_app_trans_data_rsp,
            toe_intf.net_app_trans_data,
            toe_intf.tx_app_to_mem_write_cmd,
            toe_intf.tx_app_to_mem_write_data,
            toe_intf.mem_to_tx_app_write_status,
            toe_intf.rtl_slookup_to_cam_lookup_req,
            toe_intf.rtl_cam_to_slookup_lookup_rsp,
            toe_intf.rtl_slookup_to_cam_update_req,
            toe_intf.rtl_cam_to_slookup_update_rsp,
            toe_intf.reg_session_cnt,
            toe_intf.my_ip_addr);
    mock_cam.MockCamIntf(top_logger,
                         toe_intf.rtl_slookup_to_cam_lookup_req,
                         toe_intf.rtl_cam_to_slookup_lookup_rsp,
                         toe_intf.rtl_slookup_to_cam_update_req,
                         toe_intf.rtl_cam_to_slookup_update_rsp);
    mock_mem.MockMemIntf(top_logger,
                         toe_intf.mover_read_mem_cmd_out,
                         toe_intf.mover_read_mem_data_in,
                         toe_intf.tx_app_to_mem_write_cmd,
                         toe_intf.tx_app_to_mem_write_data,
                         toe_intf.mem_to_tx_app_write_status);
    echo_server(toe_intf.net_app_to_rx_app_listen_port_req,
                toe_intf.rx_app_to_net_app_listen_port_rsp,
                toe_intf.net_app_new_client_notification,
                toe_intf.net_app_notification,
                toe_intf.net_app_read_data_req,
                toe_intf.net_app_read_data_rsp,
                toe_intf.net_app_to_tx_app_open_conn_req,
                toe_intf.tx_app_to_net_app_open_conn_rsp,
                toe_intf.net_app_to_tx_app_close_conn_req,
                toe_intf.rx_app_to_net_app_data,
                toe_intf.net_app_to_tx_app_trans_data_req,
                toe_intf.tx_app_to_net_app_trans_data_rsp,
                toe_intf.net_app_trans_data);
    while (!toe_intf.tx_ip_pkt_out.empty()) {
      output_tcp_packet.write(toe_intf.tx_ip_pkt_out.read());
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
  return 0;
}