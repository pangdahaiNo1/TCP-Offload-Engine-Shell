#include "iperf2.hpp"
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
  static ap_uint<16> connection_id = 0;
  listen_port                      = 5011;

  switch (fsm_state) {
    // Open/Listen on Port at start-up
    case OPEN_PORT:
      net_listen_req.data = listen_port + connection_id;
      net_listen_req.dest = tdest_const;
      net_app_listen_port_req.write(net_listen_req.to_net_axis());
      logger.Info(NET_APP, TOE_TOP, "ListenPort Req", net_listen_req.to_string());
      fsm_state = WAIT_RESPONSE;
      break;

    // Check if listening on Port was successful, otherwise try again
    case WAIT_RESPONSE:
      if (!net_app_listen_port_rsp.empty()) {
        net_listen_rsp = net_app_listen_port_rsp.read();
        logger.Info(TOE_TOP, NET_APP, "ListenPort Rsp", net_listen_rsp.to_string());
        if (net_listen_rsp.data.port_number == listen_port) {
          if (net_listen_rsp.data.open_successfully || net_listen_rsp.data.already_open) {
            if (connection_id == TCP_MAX_SESSIONS - 1) {
              fsm_state = IDLE;
            } else {
              connection_id++;
              fsm_state = OPEN_PORT;
            }
          } else {
            // If the port was not opened successfully try again
            fsm_state = OPEN_PORT;
          }
        }
      }
      break;
    case IDLE:
      // Stay here forever
      fsm_state = IDLE;
      break;
  }
}

// consume received data, but not transfer out data
void IperfServer(
    // new client notify
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    // rx app notify
    stream<NetAXISAppNotification> &net_app_notification,
    // read data req/rsp
    stream<NetAXISAppReadReq> &net_app_recv_data_req,
    stream<NetAXISAppReadRsp> &net_app_recv_data_rsp,
    // data in/out
    stream<NetAXIS> &net_app_recv_data,
    NetAXISDest     &tdest_const) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  // Reads new data from memory and writes it into fifo
  enum EchoServerRxFsmState { GET_DATA_NOTY, WAIT_SESSION_ID, CONSUME };
  static EchoServerRxFsmState fsm_state = GET_DATA_NOTY;
  static AppNotification      app_notification;
  AppReadReq                  read_data_req;
  NetAXISWord                 cur_word;
  NewClientNotification       new_client_notification;

  if (!net_app_new_client_notification.empty()) {
    new_client_notification = net_app_new_client_notification.read();
    logger.Info(TOE_TOP, NET_APP, "NewClient", new_client_notification.to_string());
  }

  switch (fsm_state) {
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
#ifndef __SYNTHESIS__
        AppReadRsp read_data_rsp;
        read_data_rsp = net_app_recv_data_rsp.read();
        logger.Info(TOE_TOP, NET_APP, "Read Data Rsp", read_data_rsp.to_string());
#else
        net_app_recv_data_rsp.read();
#endif
        fsm_state = CONSUME;
      }
      break;
    case CONSUME:
      if (!net_app_recv_data.empty()) {
        cur_word = net_app_recv_data.read();
        logger.Info(NET_APP, "Recv Data", cur_word.to_string());
        if (cur_word.last) {
          fsm_state = GET_DATA_NOTY;
        }
      }
      break;
  }
}

void IperfClient(
    // user settings
    IperfRegs &settings_regs,
    // open remote iperf server request
    stream<NetAXISAppOpenConnReq>  &net_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp>  &net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_close_conn_req,
    // transmit data req
    stream<NetAXISAppTransDataReq> &net_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &net_app_trans_data_rsp,
    stream<NetAXIS>                &net_app_trans_data,
    NetAXISDest                    &tdest_const,

    // stop watch timer
    stream<ap_uint<64> > &stop_watch_start,
    stream<bool>         &stop_watch_end) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  enum IperfFsmState {
    WAIT_USER_START,
    INIT_CON,
    WAIT_CON,
    REQ_WRITE_FIRST,
    INIT_RUN,
    COMPUTE_NECESSARY_SPACE,
    SPACE_RESPONSE,
    SEND_PACKET,
    REQUEST_SPACE,
    CLOSE_CONN,
    CLOSE_CONN1,
    ERROR_OPENING_CONNECTION
  };

  static IperfFsmState fsm_state                 = WAIT_USER_START;
  static ap_uint<1>    error_openning_connection = 0;
  // register for record iperf settings
  static ap_uint<1>  run_experiment_reg = 1;
  static ap_uint<1>  use_timer_reg;
  static ap_uint<14> num_connections_reg;  // total connection the user want to open
  static ap_uint<32> transfer_size_reg;
  static ap_uint<16> packet_mss_reg;
  static ap_uint<32> dst_ip_reg;
  static ap_uint<16> dst_port_reg;
  // record current state
  static ap_uint<14> cur_connection_id = 0;
  // store all connections session id
  static ap_uint<16> sessions_id[TCP_MAX_SESSIONS];
#pragma HLS DEPENDENCE variable = cur_connection_id inter false
  static ap_uint<32> bytes_already_sent;
  ap_uint<32>        remaining_bytes_to_send;
  // How many bytes are necessary for the last transaction
  static ap_uint<6>  bytes_last_word;
  static ap_uint<10> transactions;
  static ap_uint<16> transaction_length;
  static ap_uint<10> word_sent_cnt;
  static ap_uint<16> wait_close_conn_counter;
  static ap_uint<64> last_transfer_keep;

  // stop watch timer
  static ap_uint<1> stop_watch_end_reg = 0;
  // open connection
  AppOpenConnRsp open_conn_rsp;
  AppOpenConnReq open_conn_req;
  // close connection
  NetAXISAppCloseConnReq close_conn_req;
  // tansmit req
  AppTransDataReq trans_data_req;
  AppTransDataRsp trans_data_rsp;
  // transmit data
  NetAXISWord cur_word;

  // write control regs
  settings_regs.max_connections           = TCP_MAX_SESSIONS;
  settings_regs.current_state             = fsm_state;
  settings_regs.error_openning_connection = error_openning_connection;

  switch (fsm_state) {
      // Wait until user star the client. Once the starting is detect all settings
      // are registered
    case WAIT_USER_START:
      if (settings_regs.run_experiment && !run_experiment_reg) {  // Rising edge
        // Start experiment only if the user specifies a valid number of connection
        if ((settings_regs.num_connections > 0) &&
            (settings_regs.num_connections <= TCP_MAX_SESSIONS)) {
          // Register input variables
          num_connections_reg       = settings_regs.num_connections;
          transfer_size_reg         = settings_regs.transfer_size;
          use_timer_reg             = settings_regs.use_timer;
          dst_ip_reg                = settings_regs.dst_ip;
          dst_port_reg              = settings_regs.dst_port;
          packet_mss_reg            = settings_regs.packet_mss;
          error_openning_connection = 0;
          stop_watch_end_reg        = 0;
          if (settings_regs.use_timer) {
            stop_watch_start.write(settings_regs.run_time);  // Start stopwatch
          }
          fsm_state = INIT_CON;
        }
        logger.Info(TOE_TOP, NET_APP, "Launch Iperf client: ", settings_regs.to_string());
      }
      break;
    case INIT_CON:
      // Open as many connection as the user wants
      open_conn_req.data.ip_addr  = dst_ip_reg;
      open_conn_req.data.tcp_port = dst_port_reg + cur_connection_id;
      open_conn_req.dest          = tdest_const;
      net_app_open_conn_req.write(open_conn_req.to_net_axis());
      logger.Info(NET_APP, TX_APP_IF, "Open connection req", open_conn_req.to_string());
      cur_connection_id++;
      if (cur_connection_id == num_connections_reg) {
        cur_connection_id = 0;
        fsm_state         = WAIT_CON;
        if (use_timer_reg) {
          logger.Info(NET_APP, "Init iperf timer");
        }
      }
    case WAIT_CON:
      // store all sessions id for each connection
      if (!net_app_open_conn_rsp.empty()) {
        open_conn_rsp = net_app_open_conn_rsp.read();
        if (open_conn_rsp.data.success) {
          logger.Info(TX_APP_IF, NET_APP, "Open connection Resp: ", open_conn_rsp.to_string());
          sessions_id[cur_connection_id] = open_conn_rsp.data.session_id;
          cur_connection_id++;
          if (cur_connection_id == num_connections_reg) {  // maybe move outside
            cur_connection_id = 0;
            fsm_state         = REQ_WRITE_FIRST;
          }
        } else {
          fsm_state = ERROR_OPENING_CONNECTION;
          logger.Info(TX_APP_IF, NET_APP, "Open connection failed", open_conn_rsp.to_string());
        }
      }
      break;
    case ERROR_OPENING_CONNECTION:
      error_openning_connection = 1;
      fsm_state                 = WAIT_USER_START;
      break;
    case REQ_WRITE_FIRST:
      // Request space and send the first packet for each connection
      trans_data_req.id = sessions_id[cur_connection_id];
      if (use_timer_reg) {
        trans_data_req.data = 64;
      } else {
        trans_data_req.data = 24;
      }
      trans_data_req.dest = tdest_const;
      net_app_trans_data_req.write(trans_data_req.to_net_axis());
      logger.Info(NET_APP, TX_APP_IF, "Trans data req", trans_data_req.to_string());
      cur_connection_id++;
      if (stop_watch_end_reg) {
        fsm_state = CLOSE_CONN;
        logger.Info(NET_APP, "CUR->REQ_WRITE_FIRST, NEXT->CLOSE_CONN");
      } else if (cur_connection_id == num_connections_reg) {
        cur_connection_id = 0;
        fsm_state         = INIT_RUN;
        logger.Info(NET_APP, "CUR->REQ_WRITE_FIRST, NEXT->INIT_RUN");
      }
      break;

    case INIT_RUN:
      if (!net_app_trans_data_rsp.empty()) {
        trans_data_rsp = net_app_trans_data_rsp.read();
        logger.Info(NET_APP, TX_APP_IF, "Trans data resp", trans_data_rsp.to_string());
        if (trans_data_rsp.data.error == NO_ERROR) {
          if (use_timer_reg) {
            cur_word.data(63, 0)   = 0x3736353400000000;
            cur_word.data(79, 64)  = 0x3938;
            cur_word.data(511, 80) = 0;
            cur_word.keep          = 0xFFFFFFFFFFFFFFFF;
            cur_word.last          = 1;
          } else {
            if (settings_regs.dual_mode_en) {
              cur_word.data(63, 0) = 0x0100000001000080;  // run now
            } else {
              cur_word.data(63, 0) = 0x0100000000000000;
            }
            cur_word.data(127, 64)  = 0x0000000089130000;
            cur_word.data(159, 128) = 0x39383736;
            cur_word.data(191, 160) = SwapByte(transfer_size_reg);  // transfer size

            cur_word.data(511, 192) = 0;
            cur_word.keep           = 0xffffff;
            cur_word.last           = 1;
          }

          cur_connection_id++;

          if (cur_connection_id == num_connections_reg) {
            cur_connection_id  = 0;
            bytes_already_sent = 0;
            fsm_state          = COMPUTE_NECESSARY_SPACE;
          } else {
            fsm_state = REQ_WRITE_FIRST;
          }
          cur_word.dest = tdest_const;
          net_app_trans_data.write(cur_word.to_net_axis());
          logger.Info(NET_APP, "Trans data", cur_word.to_string());
        } else {
          fsm_state = REQ_WRITE_FIRST;
        }
      }
      break;
    case COMPUTE_NECESSARY_SPACE:
      trans_data_req.id = sessions_id[cur_connection_id];

      if (use_timer_reg) {         // If the timer is being used send the maximum packet
        if (stop_watch_end_reg) {  // When time is over finish
          remaining_bytes_to_send = 0;
        } else {
          remaining_bytes_to_send = packet_mss_reg;
        }
      } else {
        remaining_bytes_to_send = packet_mss_reg - bytes_already_sent;
      }

      if (remaining_bytes_to_send > 0) {
        if (remaining_bytes_to_send >= packet_mss_reg) {  // Check if we can send a packet
          bytes_last_word = packet_mss_reg(5, 0);
          if (packet_mss_reg(5, 0) == 0) {  // compute how many transactions are necessary
            transactions = packet_mss_reg(15, 6);
          } else {
            transactions = packet_mss_reg(15, 6) + 1;
          }
          trans_data_req.data = packet_mss_reg;
        } else {
          // How many bytes are necessary for the last transaction
          bytes_last_word = remaining_bytes_to_send(5, 0);
          if (remaining_bytes_to_send(5, 0) == 0) {  // compute how many transactions are necessary
            transactions = remaining_bytes_to_send(15, 6);
          } else {
            transactions = remaining_bytes_to_send(15, 6) + 1;
          }
          trans_data_req.data = remaining_bytes_to_send;
        }
        cur_connection_id   = 0;
        trans_data_req.dest = tdest_const;
        net_app_trans_data_req.write(trans_data_req.to_net_axis());
        logger.Info(NET_APP,
                    TX_APP_IF,
                    "Trans data req for COMPUTE_NECESSARY_SPACE",
                    trans_data_req.to_string());

        fsm_state = SPACE_RESPONSE;
      } else {
        cur_connection_id = 0;
        fsm_state         = CLOSE_CONN;
      }

      transaction_length = trans_data_req.data;
      word_sent_cnt      = 1;
      break;
    case SPACE_RESPONSE:
      if (!net_app_trans_data_rsp.empty()) {
        word_sent_cnt  = 1;
        trans_data_rsp = net_app_trans_data_rsp.read();
        logger.Info(TX_APP_IF, NET_APP, "Trans data resp", trans_data_rsp.to_string());

        if (trans_data_rsp.data.error == 0) {
          if (cur_connection_id != (num_connections_reg - 1)) {
            cur_connection_id++;
          }
          fsm_state          = SEND_PACKET;
          bytes_already_sent = bytes_already_sent + transaction_length;
        } else {
          wait_close_conn_counter = 0;
          fsm_state               = REQUEST_SPACE;
        }
      }
      last_transfer_keep = DataLengthToAxisKeep(bytes_last_word);
      break;
    case SEND_PACKET:

      cur_word.data(63, 0)    = 0x6d61752d6e637068;
      cur_word.data(127, 64)  = 0x202020202073652e;
      cur_word.data(191, 128) = 0x2e736d6574737973;
      cur_word.data(255, 192) = 0x2068632e7a687465;
      cur_word.data(319, 256) = word_sent_cnt;
      cur_word.data(383, 320) = word_sent_cnt + 1;
      cur_word.data(447, 384) = word_sent_cnt + 2;
      cur_word.data(511, 448) = word_sent_cnt + 3;  // Dummy data

      if (word_sent_cnt == 1 && use_timer_reg) {
        trans_data_req.id   = sessions_id[cur_connection_id];
        trans_data_req.data = transaction_length;
        trans_data_req.dest = tdest_const;
        if (!stop_watch_end_reg) {
          net_app_trans_data_req.write(trans_data_req.to_net_axis());
        }
        cur_connection_id++;
      }

      if (word_sent_cnt == transactions) {
        cur_word.keep = last_transfer_keep;
        // cur_word.keep = len2Keep(bytes_last_word);
        cur_word.last = 1;

        if (cur_connection_id == num_connections_reg) {
          cur_connection_id = 0;
        }

        if (use_timer_reg) {
          if (stop_watch_end_reg) {
            fsm_state         = CLOSE_CONN;
            cur_connection_id = 0;
          } else {
            fsm_state = SPACE_RESPONSE;
          }
        } else {
          fsm_state = COMPUTE_NECESSARY_SPACE;
        }
      } else {
        cur_word.keep = 0xFFFFFFFFFFFFFFFF;
        cur_word.last = 0;
      }
      word_sent_cnt++;
      net_app_trans_data.write(cur_word.to_net_axis());
      break;
    case REQUEST_SPACE:
      if (use_timer_reg && stop_watch_end_reg) {
        cur_connection_id = 0;
        fsm_state         = CLOSE_CONN;
      } else {
        trans_data_req.id   = sessions_id[cur_connection_id];
        trans_data_req.data = transaction_length;
        trans_data_req.dest = tdest_const;
        net_app_trans_data_req.write(trans_data_req.to_net_axis());
        fsm_state = SPACE_RESPONSE;
      }
      break;

    case CLOSE_CONN:
      if (wait_close_conn_counter == 10000) {
        fsm_state               = CLOSE_CONN1;
        wait_close_conn_counter = 0;
        cur_connection_id       = 0;
      }
      wait_close_conn_counter++;

      break;
    case CLOSE_CONN1:
      close_conn_req.data = sessions_id[cur_connection_id];
      close_conn_req.dest = tdest_const;
      net_app_close_conn_req.write(close_conn_req);
      cur_connection_id++;
      if (cur_connection_id == num_connections_reg) {
        fsm_state         = WAIT_USER_START;
        cur_connection_id = 0;
      }
      break;
  }
  run_experiment_reg = settings_regs.run_experiment;
  if (!stop_watch_end.empty()) {
    stop_watch_end.read();
    stop_watch_end_reg = 1;
  }
}
void IperfStopWatch(stream<ap_uint<64> > &stop_watch_start, stream<bool> &stop_watch_end) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  enum sw_states { WAIT_SIGNAL, RUNNING };
  static sw_states sw_fsm_state = WAIT_SIGNAL;

  static ap_uint<64> max_count;
  static ap_uint<64> counter;

  switch (sw_fsm_state) {
    case WAIT_SIGNAL:
      if (!stop_watch_start.empty()) {
        stop_watch_start.read(max_count);
        logger.Info(NET_APP, "Start Timer for counting cycles: ", max_count.to_string());
        counter      = 1;
        sw_fsm_state = RUNNING;
      }
      break;
    case RUNNING:
      if (counter == max_count) {
        stop_watch_end.write(true);
        sw_fsm_state = WAIT_SIGNAL;
        logger.Info(NET_APP, "Timer out");
      }
      counter++;
      break;
  }
}

void iperf2(
#if MULTI_IP_ADDR
    IpAddr &my_ip_addr,
#else
#endif
    // tdest constant
    NetAXISDest &tdest_const,
    // AXI Lite control register
    IperfRegs &settings_regs,
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
#pragma HLS INTERFACE ap_none register port = my_ip_addr
#else
#endif
#pragma HLS INTERFACE ap_none register port = tdest_const

#pragma HLS INTERFACE s_axilite port = settings_regs bundle = settings

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

  static stream<ap_uint<64> > stop_watch_start_fifo("stop_watch_start_fifo");
#pragma HLS STREAM variable = stop_watch_start_fifo depth = 4

  static stream<bool> stop_watch_end_fifo("stop_watch_end_fifo");
#pragma HLS STREAM variable = stop_watch_end_fifo depth = 4

  IperfClient(settings_regs,
              net_app_open_conn_req,
              net_app_open_conn_rsp,
              net_app_close_conn_req,
              net_app_trans_data_req,
              net_app_trans_data_rsp,
              net_app_trans_data,
              tdest_const,
              // timer
              stop_watch_start_fifo,
              stop_watch_end_fifo);
  IperfStopWatch(stop_watch_start_fifo, stop_watch_end_fifo);
  OpenPortHandler(net_app_listen_port_req, net_app_listen_port_rsp, tdest_const);
  IperfServer(net_app_new_client_notification,
              net_app_notification,
              net_app_recv_data_req,
              net_app_recv_data_rsp,
              net_app_recv_data,
              tdest_const);
}