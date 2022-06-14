
#include "toe/toe_conn.hpp"

void toe_top(
    // rx engine
    stream<NetAXIS> &rx_ip_pkt_in,
    // tx engine
    stream<DataMoverCmd> &mover_read_mem_cmd_out,
    stream<NetAXIS> &     mover_read_mem_data_in,
#if (TCP_NODELAY)
    stream<NetAXIS> &tx_app_to_tx_eng_data,
#endif
    stream<NetAXIS> &tx_ip_pkt_out,
    // rx app
    stream<NetAXISListenPortReq> &  net_app_to_rx_app_listen_port_req,
    stream<NetAXISListenPortRsp> &  rx_app_to_net_app_listen_port_rsp,
    stream<NetAXISAppReadReq> &     net_app_read_data_req,
    stream<NetAXISAppReadRsp> &     net_app_read_data_rsp,
    stream<NetAXIS> &               rx_app_to_net_app_data,
    stream<NetAXISAppNotification> &net_app_notification,
    // tx app
    stream<NetAXISAppOpenConnReq> &       net_app_to_tx_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp> &       tx_app_to_net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &      net_app_to_tx_app_close_conn_req,
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    stream<NetAXISAppTransDataReq> &      net_app_to_tx_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &      tx_app_to_net_app_trans_data_rsp,
    stream<NetAXIS> &                     net_app_trans_data,
    stream<DataMoverCmd> &                tx_app_to_mem_write_cmd,
    stream<NetAXIS> &                     tx_app_to_mem_write_data,
    stream<DataMoverStatus> &             mem_to_tx_app_write_status,
    // CAM
    stream<RtlSLookupToCamLupReq> &rtl_slookup_to_cam_lookup_req,
    stream<RtlCamToSlookupLupRsp> &rtl_cam_to_slookup_lookup_rsp,
    stream<RtlSlookupToCamUpdReq> &rtl_slookup_to_cam_update_req,
    stream<RtlCamToSlookupUpdRsp> &rtl_cam_to_slookup_update_rsp,
    // registers
    ap_uint<16> &reg_session_cnt,
    // in big endian
    IpAddr &my_ip_addr);