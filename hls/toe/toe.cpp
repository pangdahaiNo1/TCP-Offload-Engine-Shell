#include "toe/ack_delay/ack_delay.hpp"
#include "toe/event_engine/event_engine.hpp"
#include "toe/port_table/port_table.hpp"
#include "toe/rx_app_intf/rx_app_intf.hpp"
#include "toe/rx_engine/rx_engine.hpp"
#include "toe/rx_sar_table/rx_sar_table.hpp"
#include "toe/session_lookup_controller/session_lookup_controller.hpp"
#include "toe/state_table/state_table.hpp"
#include "toe/timer_wrapper/timer_wrapper.hpp"
#include "toe/tx_app_intf/tx_app_intf.hpp"
#include "toe/tx_engine/tx_engine.hpp"
#include "toe/tx_sar_table/tx_sar_table.hpp"

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
    stream<NetAXISAppTransDataRsp> &      tx_app_to_net_app_tans_data_rsp,
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
    IpAddr &my_ip_addr) {
   //   #pragma HLS                        DATAFLOW
      #pragma HLS INTERFACE ap_ctrl_none port = return
// interfaces
// rx engine
#pragma HLS INTERFACE axis register both port = rx_ip_pkt_in
// tx engine
#pragma HLS INTERFACE axis port = mover_read_mem_cmd_out
#pragma HLS aggregate variable = mover_read_mem_cmd_out compact = bit

#pragma HLS INTERFACE axis port = mover_read_mem_data_in
#if (TCP_NODELAY)
#pragma HLS INTERFACE axis port = tx_app_to_tx_eng_data
#endif
#pragma HLS INTERFACE axis port = tx_ip_pkt_out
// rx app
#pragma HLS INTERFACE axis register both port = net_app_to_rx_app_listen_port_req
#pragma HLS INTERFACE axis register both port = rx_app_to_net_app_listen_port_rsp
#pragma HLS INTERFACE axis register both port = net_app_read_data_req
#pragma HLS INTERFACE axis register both port = net_app_read_data_rsp
#pragma HLS INTERFACE axis register both port = rx_app_to_net_app_data
#pragma HLS INTERFACE axis register both port = net_app_notification
// tx app
#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_open_conn_req
#pragma HLS INTERFACE axis register both port = tx_app_to_net_app_open_conn_rsp
#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_close_conn_req
#pragma HLS INTERFACE axis register both port = net_app_new_client_notification
#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_trans_data_req
#pragma HLS INTERFACE axis register both port = tx_app_to_net_app_tans_data_rsp
#pragma HLS INTERFACE axis register both port = net_app_trans_data
#pragma HLS INTERFACE axis register both port = tx_app_to_mem_write_cmd
#pragma HLS INTERFACE axis register both port = tx_app_to_mem_write_data
#pragma HLS INTERFACE axis register both port = mem_to_tx_app_write_status
// CAM
#pragma HLS INTERFACE axis register port = rtl_slookup_to_cam_lookup_req
#pragma HLS INTERFACE axis register port = rtl_cam_to_slookup_lookup_rsp
#pragma HLS INTERFACE axis register port = rtl_slookup_to_cam_update_req
#pragma HLS INTERFACE axis register port = rtl_cam_to_slookup_update_rsp
#pragma HLS aggregate variable = rtl_slookup_to_cam_lookup_req compact = bit
#pragma HLS aggregate variable = rtl_cam_to_slookup_lookup_rsp compact = bit
#pragma HLS aggregate variable = rtl_slookup_to_cam_update_req compact = bit
#pragma HLS aggregate variable = rtl_cam_to_slookup_update_rsp compact = bit
// ip
#pragma HLS INTERFACE ap_stable register port = my_ip_addr name = my_ip_addr
// session count register
#pragma HLS INTERFACE ap_none register port = reg_session_cnt
  // some fifos
  // ack delay
  static stream<EventWithTuple, 4> event_eng_to_ack_delay_event_fifo(
      "event_eng_to_ack_delay_event_fifo");
#pragma HLS aggregate variable = event_eng_to_ack_delay_event_fifo compact = bit
  static stream<EventWithTuple, 32> ack_delay_to_tx_eng_event_fifo(
      "ack_delay_to_tx_eng_event_fifo");
#pragma HLS aggregate variable = ack_delay_to_tx_eng_event_fifo compact = bit
  static stream<ap_uint<1>, 4> ack_delay_read_cnt_fifo("ack_delay_read_cnt_fifo");
  static stream<ap_uint<1>, 4> ack_delay_write_cnt_fifo("ack_delay_write_cnt_fifo");

  // event engine
  static stream<Event, 4> tx_app_to_event_eng_set_event_fifo("tx_app_to_event_eng_set_event_fifo");
#pragma HLS aggregate variable = tx_app_to_event_eng_set_event_fifo compact = bit
  stream<EventWithTuple, 512> rx_eng_to_event_eng_set_event_fifo(
      "rx_eng_to_event_eng_set_event_fifo");
#pragma HLS aggregate variable = rx_eng_to_event_eng_set_event_fifo compact = bit
  stream<Event, 4> timer_to_event_eng_set_event_fifo("timer_to_event_eng_set_event_fifo");
#pragma HLS aggregate variable = timer_to_event_eng_set_event_fifo compact = bit
  stream<ap_uint<1> ,8> tx_eng_read_count_fifo("tx_eng_read_count_fifo");

  // port table
  stream<TcpPortNumber, 8> rx_eng_to_ptable_check_req_fifo("rx_eng_to_ptable_check_req_fifo");
  stream<ListenPortReq, 8> rx_app_to_ptable_listen_port_req_fifo(
      "rx_app_to_ptable_listen_port_req_fifo");
#pragma HLS aggregate variable = rx_app_to_ptable_listen_port_req_fifo compact = bit
  stream<TcpPortNumber, 8> slookup_to_ptable_release_port_req_fifo(
      "slookup_to_ptable_release_port_req_fifo");
  stream<NetAXISDest, 8>      tx_app_to_ptable_req_fifo("tx_app_to_ptable_req_fifo");
  stream<PtableToRxEngRsp, 8> ptable_to_rx_eng_check_rsp_fifo("ptable_to_rx_eng_check_rsp_fifo");
#pragma HLS aggregate variable = ptable_to_rx_eng_check_rsp_fifo compact = bit
  stream<ListenPortRsp, 8> ptable_to_rx_app_listen_port_rsp_fifo(
      "ptable_to_rx_app_listen_port_rsp_fifo");
#pragma HLS aggregate variable = ptable_to_rx_app_listen_port_rsp_fifo compact = bit
  stream<TcpPortNumber, 8> ptable_to_tx_app_rsp_fifo("ptable_to_tx_app_rsp_fifo");

  // rx app
  stream<RxSarAppReqRsp, 4> rx_app_to_rx_sar_req_fifo("rx_app_to_rx_sar_req_fifo");
#pragma HLS aggregate variable = rx_app_to_rx_sar_req_fifo compact = bit
  stream<RxSarAppReqRsp, 4> rx_sar_to_rx_app_rsp_fifo("rx_sar_to_rx_app_rsp_fifo");
#pragma HLS aggregate variable = rx_sar_to_rx_app_rsp_fifo compact = bit

  stream<NetAXISWord, 512> rx_eng_to_rx_app_data_fifo("rx_eng_to_rx_app_data_fifo");
#pragma HLS aggregate variable = rx_eng_to_rx_app_data_fifo compact = bit

  stream<AppNotificationNoTDEST, 4> rx_eng_to_rx_app_notification_fifo(
      "rx_eng_to_rx_app_notification_fifo");
#pragma HLS aggregate variable = rx_eng_to_rx_app_notification_fifo compact = bit

  stream<AppNotificationNoTDEST, 4> rtimer_to_rx_app_notification_fifo(
      "rtimer_to_rx_app_notification_fifo");
#pragma HLS aggregate variable = rtimer_to_rx_app_notification_fifo compact = bit

  stream<TcpSessionID, 4> rx_app_to_slookup_tdest_lookup_req_fifo(
      "rx_app_to_slookup_tdest_lookup_req_fifo");

  stream<NetAXISDest, 4> slookup_to_rx_app_tdest_lookup_rsp_fifo(
      "slookup_to_rx_app_tdest_lookup_rsp_fifo");

  // rx engine
  stream<RxEngToSlookupReq, 4> rx_eng_to_slookup_req_fifo("rx_eng_to_slookup_req_fifo");
#pragma HLS aggregate variable = rx_eng_to_slookup_req_fifo compact = bit

  stream<SessionLookupRsp, 4> slookup_to_rx_eng_rsp_fifo("slookup_to_rx_eng_rsp_fifo");
#pragma HLS aggregate variable = slookup_to_rx_eng_rsp_fifo compact = bit

  stream<RxEngToRxSarReq, 4> rx_eng_to_rx_sar_req_fifo("rx_eng_to_rx_sar_req_fifo");
#pragma HLS aggregate variable = rx_eng_to_rx_sar_req_fifo compact = bit

  stream<RxSarTableEntry, 4> rx_sar_to_rx_eng_rsp_fifo("rx_sar_to_rx_eng_rsp_fifo");
#pragma HLS aggregate variable = rx_sar_to_rx_eng_rsp_fifo compact = bit
  stream<RxEngToTxSarReq, 4> rx_eng_to_tx_sar_req_fifo("rx_eng_to_tx_sar_req_fifo");
#pragma HLS aggregate variable = rx_eng_to_tx_sar_req_fifo compact = bit
  stream<TxSarToRxEngRsp, 4> tx_sar_to_rx_eng_rsp_fifo("tx_sar_to_rx_eng_rsp_fifo");
#pragma HLS aggregate variable = tx_sar_to_rx_eng_rsp_fifo compact = bit
  stream<StateTableReq, 4> rx_eng_to_sttable_req_fifo("rx_eng_to_sttable_req_fifo");
#pragma HLS aggregate variable = rx_eng_to_sttable_req_fifo compact = bit
  stream<SessionState, 4> sttable_to_rx_eng_rsp_fifo("sttable_to_rx_eng_rsp_fifo");
#pragma HLS aggregate variable = sttable_to_rx_eng_rsp_fifo compact = bit
  stream<OpenConnRspNoTDEST, 4> rx_eng_to_tx_app_notification_fifo(
      "rx_eng_to_tx_app_notification_fifo");
#pragma HLS aggregate variable = rx_eng_to_tx_app_notification_fifo compact = bit
  stream<NewClientNotificationNoTDEST, 4> rx_eng_to_tx_app_new_client_notification_fifo(
      "rx_eng_to_tx_app_new_client_notification_fifo");
#pragma HLS aggregate variable = rx_eng_to_tx_app_new_client_notification_fifo compact = bit

  // rx sar table
  stream<TcpSessionID, 4>   tx_eng_to_rx_sar_lup_req_fifo("tx_eng_to_rx_sar_lup_req_fifo");
  stream<RxSarLookupRsp, 4> rx_sar_to_tx_eng_lup_rsp_fifo("rx_sar_to_tx_eng_lup_rsp_fifo");
#pragma HLS aggregate variable = rx_sar_to_tx_eng_lup_rsp_fifo compact = bit

  // session lookup
  stream<TcpSessionID, 4> sttable_to_slookup_release_req_fifo(
      "sttable_to_slookup_release_req_fifo");
  stream<TxAppToSlookupReq, 4> tx_app_to_slookup_req_fifo("tx_app_to_slookup_req_fifo");
#pragma HLS aggregate variable = tx_app_to_slookup_req_fifo compact = bit

  stream<SessionLookupRsp, 4> slookup_to_tx_app_rsp_fifo("slookup_to_tx_app_rsp_fifo");
#pragma HLS aggregate variable = slookup_to_tx_app_rsp_fifo compact = bit

  stream<TcpSessionID, 4> tx_app_to_slookup_check_tdest_req_fifo(
      "tx_app_to_slookup_check_tdest_req_fifo");
  stream<NetAXISDest, 4> slookup_to_tx_app_check_tdest_rsp_fifo(
      "slookup_to_tx_app_check_tdest_rsp_fifo");
  stream<ap_uint<16>, 4> tx_eng_to_slookup_rev_table_req_fifo(
      "tx_eng_to_slookup_rev_table_req_fifo");
  stream<ReverseTableToTxEngRsp, 4> slookup_rev_table_to_tx_eng_rsp_fifo(
      "slookup_rev_table_to_tx_eng_rsp_fifo");
#pragma HLS aggregate variable = slookup_rev_table_to_tx_eng_rsp_fifo compact = bit

  // state table
  stream<StateTableReq, 4> tx_app_to_sttable_req_fifo("tx_app_to_sttable_req_fifo");
#pragma HLS aggregate variable = tx_app_to_sttable_req_fifo compact = bit
  stream<TcpSessionID, 4> tx_app_to_sttable_lup_req_fifo("tx_app_to_sttable_lup_req_fifo");
  stream<TcpSessionID, 4> timer_to_sttable_release_state_fifo(
      "timer_to_sttable_release_state_fifo");
  stream<SessionState, 4> sttable_to_tx_app_rsp_fifo("sttable_to_tx_app_rsp_fifo");
#pragma HLS aggregate variable = sttable_to_tx_app_rsp_fifo compact = bit
  stream<SessionState, 4> sttable_to_tx_app_lup_rsp_fifo("sttable_to_tx_app_lup_rsp_fifo");
#pragma HLS aggregate variable = sttable_to_tx_app_lup_rsp_fifo compact = bit

  // timer wrapper
  stream<TcpSessionID, 4> rx_eng_to_timer_set_ctimer_fifo("rx_eng_to_timer_set_ctimer_fifo");

  stream<RxEngToRetransTimerReq, 4> rx_eng_to_timer_clear_rtimer_fifo(
      "rx_eng_to_timer_clear_rtimer_fifo");
#pragma HLS aggregate variable = rx_eng_to_timer_clear_rtimer_fifo compact = bit
  stream<TxEngToRetransTimerReq, 4> tx_eng_to_timer_set_rtimer_fifo(
      "tx_eng_to_timer_set_rtimer_fifo");
#pragma HLS aggregate variable = tx_eng_to_timer_set_rtimer_fifo compact = bit

  stream<OpenConnRspNoTDEST, 4> rtimer_to_tx_app_notification_fifo(
      "rtimer_to_tx_app_notification_fifo");
#pragma HLS aggregate variable = rtimer_to_tx_app_notification_fifo compact = bit

  stream<TcpSessionID, 4> rx_eng_to_timer_clear_ptimer_fifo("rx_eng_to_timer_clear_ptimer_fifo");
  stream<TcpSessionID, 4> tx_eng_to_timer_set_ptimer_fifo("tx_eng_to_timer_set_ptimer_fifo");

  // tx app intf
  stream<TxAppToTxSarReq, 4> tx_app_to_tx_sar_req_fifo("tx_app_to_tx_sar_req_fifo");
#pragma HLS aggregate variable = tx_app_to_tx_sar_req_fifo compact = bit
  stream<TxSarToTxAppRsp, 4> tx_sar_to_tx_app_rsp_fifo("tx_sar_to_tx_app_rsp_fifo");
#pragma HLS aggregate variable = tx_sar_to_tx_app_rsp_fifo compact = bit
  // tx engine
  stream<TxEngToTxSarReq, 4> tx_eng_to_tx_sar_req_fifo("tx_eng_to_tx_sar_req_fifo");
#pragma HLS aggregate variable = tx_eng_to_tx_sar_req_fifo compact = bit
  stream<TxSarToTxEngRsp, 4> tx_sar_to_tx_eng_rsp_fifo("tx_sar_to_tx_eng_rsp_fifo");
#pragma HLS aggregate variable = tx_sar_to_tx_eng_rsp_fifo compact = bit

  // instance
  ack_delay(event_eng_to_ack_delay_event_fifo,
            ack_delay_to_tx_eng_event_fifo,
            ack_delay_read_cnt_fifo,
            ack_delay_write_cnt_fifo);
  event_engine(tx_app_to_event_eng_set_event_fifo,
               rx_eng_to_event_eng_set_event_fifo,
               timer_to_event_eng_set_event_fifo,
               event_eng_to_ack_delay_event_fifo,
               ack_delay_read_cnt_fifo,
               ack_delay_write_cnt_fifo,
               tx_eng_read_count_fifo);
  port_table(rx_eng_to_ptable_check_req_fifo,
             rx_app_to_ptable_listen_port_req_fifo,
             slookup_to_ptable_release_port_req_fifo,
             tx_app_to_ptable_req_fifo,
             ptable_to_rx_eng_check_rsp_fifo,
             ptable_to_rx_app_listen_port_rsp_fifo,
             ptable_to_tx_app_rsp_fifo);
  // rx app intf
  rx_app_intf(net_app_to_rx_app_listen_port_req,
              rx_app_to_net_app_listen_port_rsp,
              rx_app_to_ptable_listen_port_req_fifo,
              ptable_to_rx_app_listen_port_rsp_fifo,
              net_app_read_data_req,
              net_app_read_data_rsp,
              rx_app_to_rx_sar_req_fifo,
              rx_sar_to_rx_app_rsp_fifo,
              rx_eng_to_rx_app_data_fifo,
              rx_app_to_net_app_data,
              rx_eng_to_rx_app_notification_fifo,
              rtimer_to_rx_app_notification_fifo,
              rx_app_to_slookup_tdest_lookup_req_fifo,
              slookup_to_rx_app_tdest_lookup_rsp_fifo,
              net_app_notification);
  // rx engine
  rx_engine(rx_ip_pkt_in,
            rx_eng_to_ptable_check_req_fifo,
            ptable_to_rx_eng_check_rsp_fifo,
            rx_eng_to_slookup_req_fifo,
            slookup_to_rx_eng_rsp_fifo,
            rx_eng_to_rx_sar_req_fifo,
            rx_sar_to_rx_eng_rsp_fifo,
            rx_eng_to_tx_sar_req_fifo,
            tx_sar_to_rx_eng_rsp_fifo,
            rx_eng_to_sttable_req_fifo,
            sttable_to_rx_eng_rsp_fifo,
            rx_eng_to_timer_set_ctimer_fifo,
            rx_eng_to_timer_clear_rtimer_fifo,
            rx_eng_to_timer_clear_ptimer_fifo,
            rx_eng_to_tx_app_notification_fifo,
            rx_eng_to_tx_app_new_client_notification_fifo,
            rx_eng_to_rx_app_notification_fifo,
            rx_eng_to_event_eng_set_event_fifo,
            rx_eng_to_rx_app_data_fifo);
  // rx sar
  rx_sar_table(rx_app_to_rx_sar_req_fifo,
               rx_sar_to_rx_app_rsp_fifo,
               rx_eng_to_rx_sar_req_fifo,
               rx_sar_to_rx_eng_rsp_fifo,
               tx_eng_to_rx_sar_lup_req_fifo,
               rx_sar_to_tx_eng_lup_rsp_fifo);

  session_lookup_controller(
      // from sttable
      sttable_to_slookup_release_req_fifo,
      // rx app
      rx_app_to_slookup_tdest_lookup_req_fifo,
      slookup_to_rx_app_tdest_lookup_rsp_fifo,
      // rx eng
      rx_eng_to_slookup_req_fifo,
      slookup_to_rx_eng_rsp_fifo,
      // tx app
      tx_app_to_slookup_req_fifo,
      slookup_to_tx_app_rsp_fifo,
      tx_app_to_slookup_check_tdest_req_fifo,
      slookup_to_tx_app_check_tdest_rsp_fifo,
      // tx eng
      tx_eng_to_slookup_rev_table_req_fifo,
      slookup_rev_table_to_tx_eng_rsp_fifo,
      // CAM
      rtl_slookup_to_cam_lookup_req,
      rtl_cam_to_slookup_lookup_rsp,
      rtl_slookup_to_cam_update_req,
      rtl_cam_to_slookup_update_rsp,
      // to ptable
      slookup_to_ptable_release_port_req_fifo,
      // registers
      reg_session_cnt,
      my_ip_addr);

  state_table(rx_eng_to_sttable_req_fifo,
              tx_app_to_sttable_req_fifo,
              tx_app_to_sttable_lup_req_fifo,
              timer_to_sttable_release_state_fifo,
              sttable_to_rx_eng_rsp_fifo,
              sttable_to_tx_app_rsp_fifo,
              sttable_to_tx_app_lup_rsp_fifo,
              sttable_to_slookup_release_req_fifo);

  timer_wrapper(rx_eng_to_timer_set_ctimer_fifo,
                rx_eng_to_timer_clear_rtimer_fifo,
                tx_eng_to_timer_set_rtimer_fifo,
                rtimer_to_rx_app_notification_fifo,
                rtimer_to_tx_app_notification_fifo,
                rx_eng_to_timer_clear_ptimer_fifo,
                tx_eng_to_timer_set_ptimer_fifo,
                timer_to_event_eng_set_event_fifo,
                timer_to_sttable_release_state_fifo);

  tx_app_intf(
      // net app connection request
      net_app_to_tx_app_open_conn_req,
      tx_app_to_net_app_open_conn_rsp,
      net_app_to_tx_app_close_conn_req,
      // rx eng -> net app
      rx_eng_to_tx_app_new_client_notification_fifo,
      net_app_new_client_notification,
      // rx eng
      rx_eng_to_tx_app_notification_fifo,
      // retrans timer
      rtimer_to_tx_app_notification_fifo,
      // session lookup, also for TDEST
      tx_app_to_slookup_req_fifo,
      slookup_to_tx_app_rsp_fifo,
      tx_app_to_slookup_check_tdest_req_fifo,
      slookup_to_tx_app_check_tdest_rsp_fifo,
      // port table req/rsp
      tx_app_to_ptable_req_fifo,
      ptable_to_tx_app_rsp_fifo,
      // state table read/write req/rsp
      tx_app_to_sttable_req_fifo,
      sttable_to_tx_app_rsp_fifo,

      // net app data request
      net_app_to_tx_app_trans_data_req,
      tx_app_to_net_app_tans_data_rsp,
      net_app_trans_data,
      // tx sar req/rsp
      tx_app_to_tx_sar_req_fifo,
      tx_sar_to_tx_app_rsp_fifo,
      // to event eng
      tx_app_to_event_eng_set_event_fifo,
      // state table
      tx_app_to_sttable_lup_req_fifo,
      sttable_to_tx_app_lup_rsp_fifo,
      // datamover req/rsp
      tx_app_to_mem_write_cmd,
      tx_app_to_mem_write_data,
      mem_to_tx_app_write_status,
      // in big endian
      my_ip_addr);

  tx_engine(
      // from ack delay to tx engine req
      ack_delay_to_tx_eng_event_fifo,
      tx_eng_read_count_fifo,
      // rx sar
      tx_eng_to_rx_sar_lup_req_fifo,
      rx_sar_to_tx_eng_lup_rsp_fifo,
      // tx sar
      tx_eng_to_tx_sar_req_fifo,
      tx_sar_to_tx_eng_rsp_fifo,
      // timer
      tx_eng_to_timer_set_rtimer_fifo,
      tx_eng_to_timer_set_ptimer_fifo,
      // to session lookup req/rsp
      tx_eng_to_slookup_rev_table_req_fifo,
      slookup_rev_table_to_tx_eng_rsp_fifo,
      // to datamover cmd
      mover_read_mem_cmd_out,
      // read data from data mem
      mover_read_mem_data_in,
#if (TCP_NODELAY)
      tx_app_to_tx_eng_data,
#endif
      // to outer
      tx_ip_pkt_out

  );

  tx_sar_table(rx_eng_to_tx_sar_req_fifo,
               tx_sar_to_rx_eng_rsp_fifo,
               tx_eng_to_tx_sar_req_fifo,
               tx_sar_to_tx_eng_rsp_fifo,
               tx_app_to_tx_sar_req_fifo,
               tx_sar_to_tx_app_rsp_fifo);
}
