#include "echo_server.hpp"
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

void                 OpenPortHandler(stream<NetAXISListenPortReq> &net_app_listen_port_req,
                                     stream<NetAXISListenPortRsp> &net_app_listen_port_rsp,
                                     NetAXISDest                  &tdest_const) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  enum OpenPortFsmState { OPEN_PORT, WAIT_RESPONSE, IDLE };
  static OpenPortFsmState fsm_state = OPEN_PORT;
#pragma HLS RESET variable = fsm_state

  ListenPortReq net_listen_req;
  ListenPortRsp net_listen_rsp;

  static ap_uint<16> listen_port;
  listen_port = 12333;

  switch (fsm_state) {
    /* Open/Listen on Port at start-up */
    case OPEN_PORT:
      net_listen_req.data = listen_port;
      net_listen_req.dest = tdest_const;
      net_app_listen_port_req.write(net_listen_req.to_net_axis());
      logger.Info(NET_APP, TOE_TOP, "ListenPort Req", net_listen_req.to_string());
      fsm_state = WAIT_RESPONSE;
      break;

    /* Check if listening on Port was successful, otherwise try again*/
    case WAIT_RESPONSE:
      if (!net_app_listen_port_rsp.empty()) {
        net_listen_rsp = net_app_listen_port_rsp.read();
        logger.Info(TOE_TOP, NET_APP, "ListenPort Rsp", net_listen_rsp.to_string());
        if (net_listen_rsp.data.port_number == listen_port) {
          if (net_listen_rsp.data.open_successfully) {
            fsm_state = IDLE;
          } else {
            fsm_state = OPEN_PORT;  // If the port was not opened successfully try again
          }
        }
      }
      break;
    case IDLE:
      /* Stay here for ever */
      fsm_state = IDLE;
      break;
  }
}

/**
 * receive data in rx path
 */
void EchoServerReceiveData(
    // new client notify
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    // rx app notify
    stream<NetAXISAppNotification> &net_app_notification,
    // read data req/rsp
    stream<NetAXISAppReadReq> &net_app_recv_data_req,
    stream<NetAXISAppReadRsp> &net_app_recv_data_rsp,
    // data in/out
    stream<NetAXIS>     &net_app_recv_data,
    stream<NetAXISWord> &net_app_rx_data_out,
    // echo server meta
    stream<EchoServerMeta> &echo_server_meta,
    NetAXISDest            &tdest_const) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  // Reads new data from memory and writes it into fifo
  // Read & write metadata only once per package
  enum EchoServerRxFsmState { GET_DATA_NOTY, WAIT_SESSION_ID, FORWARD_DATA };
  static EchoServerRxFsmState fsm_state = GET_DATA_NOTY;
  static AppNotification      app_notification;
  EchoServerMeta              meta_data;
  AppReadReq                  read_data_req;
  AppReadRsp                  read_data_rsp;
  NetAXISWord                 cur_word;
  NewClientNotification       new_client_notification;

  static int noty_num = 0;  // deleteme

  if (!net_app_new_client_notification.empty()) {
    new_client_notification = net_app_new_client_notification.read();
    logger.Info(TOE_TOP, NET_APP, "NewClient", new_client_notification.to_string());
  }

  switch (fsm_state) {
    /* Receive rxAppNotification, about new data which is available	*/
    case GET_DATA_NOTY:
      if (!net_app_notification.empty()) {
        app_notification = net_app_notification.read();
        logger.Info(TOE_TOP, NET_APP, "App Notify", read_data_req.to_string());

        if (app_notification.data.length != 0) {
          read_data_req.data =
              AppReadReqNoTEST(app_notification.data.session_id, app_notification.data.length);
          read_data_req.dest = tdest_const;
          net_app_recv_data_req.write(read_data_req.to_net_axis());
          logger.Info(NET_APP, TOE_TOP, "Read Data Req", read_data_req.to_string());
          fsm_state = WAIT_SESSION_ID;
        }
      }
      break;
    case WAIT_SESSION_ID:
      if (!net_app_recv_data_rsp.empty()) {
        read_data_rsp = net_app_recv_data_rsp.read();
        logger.Info(TOE_TOP, NET_APP, "Read Data Rsp", read_data_req.to_string());

        meta_data.session_id = app_notification.data.session_id;
        meta_data.length     = app_notification.data.length;
        echo_server_meta.write(meta_data);
        noty_num++;
        fsm_state = FORWARD_DATA;
      }
      break;
    case FORWARD_DATA:
      if (!net_app_recv_data.empty()) {
        cur_word = net_app_recv_data.read();
        net_app_rx_data_out.write(cur_word.to_net_axis());
        if (cur_word.last) {
          fsm_state = GET_DATA_NOTY;
        }
        logger.Info(NET_APP, "Recv Data", cur_word.to_string());
      }
      break;
  }
}

void                 EchoServerTransmitData(stream<EchoServerMeta>         &echo_server_meta,
                                            stream<NetAXISAppTransDataReq> &net_app_trans_data_req,
                                            stream<NetAXISAppTransDataRsp> &net_app_trans_data_rsp,
                                            stream<NetAXISWord>            &net_app_tx_data_in,
                                            stream<NetAXIS>                &net_app_trans_data,
                                            NetAXISDest                    &tdest_const) {
#pragma HLS INLINE   off
#pragma HLS PIPELINE II = 1

  enum EchoServerTxFsmState { READ_META, WAIT_RESPONSE, FORWARD_DATA, WAIT_CYCLES };
  static EchoServerTxFsmState fsm_state = READ_META;
  static ap_uint<8>           cycle_counter;

  static EchoServerMeta meta_data;
  AppTransDataReq       trans_data_req;
  AppTransDataRsp       trans_data_rsp;
  NetAXISWord           cur_word;
  static int            response_counter = 0;  // deleteme

  switch (fsm_state) {
    case READ_META:
      if (!echo_server_meta.empty()) {
        echo_server_meta.read(meta_data);
        trans_data_req = AppTransDataReq(meta_data.length, meta_data.session_id, tdest_const);
        net_app_trans_data_req.write(trans_data_req.to_net_axis());
        logger.Info(NET_APP, TOE_TOP, "TransData Req", trans_data_req.to_string());
        fsm_state = WAIT_RESPONSE;
      }
      break;
    case WAIT_RESPONSE:
      if (!net_app_trans_data_rsp.empty()) {
        trans_data_rsp = net_app_trans_data_rsp.read();
        logger.Info(TOE_TOP, NET_APP, "TransData Rsp", trans_data_rsp.to_string());

        if (trans_data_rsp.data.error != NO_ERROR) {
          cycle_counter = 0;
          fsm_state     = WAIT_CYCLES;
        } else {
          response_counter++;
          fsm_state = FORWARD_DATA;
        }
      }
      break;
    case FORWARD_DATA:
      if (!net_app_tx_data_in.empty()) {
        cur_word      = net_app_tx_data_in.read();
        cur_word.dest = tdest_const;
        net_app_trans_data.write(cur_word.to_net_axis());
        logger.Info(NET_APP, "Trans Data", cur_word.to_string());
        if (cur_word.last) {
          fsm_state = READ_META;
        }
      }
      break;
    case WAIT_CYCLES:
      cycle_counter++;
      if (cycle_counter == 100) {
        // issue writing command again and wait for response
        trans_data_req = AppTransDataReq(meta_data.length, meta_data.session_id, tdest_const);
        net_app_trans_data_req.write(trans_data_req.to_net_axis());
        logger.Info(NET_APP, TOE_TOP, "TransData Req in cycles", trans_data_req.to_string());

        fsm_state = WAIT_RESPONSE;
      }
      break;
  }
}

void                 EchoServerDummy(stream<NetAXISAppOpenConnReq>  &net_app_open_conn_req,
                                     stream<NetAXISAppOpenConnRsp>  &net_app_open_conn_rsp,
                                     stream<NetAXISAppCloseConnReq> &net_app_close_conn_req,
                                     NetAXISDest                    &tdest_const) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  NetAXISAppOpenConnRsp open_conn_rsp;
  NetAXISAppOpenConnReq open_conn_req;

  // Dummy code should never be executed, this is necessary because every
  // streams has to be written/read
  if (!net_app_open_conn_rsp.empty()) {
    net_app_open_conn_rsp.read(open_conn_rsp);
    open_conn_req.data.ip_addr  = 0xC0A80008;  // 192.168.0.8
    open_conn_req.data.tcp_port = 13330;
    open_conn_req.dest          = tdest_const;
    net_app_open_conn_req.write(open_conn_req);
    if (open_conn_rsp.data.success) {
      net_app_close_conn_req.write(
          AppCloseConnReq(open_conn_rsp.data.session_id, tdest_const).to_net_axis());
    }
  }
}

void echo_server(
#if MULTI_IP_ADDR
    IpAddr &my_ip_addr,
#else
#endif
    NetAXISDest &tdest_const,
    // listen port
    stream<NetAXISListenPortReq> &net_app_listen_port_req,
    stream<NetAXISListenPortRsp> &net_app_listen_port_rsp,
    // rx client notify
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    // rx app notify
    stream<NetAXISAppNotification> &net_app_notification,
    // read data req/rsp
    stream<NetAXISAppReadReq> &net_app_recv_data_req,
    stream<NetAXISAppReadRsp> &net_app_recv_data_rsp,
    // read data
    stream<NetAXIS> &net_app_recv_data,
    // open/close conn
    stream<NetAXISAppOpenConnReq>  &net_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp>  &net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_close_conn_req,
    // transmit data
    stream<NetAXISAppTransDataReq> &net_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &net_app_trans_data_rsp,
    stream<NetAXIS>                &net_app_trans_data) {
#pragma HLS                        DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port = return

#if MULTI_IP_ADDR
#pragma HLS INTERFACE ap_none register port = my_ip_addr name = my_ip_addr
#else
#endif
#pragma HLS stable variable = tdest_const

#pragma HLS INTERFACE axis register both port = net_app_listen_port_req
#pragma HLS aggregate variable = net_app_listen_port_req compact = bit

#pragma HLS INTERFACE axis register both port = net_app_listen_port_rsp
#pragma HLS aggregate variable = net_app_listen_port_rsp compact = bit

#pragma HLS INTERFACE axis register both port = net_app_new_client_notification
#pragma HLS aggregate variable = net_app_new_client_notification compact = bit

#pragma HLS INTERFACE axis register both port = net_app_notification
#pragma HLS aggregate variable = net_app_notification compact = bit

#pragma HLS INTERFACE axis register both port = net_app_recv_data_req
#pragma HLS aggregate variable = net_app_recv_data_req compact = bit

#pragma HLS INTERFACE axis register both port = net_app_recv_data_rsp
#pragma HLS aggregate variable = net_app_recv_data_rsp compact = bit

#pragma HLS INTERFACE axis register both port = net_app_open_conn_req
#pragma HLS aggregate variable = net_app_open_conn_req compact = bit

#pragma HLS INTERFACE axis register both port = net_app_open_conn_rsp
#pragma HLS aggregate variable = net_app_open_conn_rsp compact = bit

#pragma HLS INTERFACE axis register both port = net_app_close_conn_req
#pragma HLS aggregate variable = net_app_close_conn_req compact = bit

#pragma HLS INTERFACE axis register both port = net_app_recv_data
#pragma HLS aggregate variable = net_app_recv_data compact = bit

#pragma HLS INTERFACE axis register both port = net_app_trans_data_req
#pragma HLS aggregate variable = net_app_trans_data_req compact = bit

#pragma HLS INTERFACE axis register both port = net_app_trans_data_rsp
#pragma HLS aggregate variable = net_app_trans_data_rsp compact = bit

#pragma HLS INTERFACE axis register both port = net_app_trans_data
#pragma HLS aggregate variable = net_app_trans_data compact = bit

  static stream<EchoServerMeta, 128> echo_server_meta_fifo("echo_server_meta_fifo");
#pragma HLS aggregate variable = echo_server_meta_fifo compact = bit

  static stream<NetAXISWord, 512> net_app_rxtx_data_fifo("net_app_rxtx_data_fifo");
#pragma HLS stream variable = net_app_rxtx_data_fifo depth = 512
#pragma HLS aggregate variable = net_app_rxtx_data_fifo compact = bit

  OpenPortHandler(net_app_listen_port_req, net_app_listen_port_rsp, tdest_const);
  EchoServerReceiveData(net_app_new_client_notification,
                        net_app_notification,
                        net_app_recv_data_req,
                        net_app_recv_data_rsp,
                        net_app_recv_data,
                        net_app_rxtx_data_fifo,
                        echo_server_meta_fifo,
                        tdest_const);

  EchoServerTransmitData(echo_server_meta_fifo,
                         net_app_trans_data_req,
                         net_app_trans_data_rsp,
                         net_app_rxtx_data_fifo,
                         net_app_trans_data,
                         tdest_const);

  EchoServerDummy(
      net_app_open_conn_req, net_app_open_conn_rsp, net_app_close_conn_req, tdest_const);
}