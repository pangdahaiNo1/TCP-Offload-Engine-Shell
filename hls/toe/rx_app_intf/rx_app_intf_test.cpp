
#include "rx_app_intf.hpp"
#include "toe/mock/mock_toe.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

#include <iostream>

using namespace hls;

void EmptyPortHandlerFifos(std::ofstream &               out_stream,
                           stream<NetAXISListenPortRsp> &rx_app_to_net_app_listen_port_rsp,
                           stream<NetAXISListenPortReq> &rx_app_to_ptable_listen_port_req,
                           int                           sim_cycle) {
  NetAXISListenPortRsp to_net_app_rsp;
  NetAXISListenPortReq to_ptable_req;

  while (!rx_app_to_net_app_listen_port_rsp.empty()) {
    rx_app_to_net_app_listen_port_rsp.read(to_net_app_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Rx App to Net App rsp\t";
    out_stream << to_net_app_rsp.to_string() << "\n";
  }
  while (!rx_app_to_ptable_listen_port_req.empty()) {
    rx_app_to_ptable_listen_port_req.read(to_ptable_req);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Rx App to Porttable req\t";
    out_stream << to_ptable_req.to_string() << "\n";
  }
}

int TestPortHandler() {
  stream<NetAXISListenPortReq> net_app_to_rx_app_listen_port_req;
  stream<NetAXISListenPortRsp> rx_app_to_net_app_listen_port_rsp;
  stream<NetAXISListenPortReq> rx_app_to_ptable_listen_port_req;
  stream<NetAXISListenPortRsp> ptable_to_rx_app_listen_port_rsp;

  EventWithTuple ev;

  // open output file
  std::ofstream outputFile;

  outputFile.open("./out_port_handler.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }
  NetAXISListenPortReq net_app_req;
  NetAXISListenPortRsp ptable_rsp;
  int                  sim_cycle = 0;
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
        ptable_rsp.data = ListenPortRsp(true, false, false, 0x80);
        ptable_rsp.dest = 0x1;
        ptable_to_rx_app_listen_port_rsp.write(ptable_rsp);
        break;
      case 4:
        ptable_rsp.data = ListenPortRsp(true, false, false, 0x81);
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
        outputFile, rx_app_to_net_app_listen_port_rsp, rx_app_to_ptable_listen_port_req, sim_cycle);
    sim_cycle++;
  }

  return 0;
}

void EmptyDataHandlerFifos(std::ofstream &            out_stream,
                           stream<NetAXISAppReadRsp> &net_app_read_data_rsp,
                           stream<RxSarAppReqRsp> &   rx_app_to_rx_sar_req,
                           stream<NetAXIS> &          rx_app_to_net_app_data,
                           int                        sim_cycle) {
  NetAXISAppReadRsp to_net_app_read_rsp;
  RxSarAppReqRsp    to_rx_sar_req;
  NetAXIS           to_net_app_data;

  while (!rx_app_to_rx_sar_req.empty()) {
    rx_app_to_rx_sar_req.read(to_rx_sar_req);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Rx App to rx sar req\t";
    out_stream << to_rx_sar_req.to_string() << "\n";
  }
  while (!net_app_read_data_rsp.empty()) {
    net_app_read_data_rsp.read(to_net_app_read_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Rx App to Net App read data rsp\t";
    out_stream << to_net_app_read_rsp.to_string() << "\n";
  }
  while (!rx_app_to_net_app_data.empty()) {
    rx_app_to_net_app_data.read(to_net_app_data);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Rx App send \t";
    out_stream << dec << KeepToLength(to_net_app_data.keep) << " Bytes word to Net APP"
               << to_net_app_data.dest << "\n";
  }
}

int TestDataHandler(stream<NetAXIS> &input_tcp_packets) {
  stream<NetAXISAppReadReq> net_app_read_data_req;
  stream<NetAXISAppReadRsp> net_app_read_data_rsp;
  stream<RxSarAppReqRsp>    rx_app_to_rx_sar_req;
  stream<RxSarAppReqRsp>    rx_sar_to_rx_app_rsp;
  // rx engine data to net app
  stream<NetAXIS> rx_eng_to_rx_app_data("rx_eng_to_rx_app_data");
  stream<NetAXIS> rx_app_to_net_app_data("rx_app_to_net_app_data");

  // open output file
  std::ofstream outputFile;

  outputFile.open("./out_data_handler.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }
  NetAXISAppReadReq net_app_req;
  RxSarAppReqRsp    rx_sar_rsp;

  NetAXIS          cur_word{};
  TcpSessionBuffer cur_word_length = 0;
  // consume two words
  input_tcp_packets.read();
  input_tcp_packets.read();
  int sim_cycle = 0;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        net_app_req.data = AppReadReq(0x1, 0x100);
        net_app_req.dest = 0x1;
        net_app_read_data_req.write(net_app_req);
        break;
      case 2:
        net_app_req.data = AppReadReq(0x2, 0x100);
        net_app_req.dest = 0x2;
        net_app_read_data_req.write(net_app_req);
        net_app_req.data = AppReadReq(0x4, 0x100);
        net_app_req.dest = 0x2;
        net_app_read_data_req.write(net_app_req);
        break;
      case 3:
        rx_sar_rsp.app_read   = 0x1234567;
        rx_sar_rsp.session_id = 0x1;
        rx_sar_to_rx_app_rsp.write(rx_sar_rsp);
        break;
      case 4:
        do {
          input_tcp_packets.read(cur_word);
          rx_eng_to_rx_app_data.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
        } while (cur_word.last != 1);
        break;
      case 5:
        rx_sar_rsp.app_read   = 0x1234567;
        rx_sar_rsp.session_id = 0x2;
        rx_sar_to_rx_app_rsp.write(rx_sar_rsp);
      case 6:

        do {
          input_tcp_packets.read(cur_word);
          rx_eng_to_rx_app_data.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
        } while (cur_word.last != 1);

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
        outputFile, net_app_read_data_rsp, rx_app_to_rx_sar_req, rx_app_to_net_app_data, sim_cycle);
    sim_cycle++;
  }

  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *input_tcp_pcap_file = argv[1];
  cout << "Read TCP Packets from " << input_tcp_pcap_file << endl;
  stream<NetAXIS> input_tcp_packets("input_tcp_packets");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_packets);

  TestPortHandler();
  TestDataHandler(input_tcp_packets);

  return 0;
}
