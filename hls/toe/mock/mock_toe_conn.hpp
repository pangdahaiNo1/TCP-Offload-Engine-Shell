#ifndef _MOCK_TOE_CONN_HPP_
#define _MOCK_TOE_CONN_HPP_
#include "toe/mock/mock_cam.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_memory.hpp"
#include "toe/mock/mock_toe.hpp"
#include "toe/toe.hpp"
#include "toe/toe_conn.hpp"
void ConnectToeIntfWithToe(ToeIntf &toe_intf) {
  toe_top(toe_intf.rx_ip_pkt_in,
#if !TCP_RX_DDR_BYPASS
          toe_intf.rx_eng_to_mover_write_cmd,
          toe_intf.rx_eng_to_mover_write_data,
          toe_intf.mover_to_rx_eng_write_status,
#endif
          toe_intf.tx_eng_to_mover_read_cmd,
          toe_intf.mover_to_tx_eng_read_data,
          toe_intf.tx_ip_pkt_out,
          toe_intf.net_app_to_rx_app_listen_port_req,
          toe_intf.rx_app_to_net_app_listen_port_rsp,
          toe_intf.net_app_to_rx_app_recv_data_req,
          toe_intf.rx_app_to_net_app_recv_data_rsp,
          toe_intf.net_app_recv_data,
          toe_intf.net_app_notification,
#if !(TCP_RX_DDR_BYPASS)
          toe_intf.rx_app_to_mover_read_cmd,
          toe_intf.mover_to_rx_app_read_data,
#endif
          toe_intf.net_app_to_tx_app_open_conn_req,
          toe_intf.tx_app_to_net_app_open_conn_rsp,
          toe_intf.net_app_to_tx_app_close_conn_req,
          toe_intf.net_app_new_client_notification,
          toe_intf.net_app_to_tx_app_trans_data_req,
          toe_intf.tx_app_to_net_app_trans_data_rsp,
          toe_intf.net_app_trans_data,
          toe_intf.tx_app_to_mover_write_cmd,
          toe_intf.tx_app_to_mover_write_data,
          toe_intf.mover_to_tx_app_write_status,
          toe_intf.rtl_slookup_to_cam_lookup_req,
          toe_intf.rtl_cam_to_slookup_lookup_rsp,
          toe_intf.rtl_slookup_to_cam_update_req,
          toe_intf.rtl_cam_to_slookup_update_rsp,
          toe_intf.reg_session_cnt,
          toe_intf.my_ip_addr);
  return;
}

void ConnectToeIntfWithMockCam(MockLogger &logger, ToeIntf &toe_intf, MockCam &mock_cam) {
  mock_cam.MockCamIntf(logger,
                       toe_intf.rtl_slookup_to_cam_lookup_req,
                       toe_intf.rtl_cam_to_slookup_lookup_rsp,
                       toe_intf.rtl_slookup_to_cam_update_req,
                       toe_intf.rtl_cam_to_slookup_update_rsp);
}

void ConnectToeTxIntfWithMockMem(MockLogger &logger, ToeIntf &toe_intf, MockMem &mock_mem) {
  mock_mem.MockMemIntf(logger,
                       toe_intf.tx_eng_to_mover_read_cmd,
                       toe_intf.mover_to_tx_eng_read_data,
                       toe_intf.tx_app_to_mover_write_cmd,
                       toe_intf.tx_app_to_mover_write_data,
                       toe_intf.mover_to_tx_app_write_status);
}

void ConnectToeRxIntfWithMockMem(MockLogger &logger, ToeIntf &toe_intf, MockMem &mock_mem) {
  mock_mem.MockMemIntf(logger,
                       toe_intf.rx_app_to_mover_read_cmd,
                       toe_intf.mover_to_rx_app_read_data,
                       toe_intf.rx_eng_to_mover_write_cmd,
                       toe_intf.rx_eng_to_mover_write_data,
                       toe_intf.mover_to_rx_eng_write_status);
}

#endif