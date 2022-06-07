#include "session_lookup_controller.hpp"
#include "toe/mock/mock_cam.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"

using namespace hls;

void EmptyFifos(MockLogger &                    logger,
                stream<NetAXISDest> &           slookup_to_rx_app_check_tdset_rsp,
                stream<SessionLookupRsp> &      slookup_to_rx_eng_rsp,
                stream<SessionLookupRsp> &      slookup_to_tx_app_rsp,
                stream<NetAXISDest> &           slookup_to_tx_app_check_tdest_rsp,
                stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
                stream<TcpPortNumber> &         slookup_to_ptable_release_port_req) {
  NetAXISDest            rx_or_tx_app_tdest;
  SessionLookupRsp       slookup_rsp;
  ReverseTableToTxEngRsp tx_eng_rsp;
  TcpPortNumber          release_port;
  while (!slookup_to_rx_app_check_tdset_rsp.empty()) {
    slookup_to_rx_app_check_tdset_rsp.read(rx_or_tx_app_tdest);
    logger.Info(SLUP_CTRL, RX_APP_IF, "TDEST Rsp", rx_or_tx_app_tdest.to_string(16));
  }
  while (!slookup_to_rx_eng_rsp.empty()) {
    slookup_to_rx_eng_rsp.read(slookup_rsp);
    logger.Info(SLUP_CTRL, RX_ENGINE, "SessionLup Rsp", slookup_rsp.to_string());
  }
  while (!slookup_to_tx_app_rsp.empty()) {
    slookup_to_tx_app_rsp.read(slookup_rsp);
    logger.Info(SLUP_CTRL, TX_APP_IF, "SessionLup Rsp", slookup_rsp.to_string());
  }
  while (!slookup_to_tx_app_check_tdest_rsp.empty()) {
    slookup_to_tx_app_check_tdest_rsp.read(rx_or_tx_app_tdest);
    logger.Info(SLUP_CTRL, TX_APP_IF, "TDEST Rsp", rx_or_tx_app_tdest.to_string(16));
  }
  while (!slookup_rev_table_to_tx_eng_rsp.empty()) {
    slookup_rev_table_to_tx_eng_rsp.read(tx_eng_rsp);
    logger.Info(SLUP_CTRL, TX_ENGINE, "RevTable Rsp", tx_eng_rsp.to_string(), false);
  }
  while (!slookup_to_ptable_release_port_req.empty()) {
    slookup_to_ptable_release_port_req.read(release_port);
    logger.Info(SLUP_CTRL, PORT_TBLE, "Release Port Req", release_port.to_string(16), false);
  }
}

MockLogger logger("./slookup_inner.log", SLUP_CTRL);

int main() {
  // from sttable
  stream<TcpSessionID> sttable_to_slookup_release_req;
  // rx app
  stream<TcpSessionID> rx_app_to_slookup_check_tdest_req;
  stream<NetAXISDest>  slookup_to_rx_app_check_tdset_rsp;
  // rx eng
  stream<RxEngToSlookupReq> rx_eng_to_slookup_req;
  stream<SessionLookupRsp>  slookup_to_rx_eng_rsp;
  // tx app
  stream<TxAppToSlookupReq> tx_app_to_slookup_req;
  stream<SessionLookupRsp>  slookup_to_tx_app_rsp;
  stream<TcpSessionID>      tx_app_to_slookup_check_tdest_req;
  stream<NetAXISDest>       slookup_to_tx_app_check_tdest_rsp;
  // tx eng
  stream<ap_uint<16> >           tx_eng_to_slookup_rev_table_req;
  stream<ReverseTableToTxEngRsp> slookup_rev_table_to_tx_eng_rsp;
  // CAM
  stream<RtlSLookupToCamLupReq> rtl_slookup_to_cam_lookup_req;
  stream<RtlCamToSlookupLupRsp> rtl_cam_to_slookup_lookup_rsp;
  stream<RtlSlookupToCamUpdReq> rtl_slookup_to_cam_update_req;
  stream<RtlCamToSlookupUpdRsp> rtl_cam_to_slookup_update_rsp;
  // to ptable
  stream<TcpPortNumber> slookup_to_ptable_release_port_req;
  // registers
  ap_uint<16> reg_session_cnt;
  IpAddr      my_ip_addr = mock_src_ip_addr;

  MockLogger top_logger("./slookup_top.log", SLUP_CTRL);

  int sim_cycle = 0;

  TxAppToSlookupReq tx_app_req;
  RxEngToSlookupReq rx_eng_req;
  TcpSessionID      session_id;
  MockCam           mock_cam;
  while (sim_cycle < 100) {
    switch (sim_cycle) {
      case 1:
        tx_app_req.four_tuple = mock_tuple;
        tx_app_req.role_id    = 0x1;
        tx_app_to_slookup_req.write(tx_app_req);
        break;
      case 2:
        rx_eng_req.four_tuple     = reverse_mock_tuple;
        rx_eng_req.role_id        = 0x3;
        rx_eng_req.allow_creation = false;
        rx_eng_to_slookup_req.write(rx_eng_req);
        break;
      case 3:
        break;
      case 4:
        rx_eng_req.allow_creation = true;
        rx_eng_req.four_tuple     = mock_tuple;  // is the another key
        rx_eng_req.role_id        = 0x3;
        rx_eng_to_slookup_req.write(rx_eng_req);
        tx_app_to_slookup_check_tdest_req.write(0);

        break;
      case 5:
        break;
      case 13:
        tx_app_to_slookup_check_tdest_req.write(0);
        rx_app_to_slookup_check_tdest_req.write(1);
        break;
      case 20:
        sttable_to_slookup_release_req.write(0);
        sttable_to_slookup_release_req.write(1);
      default:
        break;
    }
    session_lookup_controller(sttable_to_slookup_release_req,
                              rx_app_to_slookup_check_tdest_req,
                              slookup_to_rx_app_check_tdset_rsp,
                              rx_eng_to_slookup_req,
                              slookup_to_rx_eng_rsp,
                              tx_app_to_slookup_req,
                              slookup_to_tx_app_rsp,
                              tx_app_to_slookup_check_tdest_req,
                              slookup_to_tx_app_check_tdest_rsp,
                              tx_eng_to_slookup_rev_table_req,
                              slookup_rev_table_to_tx_eng_rsp,
                              rtl_slookup_to_cam_lookup_req,
                              rtl_cam_to_slookup_lookup_rsp,
                              rtl_slookup_to_cam_update_req,
                              rtl_cam_to_slookup_update_rsp,
                              slookup_to_ptable_release_port_req,
                              reg_session_cnt,
                              my_ip_addr);
    mock_cam.MockCamIntf(top_logger,
                         rtl_slookup_to_cam_lookup_req,
                         rtl_cam_to_slookup_lookup_rsp,
                         rtl_slookup_to_cam_update_req,
                         rtl_cam_to_slookup_update_rsp);
    EmptyFifos(top_logger,
               slookup_to_rx_app_check_tdset_rsp,
               slookup_to_rx_eng_rsp,
               slookup_to_tx_app_rsp,
               slookup_to_tx_app_check_tdest_rsp,
               slookup_rev_table_to_tx_eng_rsp,
               slookup_to_ptable_release_port_req);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}
