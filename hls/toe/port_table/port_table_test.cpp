#include "port_table.hpp"
#include "toe/mock/mock_toe.hpp"

using namespace hls;

void EmptyFifos(std::ofstream &           out_stream,
                stream<PtableToRxEngRsp> &ptable_to_rx_eng_check_rsp,
                stream<ListenPortRsp> &   ptable_to_rx_app_listen_port_rsp,
                stream<TcpPortNumber> &   ptable_to_tx_app_port_rsp,
                int                       sim_cycle) {
  PtableToRxEngRsp to_rx_eng_rsp;
  ListenPortRsp    to_rx_app_rsp;
  TcpPortNumber    to_tx_app_free_port;

  while (!ptable_to_rx_eng_check_rsp.empty()) {
    ptable_to_rx_eng_check_rsp.read(to_rx_eng_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Porttable to Rx Engine response\n";
    out_stream << to_rx_eng_rsp.to_string() << "\n";
  }
  while (!ptable_to_rx_app_listen_port_rsp.empty()) {
    ptable_to_rx_app_listen_port_rsp.read(to_rx_app_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Porttable to Rx App response\n";
    out_stream << to_rx_app_rsp.data.to_string() << "\nRole ID: " << to_rx_app_rsp.dest << endl;
  }
  while (!ptable_to_tx_app_port_rsp.empty()) {
    ptable_to_tx_app_port_rsp.read(to_tx_app_free_port);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Porttable to Tx App free port\n";
    out_stream << to_tx_app_free_port << endl;
  }
}

int main() {
  stream<TcpPortNumber>    rx_eng_to_ptable_check_req;
  stream<ListenPortReq>    rx_app_to_ptable_listen_port_req;
  stream<TcpPortNumber>    slup_to_ptable_realease_port;
  stream<NetAXISDest>      tx_app_to_ptable_port_req;
  stream<PtableToRxEngRsp> ptable_to_rx_eng_check_rsp;
  stream<ListenPortRsp>    ptable_to_rx_app_listen_port_rsp;
  stream<TcpPortNumber>    ptable_to_tx_app_port_rsp;

  // open output file
  std::ofstream outputFile;

  outputFile.open("./out.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }

  // check net app listen req/rsp and check
  ListenPortReq app_req;
  app_req.data = 0x0080;
  app_req.dest = 0x1;
  TcpPortNumber rx_eng_req;

  int sim_cycle = 0;
  outputFile << "Test NetApp listen port req/rsp" << endl;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        rx_app_to_ptable_listen_port_req.write(app_req);
        break;
      case 2:
        app_req.data = 0x0081;
        app_req.dest = 0x02;
        rx_app_to_ptable_listen_port_req.write(app_req);
        break;
      case 3:
        app_req.data = 0x0081;
        app_req.dest = 0x03;
        rx_app_to_ptable_listen_port_req.write(app_req);
        break;
      case 5:
        rx_eng_req = 0x0080;
        rx_eng_to_ptable_check_req.write(rx_eng_req);
        break;
      case 6:
        rx_eng_req = 0x0081;
        rx_eng_to_ptable_check_req.write(rx_eng_req);
        break;
      case 7:
        rx_eng_req = 0x0082;
        rx_eng_to_ptable_check_req.write(rx_eng_req);
        break;
      default:
        break;
    }
    port_table(rx_eng_to_ptable_check_req,
               rx_app_to_ptable_listen_port_req,
               slup_to_ptable_realease_port,
               tx_app_to_ptable_port_req,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_port_rsp);
    EmptyFifos(outputFile,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_port_rsp,
               sim_cycle);
    sim_cycle++;
  }
  outputFile << "--------------------------\n\n";

  // check tx app get free port and check
  sim_cycle                = 0;
  NetAXISDest tx_app_tdest = 1;
  outputFile << "Test NetApp get free port req/rsp" << endl;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        tx_app_to_ptable_port_req.write(tx_app_tdest);
        break;
      case 2:
        rx_eng_to_ptable_check_req.write(32789);
        tx_app_tdest = 2;
        tx_app_to_ptable_port_req.write(tx_app_tdest);
        break;
      case 3:
        tx_app_tdest = 3;
        tx_app_to_ptable_port_req.write(tx_app_tdest);
        break;
      case 4:
        break;
      default:
        break;
    }
    port_table(rx_eng_to_ptable_check_req,
               rx_app_to_ptable_listen_port_req,
               slup_to_ptable_realease_port,
               tx_app_to_ptable_port_req,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_port_rsp);
    EmptyFifos(outputFile,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_port_rsp,
               sim_cycle);
    sim_cycle++;
  }
  outputFile << "--------------------------\n\n";

  return 0;
}