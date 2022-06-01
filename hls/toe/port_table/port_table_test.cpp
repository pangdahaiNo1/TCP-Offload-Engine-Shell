#include "port_table.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"
using namespace hls;

void EmptyFifos(MockLogger &              logger,
                stream<PtableToRxEngRsp> &ptable_to_rx_eng_check_rsp,
                stream<ListenPortRsp> &   ptable_to_rx_app_listen_port_rsp,
                stream<TcpPortNumber> &   ptable_to_tx_app_rsp) {
  PtableToRxEngRsp to_rx_eng_rsp;
  ListenPortRsp    to_rx_app_rsp;
  TcpPortNumber    to_tx_app_free_port;

  while (!ptable_to_rx_eng_check_rsp.empty()) {
    ptable_to_rx_eng_check_rsp.read(to_rx_eng_rsp);
    logger.Info("Porttable to Rx Engine rsp ", to_rx_eng_rsp.to_string(), false);
  }
  while (!ptable_to_rx_app_listen_port_rsp.empty()) {
    ptable_to_rx_app_listen_port_rsp.read(to_rx_app_rsp);
    logger.Info("Porttable to Rx App rsp ", to_rx_app_rsp.to_string(), false);
  }
  while (!ptable_to_tx_app_rsp.empty()) {
    ptable_to_tx_app_rsp.read(to_tx_app_free_port);
    logger.Info("Porttable to Tx App free port ", to_tx_app_free_port.to_string(16), false);
  }
}
MockLogger logger("./ptable_inner.log");

int main() {
  stream<TcpPortNumber>    rx_eng_to_ptable_check_req;
  stream<ListenPortReq>    rx_app_to_ptable_listen_port_req;
  stream<TcpPortNumber>    slookup_to_ptable_release_port_req;
  stream<NetAXISDest>      tx_app_to_ptable_req;
  stream<PtableToRxEngRsp> ptable_to_rx_eng_check_rsp;
  stream<ListenPortRsp>    ptable_to_rx_app_listen_port_rsp;
  stream<TcpPortNumber>    ptable_to_tx_app_rsp;

  // open output file
  MockLogger top_logger("ptable_top.log");

  // check net app listen req/rsp and check
  ListenPortReq app_req;

  TcpPortNumber rx_eng_req;

  int sim_cycle = 0;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        app_req.data = 0x0080;
        app_req.dest = 0x1;
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
               slookup_to_ptable_release_port_req,
               tx_app_to_ptable_req,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_rsp);
    EmptyFifos(top_logger,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_rsp);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  // check tx app get free port and check
  sim_cycle                = 0;
  NetAXISDest tx_app_tdest = 0;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        tx_app_tdest = 1;
        tx_app_to_ptable_req.write(tx_app_tdest);
        break;
      case 2:
        rx_eng_to_ptable_check_req.write(0x8000);
        tx_app_tdest = 2;
        tx_app_to_ptable_req.write(tx_app_tdest);
        break;
      case 3:
        tx_app_tdest = 3;
        tx_app_to_ptable_req.write(tx_app_tdest);
        break;
      case 4:
        rx_eng_to_ptable_check_req.write(0x8002);

        break;
      default:
        break;
    }
    port_table(rx_eng_to_ptable_check_req,
               rx_app_to_ptable_listen_port_req,
               slookup_to_ptable_release_port_req,
               tx_app_to_ptable_req,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_rsp);
    EmptyFifos(top_logger,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_rsp);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}