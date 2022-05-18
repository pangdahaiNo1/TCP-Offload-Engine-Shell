#include "echo_server.hpp"

void OpenPortHandler(stream<NetAXISListenPortReq> &net_app_listen_port_req,
                     stream<NetAXISListenPortRsp> &net_app_listen_port_rsp) {
  // #pragma HLS PIPELINE II = 1
  // #pragma HLS INLINE   off

  enum OpenPortFsmState { OPEN_PORT, WAIT_RESPONSE, IDLE };
  static OpenPortFsmState fsm_state = OPEN_PORT;
#pragma HLS RESET variable = fsm_state

  NetAXISListenPortReq net_listen_req;
  NetAXISListenPortRsp net_listen_rsp;

  static ap_uint<16> listen_port;
  listen_port = 15000;

  switch (fsm_state) {
    /* Open/Listen on Port at start-up */
    case OPEN_PORT:
      net_listen_req.data = listen_port;
      net_listen_req.dest = kTDEST;
      net_app_listen_port_req.write(net_listen_req);
      fsm_state = WAIT_RESPONSE;
      break;

    /* Check if listening on Port was successful, otherwise try again*/
    case WAIT_RESPONSE:
      if (!net_app_listen_port_rsp.empty()) {
        net_listen_rsp = net_app_listen_port_rsp.read();
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
    stream<NetAXISNewClientNotificaion> &net_app_new_client_notification,
    // rx app notify
    stream<NetAXISAppNotification> &net_app_notification,
    // read data req/rsp
    stream<NetAXISAppReadReq> &net_app_read_data_req,
    stream<NetAXISAppReadRsp> &net_app_read_data_rsp,
    // data in/out
    stream<NetAXIS> &net_app_rx_data_in,
    stream<NetAXIS> &net_app_rx_data_out,
    // echo server meta
    stream<EchoServerMeta> &echo_server_meta) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  // Reads new data from memory and writes it into fifo
  // Read & write metadata only once per package
  enum EchoServerRxFsmState { GET_DATA_NOTY, WAIT_SESSION_ID, FORWARD_DATA };
  static EchoServerRxFsmState   fsm_state = GET_DATA_NOTY;
  static NetAXISAppNotification app_notification;
  EchoServerMeta                meta_data;

  NetAXISAppReadRsp           read_data_rsp;
  NetAXIS                     cur_word;
  NetAXISNewClientNotificaion new_client_notification;

  static int noty_num = 0;  // deleteme

  if (!net_app_new_client_notification.empty()) {
    net_app_new_client_notification.read(new_client_notification);
#ifndef __SYNTHESIS__
    cout << new_client_notification.to_string() << endl;
#endif
  }

  switch (fsm_state) {
    /* Receive rxAppNotification, about new data which is available	*/
    case GET_DATA_NOTY:
      if (!net_app_notification.empty()) {
        net_app_notification.read(app_notification);
        if (app_notification.data.length != 0) {
          net_app_read_data_req.write(NetAXISAppReadReq(
              AppReadReq(app_notification.data.session_id, app_notification.data.length), kTDEST));
          fsm_state = WAIT_SESSION_ID;
        }
      }
      break;
    case WAIT_SESSION_ID:
      if (!net_app_read_data_rsp.empty()) {
        net_app_read_data_rsp.read(read_data_rsp);

        meta_data.session_id = app_notification.data.session_id;
        meta_data.length     = app_notification.data.length;
        echo_server_meta.write(meta_data);
        noty_num++;
        fsm_state = FORWARD_DATA;
      }
      break;
    case FORWARD_DATA:
      if (!net_app_rx_data_in.empty()) {
        net_app_rx_data_in.read(cur_word);
        net_app_rx_data_out.write(cur_word);
        if (cur_word.last) {
          fsm_state = GET_DATA_NOTY;
        }
      }
      break;
  }
}

void                 EchoServerTransmitData(stream<EchoServerMeta> &        echo_server_meta,
                                            stream<NetAXISAppTransDataReq> &net_app_trans_data_req,
                                            stream<NetAXISAppTransDataRsp> &net_app_trans_data_rsp,
                                            stream<NetAXIS> &               net_app_tx_data_in,
                                            stream<NetAXIS> &               net_app_tx_data_out) {
#pragma HLS INLINE   off
#pragma HLS PIPELINE II = 1

  enum EchoServerTxFsmState { READ_META, WAIT_RESPONSE, FORWARD_DATA, WAIT_CYCLES };
  static EchoServerTxFsmState fsm_state = READ_META;
  static ap_uint<8>           cycle_counter;

  static EchoServerMeta  meta_data;
  NetAXISAppTransDataRsp trans_data_rsp;
  NetAXIS                cur_word;
  static int             response_counter = 0;  // deleteme

  switch (fsm_state) {
    case READ_META:
      if (!echo_server_meta.empty()) {
        echo_server_meta.read(meta_data);
        net_app_trans_data_req.write(
            NetAXISAppTransDataReq(meta_data.length, meta_data.session_id, kTDEST));
        fsm_state = WAIT_RESPONSE;
      }
      break;
    case WAIT_RESPONSE:
      if (!net_app_trans_data_rsp.empty()) {
        net_app_trans_data_rsp.read(trans_data_rsp);
        if (trans_data_rsp.data.error != 0) {
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
        net_app_tx_data_in.read(cur_word);
        cur_word.dest = kTDEST;
        net_app_tx_data_out.write(cur_word);
        if (cur_word.last) {
          fsm_state = READ_META;
        }
      }
      break;
    case WAIT_CYCLES:
      cycle_counter++;
      if (cycle_counter == 100) {
        // issue writing command again and wait for response
        net_app_trans_data_req.write(
            NetAXISAppTransDataReq(meta_data.length, meta_data.session_id, kTDEST));
        fsm_state = WAIT_RESPONSE;
      }
      break;
  }
}

void                 EchoServerDummy(stream<NetAXISAppOpenConnReq> & net_app_open_conn_req,
                                     stream<NetAXISAppOpenConnRsp> & net_app_open_conn_rsp,
                                     stream<NetAXISAppCloseConnReq> &net_app_close_conn_req) {
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
    open_conn_req.dest          = kTDEST;
    net_app_open_conn_req.write(open_conn_req);
    if (open_conn_rsp.data.success) {
      net_app_close_conn_req.write(NetAXISAppCloseConnReq(open_conn_rsp.data.session_id, kTDEST));
      // closePort.write(tuple.ip_port);
    }
  }
}

void echo_server(
    // listen port
    stream<NetAXISListenPortReq> &net_app_listen_port_req,
    stream<NetAXISListenPortRsp> &net_app_listen_port_rsp,
    // rx client notify
    stream<NetAXISNewClientNotificaion> &net_app_new_client_notification,
    // rx app notify
    stream<NetAXISAppNotification> &net_app_notification,
    // read data req/rsp
    stream<NetAXISAppReadReq> &net_app_read_data_req,
    stream<NetAXISAppReadRsp> &net_app_read_data_rsp,
    // open/close conn
    stream<NetAXISAppOpenConnReq> & net_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp> & net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_close_conn_req,
    // read data
    stream<NetAXIS> &net_app_rx_data_in,
    // transmit data
    stream<NetAXISAppTransDataReq> &net_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &net_app_trans_data_rsp,
    stream<NetAXIS> &               net_app_tx_data_out) {
//#pragma HLS                        DATAFLOW
#pragma HLS INTERFACE ap_ctrl_none port = return

#pragma HLS INTERFACE axis register both port = net_app_listen_port_req name =                     \
    m_net_app_listen_port_req
#pragma HLS INTERFACE axis register both port = net_app_listen_port_rsp
#pragma HLS INTERFACE axis register both port = net_app_new_client_notification name =             \
    s_net_app_new_client_notification
#pragma HLS INTERFACE axis register both port = net_app_notification name = s_net_app_notification
#pragma HLS INTERFACE axis register both port = net_app_read_data_req name = m_net_app_read_data_req
#pragma HLS INTERFACE axis register both port = net_app_read_data_rsp name = s_net_app_read_data_rsp
#pragma HLS INTERFACE axis register both port = net_app_open_conn_req name = m_net_app_open_conn_req
#pragma HLS INTERFACE axis register both port = net_app_open_conn_rsp name = s_net_app_open_conn_rsp
#pragma HLS INTERFACE axis register both port = net_app_close_conn_req name =                      \
    m_net_app_close_conn_req
#pragma HLS INTERFACE axis register both port = net_app_rx_data_in name = s_net_app_rx_data_in
#pragma HLS INTERFACE axis register both port = net_app_trans_data_req name =                      \
    m_net_app_trans_data_req
#pragma HLS INTERFACE axis register both port = net_app_trans_data_rsp name =                      \
    s_net_app_trans_data_rsp
#pragma HLS INTERFACE axis register both port = net_app_tx_data_out name = m_net_app_tx_data_out

#pragma HLS DATA_PACK variable = net_app_listen_port_req
#pragma HLS DATA_PACK variable = net_app_listen_port_rsp
#pragma HLS DATA_PACK variable = net_app_new_client_notification
#pragma HLS DATA_PACK variable = net_app_notification
#pragma HLS DATA_PACK variable = net_app_read_data_req
#pragma HLS DATA_PACK variable = net_app_read_data_rsp
#pragma HLS DATA_PACK variable = net_app_open_conn_req
#pragma HLS DATA_PACK variable = net_app_open_conn_rsp
#pragma HLS DATA_PACK variable = net_app_close_conn_req
#pragma HLS DATA_PACK variable = net_app_rx_data_in
#pragma HLS DATA_PACK variable = net_app_trans_data_req
#pragma HLS DATA_PACK variable = net_app_trans_data_rsp
#pragma HLS DATA_PACK variable = net_app_tx_data_out

  static stream<EchoServerMeta> echo_server_meta_fifo("echo_server_meta_fifo");
#pragma HLS stream variable = echo_server_meta_fifo depth    = 128
#pragma HLS DATA_PACK                               variable = echo_server_meta_fifo

  static stream<NetAXIS> net_app_rxtx_data_fifo("net_app_rxtx_data_fifo");
#pragma HLS stream variable = net_app_rxtx_data_fifo depth    = 512
#pragma HLS DATA_PACK                                variable = net_app_rxtx_data_fifo

  OpenPortHandler(net_app_listen_port_req, net_app_listen_port_rsp);
  EchoServerReceiveData(net_app_new_client_notification,
                        net_app_notification,
                        net_app_read_data_req,
                        net_app_read_data_rsp,
                        net_app_rx_data_in,
                        net_app_rxtx_data_fifo,
                        echo_server_meta_fifo

  );

  EchoServerTransmitData(echo_server_meta_fifo,
                         net_app_trans_data_req,
                         net_app_trans_data_rsp,
                         net_app_rxtx_data_fifo,
                         net_app_tx_data_out);
  EchoServerDummy(net_app_open_conn_req, net_app_open_conn_rsp, net_app_close_conn_req);
}