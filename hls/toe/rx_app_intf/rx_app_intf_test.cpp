
#include "rx_app_intf.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace hls;
using std::to_string;
MockLogger logger("./rx_app_intf_inner.log", RX_APP_INTF);

void EmptyPortHandlerFifos(MockLogger &                  logger,
                           stream<NetAXISListenPortRsp> &rx_app_to_net_app_listen_port_rsp,
                           stream<ListenPortReq> &       rx_app_to_ptable_listen_port_req) {
  NetAXISListenPortRsp to_net_app_rsp;
  ListenPortRsp        to_net_app_rsp_inner;
  ListenPortReq        to_ptable_req;
  logger.DisableSendLog();
  while (!rx_app_to_net_app_listen_port_rsp.empty()) {
    rx_app_to_net_app_listen_port_rsp.read(to_net_app_rsp);
    to_net_app_rsp_inner = to_net_app_rsp;
    logger.Info(RX_APP_INTF, NET_APP, "ListenPort Rsp", to_net_app_rsp_inner.to_string());
  }
  while (!rx_app_to_ptable_listen_port_req.empty()) {
    rx_app_to_ptable_listen_port_req.read(to_ptable_req);
    logger.Info(RX_APP_INTF, PORT_TABLE, "ListenPort Req", to_ptable_req.to_string());
  }
}

int TestPortHandler() {
  stream<NetAXISListenPortReq> net_app_to_rx_app_listen_port_req;
  stream<NetAXISListenPortRsp> rx_app_to_net_app_listen_port_rsp;
  stream<ListenPortReq>        rx_app_to_ptable_listen_port_req;
  stream<ListenPortRsp>        ptable_to_rx_app_listen_port_rsp;

  EventWithTuple ev;

  MockLogger port_logger("rx_app_port.log", RX_APP_INTF);

  NetAXISListenPortReq net_app_req;
  ListenPortRsp        ptable_rsp;
  int                  sim_cycle = 0;
  logger.Info("Test Port");
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        net_app_req.data = 0x80;
        net_app_req.dest = 0x1;
        net_app_to_rx_app_listen_port_req.write(net_app_req);
        break;
      case 2:
        net_app_req.data = 0x81;
        net_app_req.dest = 0x2;
        net_app_to_rx_app_listen_port_req.write(net_app_req);
        break;
      case 3:
        ptable_rsp.data = ListenPortRspNoTDEST(true, false, false, 0x80);
        ptable_rsp.dest = 0x1;
        ptable_to_rx_app_listen_port_rsp.write(ptable_rsp);
        break;
      case 4:
        ptable_rsp.data = ListenPortRspNoTDEST(true, false, false, 0x81);
        ptable_rsp.dest = 0x2;
        ptable_to_rx_app_listen_port_rsp.write(ptable_rsp);
        break;
      default:
        break;
    }
    RxAppPortHandler(net_app_to_rx_app_listen_port_req,
                     rx_app_to_net_app_listen_port_rsp,
                     rx_app_to_ptable_listen_port_req,
                     ptable_to_rx_app_listen_port_rsp);
    EmptyPortHandlerFifos(
        port_logger, rx_app_to_net_app_listen_port_rsp, rx_app_to_ptable_listen_port_req);
    sim_cycle++;
    port_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}

void EmptyDataHandlerFifos(MockLogger &               logger,
                           stream<NetAXISAppReadRsp> &net_app_read_data_rsp,
                           stream<RxSarAppReqRsp> &   rx_app_to_rx_sar_req,
                           stream<NetAXIS> &          rx_app_to_net_app_data) {
  NetAXISAppReadRsp to_net_app_read_rsp;
  AppReadRsp        to_net_app_read_rsp_inner;
  RxSarAppReqRsp    to_rx_sar_req;
  NetAXISWord       to_net_app_data;

  while (!rx_app_to_rx_sar_req.empty()) {
    rx_app_to_rx_sar_req.read(to_rx_sar_req);
    logger.Info("Rx App to rx sar req", to_rx_sar_req.to_string(), false);
  }
  while (!net_app_read_data_rsp.empty()) {
    net_app_read_data_rsp.read(to_net_app_read_rsp);
    to_net_app_read_rsp_inner = to_net_app_read_rsp;
    logger.Info("Rx App to Net App read data rsp", to_net_app_read_rsp_inner.to_string(), false);
  }
  while (!rx_app_to_net_app_data.empty()) {
    to_net_app_data = rx_app_to_net_app_data.read();
    logger.Info("Rx App to Net APP data", to_net_app_data.to_string(), false);
  }
}

int TestDataHandler(stream<NetAXIS> &input_tcp_packets) {
  stream<NetAXISAppReadReq> net_app_read_data_req;
  stream<NetAXISAppReadRsp> net_app_read_data_rsp;
  stream<RxSarAppReqRsp>    rx_app_to_rx_sar_req;
  stream<RxSarAppReqRsp>    rx_sar_to_rx_app_rsp;
  // rx engine data to net app
  stream<NetAXISWord> rx_eng_to_rx_app_data("rx_eng_to_rx_app_data");
  stream<NetAXIS>     rx_app_to_net_app_data("rx_app_to_net_app_data");

  MockLogger        data_logger("rx_app_data.log", RX_APP_INTF);
  NetAXISAppReadReq net_app_req;
  RxSarAppReqRsp    rx_sar_rsp;

  NetAXIS          cur_word{};
  TcpSessionBuffer cur_word_length = 0;
  logger.Info("Test Data");

  int sim_cycle = 0;
  while (sim_cycle < 500) {
    switch (sim_cycle % 20) {
      case 1:
        net_app_req.data = AppReadReqNoTEST(0x1, 0x100);
        net_app_req.dest = 0x1;
        net_app_read_data_req.write(net_app_req);
        break;
      case 2:
        net_app_req.data = AppReadReqNoTEST(0x2, 0x100);
        net_app_req.dest = 0x2;
        net_app_read_data_req.write(net_app_req);
        net_app_req.data = AppReadReqNoTEST(0x4, 0x100);
        net_app_req.dest = 0x2;
        net_app_read_data_req.write(net_app_req);
        break;
      case 3:
        rx_sar_rsp.app_read   = 0x1234567;
        rx_sar_rsp.session_id = 0x1;
        rx_sar_to_rx_app_rsp.write(rx_sar_rsp);
        break;
      case 4:
        cur_word.last = 0;
        while (!input_tcp_packets.empty() && cur_word.last != 1) {
          input_tcp_packets.read(cur_word);
          rx_eng_to_rx_app_data.write(cur_word);
        }
        break;
      case 5:
        rx_sar_rsp.app_read   = 0x1230000;
        rx_sar_rsp.session_id = 0x2;
        rx_sar_to_rx_app_rsp.write(rx_sar_rsp);
        break;
      default:
        break;
    }
    RxAppDataHandler(net_app_read_data_req,
                     net_app_read_data_rsp,
                     rx_app_to_rx_sar_req,
                     rx_sar_to_rx_app_rsp,
                     rx_eng_to_rx_app_data,
                     rx_app_to_net_app_data);
    EmptyDataHandlerFifos(
        data_logger, net_app_read_data_rsp, rx_app_to_rx_sar_req, rx_app_to_net_app_data);
    sim_cycle++;
    data_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}

void TestRxAppIntf(stream<NetAXIS> &input_tcp_packets) {
  // net role - port handler
  stream<NetAXISListenPortReq> net_app_to_rx_app_listen_port_req;
  stream<NetAXISListenPortRsp> rx_app_to_net_app_listen_port_rsp;
  // rxapp - port table
  stream<ListenPortReq> rx_app_to_ptable_listen_port_req;
  stream<ListenPortRsp> ptable_to_rx_app_listen_port_rsp;
  // not role - data handler
  stream<NetAXISAppReadReq> net_app_read_data_req;
  stream<NetAXISAppReadRsp> net_app_read_data_rsp;
  // rx sar req/rsp
  stream<RxSarAppReqRsp> rx_app_to_rx_sar_req;
  stream<RxSarAppReqRsp> rx_sar_to_rx_app_rsp;
  // data from rx engine to net app
  stream<NetAXISWord> rx_eng_to_rx_app_data;
  stream<NetAXIS>     rx_app_to_net_app_data;
  // net role app - notification
  // Rx engine to Rx app
  stream<AppNotificationNoTDEST> rx_eng_to_rx_app_notification;
  // Timer to Rx app
  stream<AppNotificationNoTDEST> rtimer_to_rx_app_notification;
  // lookup for tdest, req/rsp connect to slookup controller
  stream<TcpSessionID> rx_app_to_slookup_tdest_lookup_req;
  stream<NetAXISDest>  slookup_to_rx_app_tdest_lookup_rsp;
  // appnotifacation to net app with TDEST
  stream<NetAXISAppNotification> net_app_notification;

  MockLogger top_logger("rx_app_intf.log", RX_APP_INTF);
  logger.Info("Test Rx App Intf");

  NetAXISListenPortReq   net_app_port_req;
  ListenPortRsp          ptable_rsp;
  NetAXISAppReadReq      net_app_data_req;
  RxSarAppReqRsp         rx_sar_rsp;
  AppNotificationNoTDEST rx_eng_notify;
  NetAXIS                cur_word{};

  int sim_cycle = 0;
  while (sim_cycle < 500) {
    switch (sim_cycle) {
      case 1:
        // net app listen
        net_app_port_req.data = 0x80;
        net_app_port_req.dest = 0x1;
        net_app_to_rx_app_listen_port_req.write(net_app_port_req);
        break;
      case 2:
        // ptable resp for port listening
        ptable_rsp.data = ListenPortRspNoTDEST(true, false, false, net_app_port_req.data);
        ptable_rsp.dest = 0x1;
        ptable_to_rx_app_listen_port_rsp.write(ptable_rsp);
        break;
      case 3:
        // rx engine notify the role, new data is coming
        rx_eng_notify = AppNotificationNoTDEST(0x1,     // session id
                                               0x1000,  // length
                                               mock_src_ip_addr,
                                               mock_dst_tcp_port);
        rx_eng_to_rx_app_notification.write(rx_eng_notify);
        // current session belongs to role 0x1
        slookup_to_rx_app_tdest_lookup_rsp.write(0x1);
      case 4:
        // net app read data req from role 0x1
        net_app_data_req.data = AppReadReqNoTEST(0x1, 0x100);
        net_app_data_req.dest = 0x1;
        net_app_read_data_req.write(net_app_data_req);
        break;
      case 5:
        // rx sar resp to rx app
        rx_sar_rsp.app_read   = 0x1234567;
        rx_sar_rsp.session_id = 0x1;
        rx_sar_to_rx_app_rsp.write(rx_sar_rsp);
      case 6:
      case 7:
      case 8:
      case 9:
      case 10:
      case 11:
        cur_word.last = 0;
        while (!input_tcp_packets.empty() && cur_word.last != 1) {
          input_tcp_packets.read(cur_word);
          rx_eng_to_rx_app_data.write(cur_word);
        }
        break;
      default:
        break;
    }
    rx_app_intf(net_app_to_rx_app_listen_port_req,
                rx_app_to_net_app_listen_port_rsp,
                rx_app_to_ptable_listen_port_req,
                ptable_to_rx_app_listen_port_rsp,
                net_app_read_data_req,
                net_app_read_data_rsp,
                rx_app_to_rx_sar_req,
                rx_sar_to_rx_app_rsp,
                rx_eng_to_rx_app_data,
                rx_app_to_net_app_data,
                rx_eng_to_rx_app_notification,
                rtimer_to_rx_app_notification,
                rx_app_to_slookup_tdest_lookup_req,
                slookup_to_rx_app_tdest_lookup_rsp,
                net_app_notification);
    EmptyPortHandlerFifos(
        top_logger, rx_app_to_net_app_listen_port_rsp, rx_app_to_ptable_listen_port_req);
    EmptyDataHandlerFifos(
        top_logger, net_app_read_data_rsp, rx_app_to_rx_sar_req, rx_app_to_net_app_data);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *          input_tcp_pcap_file = argv[1];
  stream<NetAXIS> input_tcp_packets("input_tcp_packets");
  stream<NetAXIS> input_tcp_packets2("input_tcp_packets2");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_packets);
  PcapToStream(input_tcp_pcap_file, true, input_tcp_packets2);
  TestPortHandler();
  TestDataHandler(input_tcp_packets);
  // TestRxAppIntf(input_tcp_packets2);

  return 0;
}
