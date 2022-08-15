
#include "tx_sar_table.hpp"
// DONOT change the header files sequence
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"
using namespace hls;

void EmptyFifos(MockLogger              &logger,
                stream<TxSarToRxEngRsp> &tx_sar_to_rx_eng_rsp,
                stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
                stream<TxSarToTxAppReq> &tx_sar_to_tx_app_upd_req) {
  TxSarToRxEngRsp to_rx_eng_rsp;
  TxSarToTxEngRsp to_tx_eng_rsp;
  TxSarToTxAppReq to_tx_app_rsp;

  while (!tx_sar_to_rx_eng_rsp.empty()) {
    tx_sar_to_rx_eng_rsp.read(to_rx_eng_rsp);
    logger.Info(TX_SAR_TB, RX_ENGINE, "R/W Rsp", to_rx_eng_rsp.to_string());
  }

  while (!tx_sar_to_tx_eng_rsp.empty()) {
    tx_sar_to_tx_eng_rsp.read(to_tx_eng_rsp);
    logger.Info(TX_SAR_TB, TX_ENGINE, "R/W Rsp", to_rx_eng_rsp.to_string());
  }

  while (!tx_sar_to_tx_app_upd_req.empty()) {
    tx_sar_to_tx_app_upd_req.read(to_tx_app_rsp);
    logger.Info(TX_SAR_TB, TX_APP_IF, "Lup Rsp", to_rx_eng_rsp.to_string());
  }
}

MockLogger logger("./tx_sar_inner.log", TX_SAR_TB);

int main() {
  stream<RxEngToTxSarReq> rx_eng_to_tx_sar_req;
  stream<TxSarToRxEngRsp> tx_sar_to_rx_eng_rsp;
  stream<TxEngToTxSarReq> tx_eng_to_tx_sar_req;
  stream<TxSarToTxEngRsp> tx_sar_to_tx_eng_rsp;
  stream<TxAppToTxSarReq> tx_app_to_tx_sar_upd_req;
  stream<TxSarToTxAppReq> tx_sar_to_tx_app_upd_req;

  MockLogger top_logger("./tx_sar_top.log", TX_SAR_TB);
  int        sim_cycle = 0;

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
        tx_app_to_tx_sar_upd_req.write(tx_app_req);
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
    tx_sar_table(tx_app_to_tx_sar_upd_req,
                 rx_eng_to_tx_sar_req,
                 tx_sar_to_rx_eng_rsp,
                 tx_eng_to_tx_sar_req,
                 tx_sar_to_tx_eng_rsp,
                 tx_sar_to_tx_app_upd_req);
    EmptyFifos(top_logger, tx_sar_to_rx_eng_rsp, tx_sar_to_tx_eng_rsp, tx_sar_to_tx_app_upd_req);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}
