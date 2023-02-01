
#include "rx_sar_table.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"
using namespace hls;

void EmptyFifos(MockLogger              &logger,
                stream<RxSarAppReqRsp>  &rx_sar_to_rx_app_rsp,
                stream<RxSarTableEntry> &rx_sar_to_rx_eng_rsp,
                stream<RxSarLookupRsp>  &rx_sar_to_tx_eng_lup_rsp) {
  RxSarAppReqRsp  to_rx_app_rsp;
  RxSarTableEntry to_rx_eng_rsp;
  RxSarLookupRsp  to_tx_eng_rsp;
  while (!(rx_sar_to_rx_app_rsp.empty())) {
    rx_sar_to_rx_app_rsp.read(to_rx_app_rsp);
    logger.Info(RX_SAR_TB, RX_APP_IF, "R/W Rsp", to_rx_app_rsp.to_string());
  }

  while (!(rx_sar_to_rx_eng_rsp.empty())) {
    rx_sar_to_rx_eng_rsp.read(to_rx_eng_rsp);
    logger.Info(RX_SAR_TB, RX_ENGINE, "R/W Rsp", to_rx_eng_rsp.to_string());
  }
  while (!(rx_sar_to_tx_eng_lup_rsp.empty())) {
    rx_sar_to_tx_eng_lup_rsp.read(to_tx_eng_rsp);
    logger.Info(RX_SAR_TB, TX_ENGINE, "Lup Rsp", to_tx_eng_rsp.to_string());
  }
}
MockLogger logger("./rx_sar_inner.log", RX_ENGINE);

int main() {
  stream<RxSarAppReqRsp>  rx_app_to_rx_sar_req;
  stream<RxSarAppReqRsp>  rx_sar_to_rx_app_rsp;
  stream<RxEngToRxSarReq> rx_eng_to_rx_sar_req;
  stream<RxSarTableEntry> rx_sar_to_rx_eng_rsp;
  stream<TcpSessionID>    tx_eng_to_rx_sar_lup_req;
  stream<RxSarLookupRsp>  rx_sar_to_tx_eng_lup_rsp;

  MockLogger top_logger("./rx_sar_top.log", RX_SAR_TB);

  RxEngToRxSarReq rx_eng_req;
  RxSarAppReqRsp  rx_app_req;
  TcpSessionID    tx_eng_req;
  int             sim_cycle = 0;
  while (sim_cycle < 200) {
    switch (sim_cycle) {
      case 1:
        // rx eng init session 1
        rx_eng_req.init       = 1;
        rx_eng_req.session_id = 0x1;
        rx_eng_req.recvd      = 0x12345678;
        rx_eng_req.write      = 1;
        rx_eng_req.win_shift  = 2;
        rx_eng_to_rx_sar_req.write(rx_eng_req);
        break;
      case 2:
        // rx engine init session 2
        rx_eng_req.init       = 1;
        rx_eng_req.session_id = 0x2;
        rx_eng_req.recvd      = 0x12345678;
        rx_eng_req.write      = 1;
        rx_eng_req.win_shift  = 2;
        rx_eng_to_rx_sar_req.write(rx_eng_req);
        break;
      case 3:
        // read app read ptr from session 1
        rx_app_req.session_id = 0x1;
        rx_app_to_rx_sar_req.write(rx_app_req);
        break;
      case 4:
        // update app read ptr
        rx_app_req.session_id = 0x1;
        rx_app_req.write      = 1;
        rx_app_req.app_read   = 0x04321;
        rx_app_to_rx_sar_req.write(rx_app_req);
        break;
      case 5:
        // will be ((0x4321-0x5678) - 1) >> WinScaleOpt
        tx_eng_req = 0x1;
        tx_eng_to_rx_sar_lup_req.write(tx_eng_req);
        break;
      case 6:
        tx_eng_req = 0x2;
        // will be 0x0ffff
        tx_eng_to_rx_sar_lup_req.write(tx_eng_req);
        break;
      default:
        break;
    }
    rx_sar_table(rx_app_to_rx_sar_req,
                 rx_sar_to_rx_app_rsp,
                 rx_eng_to_rx_sar_req,
                 rx_sar_to_rx_eng_rsp,
                 tx_eng_to_rx_sar_lup_req,
                 rx_sar_to_tx_eng_lup_rsp);
    EmptyFifos(top_logger, rx_sar_to_rx_app_rsp, rx_sar_to_rx_eng_rsp, rx_sar_to_tx_eng_lup_rsp);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}
