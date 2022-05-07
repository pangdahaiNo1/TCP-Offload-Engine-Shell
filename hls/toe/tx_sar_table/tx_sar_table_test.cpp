
#include "toe/mock/mock_toe.hpp"
#include "tx_sar_table.hpp"
using namespace hls;

void EmptyFifos(std::ofstream &          out_stream,
                stream<TxSarToRxEngRsp> &tx_sar_to_rx_eng_rsp,
                stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
                stream<TxSarToTxAppRsp> &tx_sar_to_tx_app_rsp,
                int                      sim_cycle) {
  TxSarToRxEngRsp to_rx_eng_rsp;
  TxSarToTxEngRsp to_tx_eng_rsp;
  TxSarToTxAppRsp to_tx_app_rsp;

  while (!tx_sar_to_rx_eng_rsp.empty()) {
    tx_sar_to_rx_eng_rsp.read(to_rx_eng_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx SAR to Rx Eng\n";
    out_stream << to_rx_eng_rsp.to_string() << "\n";
  }

  while (!tx_sar_to_tx_eng_rsp.empty()) {
    tx_sar_to_tx_eng_rsp.read(to_tx_eng_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx SAR to Tx Eng\n";
    out_stream << to_tx_eng_rsp.to_string() << "\n";
  }

  while (!tx_sar_to_tx_app_rsp.empty()) {
    tx_sar_to_tx_app_rsp.read(to_tx_app_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx SAR to Tx App\n";
    out_stream << to_tx_app_rsp.to_string() << "\n";
  }
}

int main() {
  stream<RxEngToTxSarReq> rx_eng_to_tx_sar_req;
  stream<TxSarToRxEngRsp> tx_sar_to_rx_eng_rsp;
  stream<TxEngToTxSarReq> tx_eng_to_tx_sar_req;
  stream<TxSarToTxEngRsp> tx_sar_to_tx_eng_rsp;
  stream<TxAppToTxSarReq> tx_app_to_tx_sar_req;
  stream<TxSarToTxAppRsp> tx_sar_to_tx_app_rsp;

  // open output file
  std::ofstream outputFile;

  outputFile.open("./out.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }

  int sim_cycle = 0;

  RxEngToTxSarReq rx_eng_req;
  TxEngToTxSarReq tx_eng_req;
  TxAppToTxSarReq tx_app_req;
  while (sim_cycle < 200) {
    switch (sim_cycle) {
      case 1:
        // init session tx sar entry by rx engine
        rx_eng_req.write           = 1;
        rx_eng_req.session_id      = 0x1;
        rx_eng_req.ackd            = 0x12345678;
        rx_eng_req.recv_window     = 0xFFFF;
        rx_eng_req.win_shift       = 2;
        rx_eng_req.win_shift_write = 1;
        rx_eng_req.cong_window     = 0xFFFF;
        rx_eng_to_tx_sar_req.write(rx_eng_req);

        break;
      case 2:
        // init session by tx engine
        tx_eng_req = TxEngToTxSarReq(0x2, 0x12345678, 1, 1, 1, 1);
        tx_eng_to_tx_sar_req.write(tx_eng_req);
        break;
      case 3:
        tx_app_req.app_written = 0x4321;
        tx_app_req.session_id  = 0x1;
        tx_app_to_tx_sar_req.write(tx_app_req);
        break;
      case 4:
        // read tx sar table
        rx_eng_req = RxEngToTxSarReq(0x1);
        rx_eng_to_tx_sar_req.write(rx_eng_req);
        break;
      case 5:
        tx_eng_req = TxEngToTxSarReq(0x1);
        tx_eng_to_tx_sar_req.write(tx_eng_req);
        break;
      default:
        break;
    }
    tx_sar_table(rx_eng_to_tx_sar_req,
                 tx_sar_to_rx_eng_rsp,
                 tx_eng_to_tx_sar_req,
                 tx_sar_to_tx_eng_rsp,
                 tx_app_to_tx_sar_req,
                 tx_sar_to_tx_app_rsp);
    EmptyFifos(
        outputFile, tx_sar_to_rx_eng_rsp, tx_sar_to_tx_eng_rsp, tx_sar_to_tx_app_rsp, sim_cycle);
    sim_cycle++;
  }

  return 0;
}
