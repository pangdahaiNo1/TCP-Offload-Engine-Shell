#ifndef _MOCK_TOE_HPP_
#define _MOCK_TOE_HPP_
#pragma once

#include "utils/axi_utils.hpp"
/**
 * NOTE: all varibles are in big endian
 */
using std::string;
// test mac addr
ap_uint<48> mock_my_mac_addr     = SwapByte<48>(0x000A35029DE5);
ap_uint<32> mock_gateway_ip_addr = SwapByte<32>(0xC0A80001);
ap_uint<32> mock_subnet_mask     = SwapByte<32>(0xFFFFFF00);
// test tuple
IpAddr        mock_dst_ip_addr  = 0x0200A8C0;  // 192.168.0.2
IpAddr        mock_src_ip_addr  = 0x0500A8C0;  // 192.168.0.5
TcpPortNumber mock_dst_tcp_port = 0x1580;      // 32789
TcpPortNumber mock_src_tcp_port = 0x8000;      // 128

FourTuple mock_tuple(mock_src_ip_addr, mock_dst_ip_addr, mock_src_tcp_port, mock_dst_tcp_port);
FourTuple reverse_mock_tuple(mock_dst_ip_addr,
                             mock_src_ip_addr,
                             mock_dst_tcp_port,
                             mock_src_tcp_port);

struct ToeIntf {
  // rx engine
  stream<NetAXIS> rx_ip_pkt_in;
  // rx eng write to data mover
  stream<DataMoverCmd>    rx_eng_to_mover_write_cmd;
  stream<NetAXIS>         rx_eng_to_mover_write_data;
  stream<DataMoverStatus> mover_to_rx_eng_write_status;
  // tx engine
  // tx engine read from data mover
  stream<DataMoverCmd> tx_eng_to_mover_read_cmd;
  stream<NetAXIS>      mover_to_tx_eng_read_data;
  stream<NetAXIS>      tx_ip_pkt_out;
  // rx app
  stream<NetAXISListenPortReq>   net_app_to_rx_app_listen_port_req;
  stream<NetAXISListenPortRsp>   rx_app_to_net_app_listen_port_rsp;
  stream<NetAXISAppReadReq>      net_app_to_rx_app_recv_data_req;
  stream<NetAXISAppReadRsp>      rx_app_to_net_app_recv_data_rsp;
  stream<NetAXIS>                net_app_recv_data;
  stream<NetAXISAppNotification> net_app_notification;
  // rx app read from data mover
  stream<DataMoverCmd> rx_app_to_mover_read_cmd;
  stream<NetAXIS>      mover_to_rx_app_read_data;
  // tx app
  stream<NetAXISAppOpenConnReq>        net_app_to_tx_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>        tx_app_to_net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq>       net_app_to_tx_app_close_conn_req;
  stream<NetAXISNewClientNotification> net_app_new_client_notification;
  stream<NetAXISAppTransDataReq>       net_app_to_tx_app_trans_data_req;
  stream<NetAXISAppTransDataRsp>       tx_app_to_net_app_trans_data_rsp;
  stream<NetAXIS>                      net_app_trans_data;
  // tx app write to data mover
  stream<DataMoverCmd>    tx_app_to_mover_write_cmd;
  stream<NetAXIS>         tx_app_to_mover_write_data;
  stream<DataMoverStatus> mover_to_tx_app_write_status;
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
    // rx eng
    rx_ip_pkt_in.set_name(("rx_ip_pkt_in" + fifo_suffix).c_str());
    rx_eng_to_mover_write_cmd.set_name(("rx_eng_to_mover_write_cmd" + fifo_suffix).c_str());
    rx_eng_to_mover_write_data.set_name(("rx_eng_to_mover_write_data" + fifo_suffix).c_str());
    mover_to_rx_eng_write_status.set_name(("mover_to_rx_eng_write_status" + fifo_suffix).c_str());
    // tx eng
    tx_eng_to_mover_read_cmd.set_name(("tx_eng_to_mover_read_cmd" + fifo_suffix).c_str());
    mover_to_tx_eng_read_data.set_name(("mover_to_tx_eng_read_data" + fifo_suffix).c_str());
    tx_ip_pkt_out.set_name(("tx_ip_pkt_out" + fifo_suffix).c_str());
    // rx app
    net_app_to_rx_app_listen_port_req.set_name(
        ("net_app_to_rx_app_listen_port_req" + fifo_suffix).c_str());
    rx_app_to_net_app_listen_port_rsp.set_name(
        ("rx_app_to_net_app_listen_port_rsp" + fifo_suffix).c_str());
    net_app_to_rx_app_recv_data_req.set_name(
        ("net_app_to_rx_app_recv_data_req" + fifo_suffix).c_str());
    rx_app_to_net_app_recv_data_rsp.set_name(
        ("rx_app_to_net_app_recv_data_rsp" + fifo_suffix).c_str());
    net_app_recv_data.set_name(("net_app_recv_data" + fifo_suffix).c_str());
    net_app_notification.set_name(("net_app_notification" + fifo_suffix).c_str());
    rx_app_to_mover_read_cmd.set_name(("rx_app_to_mover_read_cmd" + fifo_suffix).c_str());
    mover_to_rx_app_read_data.set_name(("mover_to_rx_app_read_data" + fifo_suffix).c_str());
    // tx app
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
    tx_app_to_mover_write_cmd.set_name(("tx_app_to_mover_write_cmd" + fifo_suffix).c_str());
    tx_app_to_mover_write_data.set_name(("tx_app_to_mover_write_data" + fifo_suffix).c_str());
    mover_to_tx_app_write_status.set_name(("mover_to_tx_app_write_status" + fifo_suffix).c_str());
    // CAM
    rtl_slookup_to_cam_lookup_req.set_name(("rtl_slookup_to_cam_lookup_req" + fifo_suffix).c_str());
    rtl_cam_to_slookup_lookup_rsp.set_name(("rtl_cam_to_slookup_lookup_rsp" + fifo_suffix).c_str());
    rtl_slookup_to_cam_update_req.set_name(("rtl_slookup_to_cam_update_req" + fifo_suffix).c_str());
    rtl_cam_to_slookup_update_rsp.set_name(("rtl_cam_to_slookup_update_rsp" + fifo_suffix).c_str());
  }
};

#endif