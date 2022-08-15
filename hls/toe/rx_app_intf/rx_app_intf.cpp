#include "rx_app_intf.hpp"

#include "toe/memory_access/memory_access.hpp"
using namespace hls;
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/** @ingroup rx_app_intf
 *  This application interface is used to open passive connections
 *  @param[in]		appListeningIn
 *  @param[in]		appStopListeningIn
 *  @param[in]		rxAppPorTableListenIn
 *  @param[in]		rxAppPortTableCloseIn
 *  @param[out]		appListeningOut
 *  @param[out]		rxAppPorTableListenOut
 */
// lock step for the multi role listening request
void RxAppPortHandler(stream<NetAXISListenPortReq> &net_app_to_rx_app_listen_port_req,
                      stream<NetAXISListenPortRsp> &rx_app_to_net_app_listen_port_rsp,
                      stream<ListenPortReq>        &rx_app_to_ptable_listen_port_req,
                      stream<ListenPortRsp>        &ptable_to_rx_app_listen_port_rsp) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

#pragma HLS INTERFACE axis register both port = net_app_to_rx_app_listen_port_req
#pragma HLS INTERFACE axis register both port = rx_app_to_net_app_listen_port_rsp

  enum RxAppPortFsmState { WAIT_NET, WAIT_PTABLE };
  static RxAppPortFsmState fsm_state = WAIT_NET;
  ListenPortReq            listen_req;
  ListenPortRsp            listen_rsp;
  switch (fsm_state) {
    case WAIT_NET:
      if (!net_app_to_rx_app_listen_port_req.empty()) {
        listen_req = net_app_to_rx_app_listen_port_req.read();
        rx_app_to_ptable_listen_port_req.write(listen_req);
        logger.Info(RX_APP_IF, PORT_TBLE, "ListenPort Req", listen_req.to_string());
        fsm_state = WAIT_PTABLE;
      }
      break;
    case WAIT_PTABLE:
      if (!ptable_to_rx_app_listen_port_rsp.empty()) {
        listen_rsp = ptable_to_rx_app_listen_port_rsp.read();
        rx_app_to_net_app_listen_port_rsp.write(listen_rsp.to_net_axis());
        logger.Info(RX_APP_IF, NET_APP, "ListenPort Rsp", listen_rsp.to_string());
        fsm_state = WAIT_NET;
      }
      break;
  }
}

/** @ingroup rx_app_stream_if
 *  This application interface is used to receive data streams of established connections.
 *  The Application polls data from the buffer by sending a readRequest. The module checks
 *  if the readRequest is valid then it sends a read request to the memory. After processing
 *  the request the MetaData containig the Session-ID is also written back.
 *  @param[in]		net_app_to_rx_app_recv_data_req
 *  @param[in]		rx_sar_to_rx_app_rsp
 *  @param[out]		rx_app_to_net_app_recv_data_rsp
 *  @param[out]		rx_app_to_rx_sar_req
 *  @param[out]		rx_buffer_read_cmd
 */
void RxAppDataHandler(stream<NetAXISAppReadReq> &net_app_to_rx_app_recv_data_req,
                      stream<NetAXISAppReadRsp> &rx_app_to_net_app_recv_data_rsp,
                      stream<RxSarAppReqRsp>    &rx_app_to_rx_sar_req,
                      stream<RxSarAppReqRsp>    &rx_sar_to_rx_app_rsp,
#if !(TCP_RX_DDR_BYPASS)
                      // inner read mem cmd
                      stream<MemBufferRWCmd> &rx_app_to_mem_read_cmd,
                      // read mem payload
                      stream<NetAXISWord> &mover_to_rx_app_read_data,
#else
                      // rx engine data to net app
                      stream<NetAXISWord> &rx_eng_to_rx_app_data,
#endif
                      stream<NetAXIS> &net_app_recv_data) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

#pragma HLS INTERFACE axis register both port = net_app_to_rx_app_recv_data_req
#pragma HLS INTERFACE axis register both port = rx_app_to_net_app_recv_data_rsp
#pragma HLS INTERFACE axis register both port = net_app_recv_data

  static ap_uint<16> rx_app_read_length;
  static NetAXISDest rx_app_role_id;
  enum RxAppDataDataFsmState { WAIT_NET_APP_DATA_REQ, WAIT_SAR_RSP, WAIT_DATA };
  static RxAppDataDataFsmState fsm_state = WAIT_NET_APP_DATA_REQ;
  // add a lock for record role_id
  static bool    net_app_data_lock = false;
  NetAXISWord    cur_word;
  MemBufferRWCmd to_mem_cmd;
  ap_uint<32>    payload_mem_addr = 0;

  switch (fsm_state) {
    case WAIT_NET_APP_DATA_REQ:
      if (!net_app_to_rx_app_recv_data_req.empty()) {
        AppReadReq app_read_request = net_app_to_rx_app_recv_data_req.read();
        logger.Info(NET_APP, RX_APP_IF, "ReadData Req", app_read_request.to_string(), false);
        // Make sure length is not 0, otherwise Data Mover will hang up
        if (app_read_request.data.read_length != 0) {
          // record the TDEST
          rx_app_role_id = app_read_request.dest;
          // Get app pointer
          rx_app_to_rx_sar_req.write(RxSarAppReqRsp(app_read_request.data.session_id));
          logger.Info(RX_APP_IF,
                      RX_SAR_TB,
                      "SessionSAR Lup Req",
                      app_read_request.data.session_id.to_string(16),
                      false);
          rx_app_read_length = app_read_request.data.read_length;
          fsm_state          = WAIT_SAR_RSP;
          net_app_data_lock  = true;
        }
      }
      break;
    case WAIT_SAR_RSP:
      if (!rx_sar_to_rx_app_rsp.empty()) {
        RxSarAppReqRsp    rx_app_rsp = rx_sar_to_rx_app_rsp.read();
        NetAXISAppReadRsp net_app_rsp;
        net_app_rsp.data = rx_app_rsp.session_id;
        net_app_rsp.dest = rx_app_role_id;
        rx_app_to_net_app_recv_data_rsp.write(net_app_rsp);
        // Update app read pointer
        RxSarAppReqRsp rx_sar_upd_req(rx_app_rsp.session_id,
                                      rx_app_rsp.app_read + rx_app_read_length);
        rx_app_to_rx_sar_req.write(rx_sar_upd_req);
        logger.Info(RX_APP_IF, RX_SAR_TB, "SessionSAR Upd Req", rx_sar_upd_req.to_string());
#if !(TCP_RX_DDR_BYPASS)
        // inner read mem cmd
        GetSessionMemAddr<1>(rx_app_rsp.session_id, rx_app_rsp.app_read, payload_mem_addr);
        to_mem_cmd = MemBufferRWCmd(payload_mem_addr, rx_app_read_length);
        logger.Info(RX_APP_IF, DATA_MVER, "Rx read cmd", to_mem_cmd.to_string());

        rx_app_to_mem_read_cmd.write(to_mem_cmd);
#endif
        fsm_state = WAIT_DATA;
      }
      break;
#if !(TCP_RX_DDR_BYPASS)
    case WAIT_DATA:
      if (!mover_to_rx_app_read_data.empty()) {
        mover_to_rx_app_read_data.read(cur_word);
        cur_word.dest = rx_app_role_id;  // TDEST should be assigned in Rx app intf
        net_app_recv_data.write(cur_word.to_net_axis());
        logger.Info(RX_APP_IF, NET_APP, "Rx App Data from Mem", cur_word.to_string());
        if (cur_word.last) {
          fsm_state         = WAIT_NET_APP_DATA_REQ;
          net_app_data_lock = false;
        }
      }
      break;
#else
    case WAIT_DATA:
      if (!rx_eng_to_rx_app_data.empty()) {
        rx_eng_to_rx_app_data.read(cur_word);
        // cur_word.dest = rx_app_role_id;  // assume the TDEST is assigned in Rx engine
        net_app_recv_data.write(cur_word.to_net_axis());
        logger.Info(RX_APP_IF, NET_APP, "Rx App Data from rx eng", cur_word.to_string());
        if (cur_word.last) {
          fsm_state         = WAIT_NET_APP_DATA_REQ;
          net_app_data_lock = false;
        }
      }
      break;
#endif
  }
}

void NetAppNotificationTdestHandler(stream<AppNotificationNoTDEST> &app_notification_no_tdest,
                                    stream<NetAXISAppNotification> &net_app_notification,
                                    stream<TcpSessionID>           &slookup_tdest_lookup_req,
                                    stream<NetAXISDest>            &slookup_tdest_lookup_rsp) {
#pragma HLS PIPELINE                     II = 1
#pragma HLS INLINE                       off
#pragma HLS INTERFACE axis register both port = net_app_notification
  static bool                   tdest_req_lock = false;
  static AppNotificationNoTDEST notify_reg;

  if (!app_notification_no_tdest.empty() && !tdest_req_lock) {
    notify_reg = app_notification_no_tdest.read();
    logger.Info(MISC_MDLE, RX_APP_IF, "AppNotify", notify_reg.to_string(), false);
    slookup_tdest_lookup_req.write(notify_reg.session_id);
    tdest_req_lock = true;
  } else if (!slookup_tdest_lookup_rsp.empty() && tdest_req_lock) {
    NetAXISDest     temp_role_id = slookup_tdest_lookup_rsp.read();
    AppNotification net_app_notify;
    net_app_notify.data = notify_reg;
    net_app_notify.dest = temp_role_id;
    logger.Info(RX_APP_IF, NET_APP, "NetAppNotify", net_app_notify.to_string(), false);
    net_app_notification.write(net_app_notify.to_net_axis());
    tdest_req_lock = false;
  }
}

void rx_app_intf(
    // net role - port handler
    stream<NetAXISListenPortReq> &net_app_to_rx_app_listen_port_req,
    stream<NetAXISListenPortRsp> &rx_app_to_net_app_listen_port_rsp,
    // rxapp - port table
    stream<ListenPortReq> &rx_app_to_ptable_listen_port_req,
    stream<ListenPortRsp> &ptable_to_rx_app_listen_port_rsp,
    // not role - data handler
    stream<NetAXISAppReadReq> &net_app_to_rx_app_recv_data_req,
    stream<NetAXISAppReadRsp> &rx_app_to_net_app_recv_data_rsp,
    // rx sar req/rsp
    stream<RxSarAppReqRsp> &rx_app_to_rx_sar_req,
    stream<RxSarAppReqRsp> &rx_sar_to_rx_app_rsp,
#if !(TCP_RX_DDR_BYPASS)
    // data from mem to net app
    stream<DataMoverCmd> &rx_app_to_mover_read_cmd,
    stream<NetAXIS>      &mover_to_rx_app_read_data,
#else
    // data from rx engine to net app
    stream<NetAXISWord> &rx_eng_to_rx_app_data,
#endif
    stream<NetAXIS> &net_app_recv_data,

    // net role app - notification
    // Rx engine to Rx app
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification,
    // Timer to Rx app
    stream<AppNotificationNoTDEST> &rtimer_to_rx_app_notification,
    // lookup for tdest, req/rsp connect to slookup controller
    stream<TcpSessionID> &rx_app_to_slookup_tdest_lookup_req,
    stream<NetAXISDest>  &slookup_to_rx_app_tdest_lookup_rsp,
    // appnotifacation to net app with TDEST
    stream<NetAXISAppNotification> &net_app_notification) {
// #pragma HLS PIPELINE II = 1
#pragma HLS DATAFLOW

#pragma HLS INTERFACE axis register both port = net_app_to_rx_app_listen_port_req
#pragma HLS INTERFACE axis register both port = rx_app_to_net_app_listen_port_rsp
#pragma HLS INTERFACE axis register both port = net_app_to_rx_app_recv_data_req
#pragma HLS INTERFACE axis register both port = rx_app_to_net_app_recv_data_rsp
#pragma HLS INTERFACE axis register both port = net_app_recv_data
#pragma HLS INTERFACE axis register both port = net_app_notification

  RxAppPortHandler(net_app_to_rx_app_listen_port_req,
                   rx_app_to_net_app_listen_port_rsp,
                   rx_app_to_ptable_listen_port_req,
                   ptable_to_rx_app_listen_port_rsp);
#if !(TCP_RX_DDR_BYPASS)

// interface
#pragma HLS INTERFACE axis register both port = rx_app_to_mover_read_cmd
#pragma HLS INTERFACE axis register both port = mover_to_rx_app_read_data

  static stream<MemBufferRWCmd> rx_app_to_mem_read_cmd_fifo("rx_app_to_mem_read_cmd_fifo");
#pragma HLS aggregate variable = rx_app_to_mem_read_cmd_fifo compact = bit
#pragma HLS stream variable = rx_app_to_mem_read_cmd_fifo depth = 16

  static stream<MemBufferRWCmdDoubleAccess> rx_app_mem_double_access_fifo(
      "rx_app_mem_double_access_fifo");
#pragma HLS stream variable = rx_app_mem_double_access_fifo depth = 16
#pragma HLS aggregate variable = rx_app_mem_double_access_fifo compact = bit

  static stream<NetAXISWord> mem_to_rx_app_read_data_fifo("mem_to_rx_app_read_data_fifo");
#pragma HLS aggregate variable = mem_to_rx_app_read_data_fifo compact = bit
#pragma HLS stream variable = mem_to_rx_app_read_data_fifo depth = 256

  RxAppDataHandler(net_app_to_rx_app_recv_data_req,
                   rx_app_to_net_app_recv_data_rsp,
                   rx_app_to_rx_sar_req,
                   rx_sar_to_rx_app_rsp,
                   rx_app_to_mem_read_cmd_fifo,
                   mem_to_rx_app_read_data_fifo,
                   net_app_recv_data);
  ReadDataSendCmd<1>(
      rx_app_to_mem_read_cmd_fifo, rx_app_to_mover_read_cmd, rx_app_mem_double_access_fifo);
  ReadDataFromMem<1>(
      mover_to_rx_app_read_data, rx_app_mem_double_access_fifo, mem_to_rx_app_read_data_fifo);

#else

  RxAppDataHandler(net_app_to_rx_app_recv_data_req,
                   rx_app_to_net_app_recv_data_rsp,
                   rx_app_to_rx_sar_req,
                   rx_sar_to_rx_app_rsp,
                   rx_eng_to_rx_app_data,
                   net_app_recv_data);
#endif
  // to be merged in Slookup controller for looking TDEST
  static stream<AppNotificationNoTDEST> rx_app_notification_no_tdest(
      "rx_app_notification_no_tdest");
#pragma HLS STREAM variable = rx_app_notification_no_tdest depth = 16
#pragma HLS aggregate variable = rx_app_notification_no_tdest compact = bit
  AxiStreamMerger(
      rx_eng_to_rx_app_notification, rtimer_to_rx_app_notification, rx_app_notification_no_tdest);

  NetAppNotificationTdestHandler(rx_app_notification_no_tdest,
                                 net_app_notification,
                                 rx_app_to_slookup_tdest_lookup_req,
                                 slookup_to_rx_app_tdest_lookup_rsp);
}