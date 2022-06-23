#include "tx_app_intf.hpp"
using namespace hls;
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/**
 * NOT_USED: if TCP_NODELAY, the @p tx_app_to_event_eng_cache will be sent to event engine directly.
 * if not TCP_NODELAY, the @p tx_app_to_event_eng_cache will be sent to Tx APP Sts Handler
 */
void                 TxAppEventMerger(stream<Event> &tx_app_conn_handler_to_event_engine,
                                      stream<Event> &tx_app_data_handler_to_event_engine,
                                      stream<Event> &tx_app_to_event_eng_cache) {
#pragma HLS PIPELINE II = 1

  Event ev;
  // Merge Events
  if (!tx_app_conn_handler_to_event_engine.empty()) {
    tx_app_conn_handler_to_event_engine.read(ev);
    tx_app_to_event_eng_cache.write(ev);
  } else if (!tx_app_data_handler_to_event_engine.empty()) {
    tx_app_data_handler_to_event_engine.read(ev);
    tx_app_to_event_eng_cache.write(ev);
  }
}

/** @ingroup tx_app_if
 *  This interface exposes the creation and tear down of connetions to the
 * application. The IP tuple for a new connection is read from @p net_app_to_tx_app_open_conn_req ,
 * the interface then requests a free port number from the @ref port_table and fires a SYN event.
 * Once the connetion is established it notifies the application through @p appOpenConOut and
 * delivers the Session-ID belonging to the new connection. If opening of the connection is not
 * successful this is also indicated through the @p appOpenConOut. By sending the Session-ID through
 * @p closeConIn the application can initiate the teardown of the connection.
 */
void TxAppConnectionHandler(
    // net app
    stream<NetAXISAppOpenConnReq> & net_app_to_tx_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp> & tx_app_to_net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_to_tx_app_close_conn_req,
    // rx eng -> net app
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    // rx eng
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // retrans timer
    stream<OpenConnRspNoTDEST> &rtimer_to_tx_app_notification,
    // session lookup, also for TDEST
    stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
    stream<SessionLookupRsp> & slookup_to_tx_app_rsp,
    stream<TcpSessionID> &     tx_app_to_slookup_check_tdest_req,
    stream<NetAXISDest> &      slookup_to_tx_app_check_tdest_rsp,
    // port table req/rsp
    stream<NetAXISDest> &  tx_app_to_ptable_req,
    stream<TcpPortNumber> &ptable_to_tx_app_rsp,
    // state table read/write req/rsp
    stream<StateTableReq> &tx_app_to_sttable_req,
    stream<SessionState> & sttable_to_tx_app_rsp,
    // event engine
    stream<Event> &tx_app_conn_handler_to_event_engine,
    IpAddr &       my_ip_addr) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_open_conn_req
#pragma HLS INTERFACE axis register both port = tx_app_to_net_app_open_conn_rsp
#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_close_conn_req
#pragma HLS INTERFACE axis register both port = net_app_new_client_notification

  static AppOpenConnReq app_open_req_reg;
  static bool           tx_app_wait_for_free_port_lock = false;
  TxAppToSlookupReq     to_slup_req;
  // get free port number from port table, request to ptable for recording the TDSET
  if (!net_app_to_tx_app_open_conn_req.empty() && !tx_app_wait_for_free_port_lock) {
    app_open_req_reg = net_app_to_tx_app_open_conn_req.read();
    logger.Info(NET_APP, TX_APP_IF, "Open Conn Req", app_open_req_reg.to_string());
    tx_app_to_ptable_req.write(app_open_req_reg.dest);
    logger.Info(TX_APP_IF, PORT_TBLE, "FreePort Req", app_open_req_reg.dest.to_string(16));
    tx_app_wait_for_free_port_lock = true;
  } else if (!ptable_to_tx_app_rsp.empty() && tx_app_wait_for_free_port_lock) {
    TcpPortNumber free_port = ptable_to_tx_app_rsp.read();
    logger.Info(PORT_TBLE, TX_APP_IF, "FreePort Rsp LE", free_port.to_string(16));
    // generate a session id, and record the TDEST
    to_slup_req = TxAppToSlookupReq(FourTuple(my_ip_addr,
                                              app_open_req_reg.data.ip_addr,
                                              SwapByte<16>(free_port),
                                              app_open_req_reg.data.tcp_port),
                                    app_open_req_reg.dest);
    tx_app_to_slookup_req.write(to_slup_req);
    logger.Info(TX_APP_IF, SLUP_CTRL, "Session Create", to_slup_req.to_string());
    tx_app_wait_for_free_port_lock = false;
  }

  enum TxAppConnFsmState {
    IDLE,
    CLOSE_CONN,
    WAIT_FOR_TDEST_ACTIVE_OPEN,
    WAIT_FOR_TDEST_PASSIVE_OPEN
  };
  static TxAppConnFsmState fsm_state = IDLE;

  SessionLookupRsp                    slookup_rsp;
  SessionState                        sttable_rsp;
  static NewClientNotificationNoTDEST passive_open_session_reg;
  static OpenConnRspNoTDEST           open_session_reg;
  static AppCloseConnReq              app_close_session_reg;
  Event                               to_event_eng_event;
  StateTableReq                       to_sttable_req;
  AppOpenConnRsp                      to_net_app_open_conn_rsp;
  NewClientNotification               to_net_app_new_client_notify;
  // handle net app open/close connection request
  switch (fsm_state) {
    case IDLE:
      if (!slookup_to_tx_app_rsp.empty()) {
        // get the session state
        slookup_to_tx_app_rsp.read(slookup_rsp);
        logger.Info(SLUP_CTRL, TX_APP_IF, "Session Rsp", slookup_rsp.to_string());
        // if the connection creation success
        if (slookup_rsp.hit) {
          // event engine should trigger three-way handshake,
          // if success, the rx engine will send notification
          to_event_eng_event = Event(SYN, slookup_rsp.session_id);
          tx_app_conn_handler_to_event_engine.write(to_event_eng_event);
          logger.Info(TX_APP_IF, EVENT_ENG, "Sent-SYN", to_event_eng_event.to_string());
          // write to state table is not required to release the lock
          to_sttable_req = StateTableReq(slookup_rsp.session_id, SYN_SENT, 1);
          tx_app_to_sttable_req.write(to_sttable_req);
          logger.Info(TX_APP_IF, STAT_TBLE, "Upd State", to_sttable_req.to_string());
        } else {
          // if the connection creation failed
          to_net_app_open_conn_rsp = AppOpenConnRsp(
              OpenConnRspNoTDEST(slookup_rsp.session_id, false), slookup_rsp.role_id);
          tx_app_to_net_app_open_conn_rsp.write(to_net_app_open_conn_rsp.to_net_axis());
          logger.Info(TX_APP_IF, NET_APP, "OpenConn Failed", to_net_app_open_conn_rsp.to_string());
        }
      } else if (!rx_eng_to_tx_app_notification.empty()) {
        rx_eng_to_tx_app_notification.read(open_session_reg);
        logger.Info(RX_ENGINE, TX_APP_IF, "OpenConn Notify", open_session_reg.to_string());
        tx_app_to_slookup_check_tdest_req.write(open_session_reg.session_id);
        logger.Info(TX_APP_IF, SLUP_CTRL, "CheckTDEST", open_session_reg.session_id.to_string(16));
        fsm_state = WAIT_FOR_TDEST_ACTIVE_OPEN;
      } else if (!rtimer_to_tx_app_notification.empty()) {
        rtimer_to_tx_app_notification.read(open_session_reg);
        logger.Info(RTRMT_TMR, TX_APP_IF, "OpenConn Notify", open_session_reg.to_string());
        tx_app_to_slookup_check_tdest_req.write(open_session_reg.session_id);
        logger.Info(TX_APP_IF, SLUP_CTRL, "CheckTDEST", open_session_reg.session_id.to_string(16));

        fsm_state = WAIT_FOR_TDEST_ACTIVE_OPEN;
      } else if (!rx_eng_to_tx_app_new_client_notification.empty()) {
        rx_eng_to_tx_app_new_client_notification.read(passive_open_session_reg);
        logger.Info(RTRMT_TMR, TX_APP_IF, "OpenClient Notify", open_session_reg.to_string());
        tx_app_to_slookup_check_tdest_req.write(passive_open_session_reg.session_id);
        logger.Info(
            TX_APP_IF, SLUP_CTRL, "CheckTDEST", passive_open_session_reg.session_id.to_string(16));
        fsm_state = WAIT_FOR_TDEST_PASSIVE_OPEN;
      } else if (!net_app_to_tx_app_close_conn_req.empty()) {
        app_close_session_reg = net_app_to_tx_app_close_conn_req.read();
        // read current session state, should write back to state table
        to_sttable_req = StateTableReq(app_close_session_reg.data);
        tx_app_to_sttable_req.write(to_sttable_req);
        logger.Info(NET_APP, TX_APP_IF, "CloseConn Req", app_close_session_reg.to_string());
        logger.Info(TX_APP_IF, STAT_TBLE, "Lup State", to_sttable_req.to_string());

        fsm_state = CLOSE_CONN;
      }
      break;
    case WAIT_FOR_TDEST_ACTIVE_OPEN:
      if (!slookup_to_tx_app_check_tdest_rsp.empty()) {
        NetAXISDest temp_dest    = slookup_to_tx_app_check_tdest_rsp.read();
        to_net_app_open_conn_rsp = AppOpenConnRsp(open_session_reg, temp_dest);
        tx_app_to_net_app_open_conn_rsp.write(to_net_app_open_conn_rsp.to_net_axis());
        logger.Info(SLUP_CTRL, TX_APP_IF, "TDEST Rsp", temp_dest.to_string(16));
        logger.Info(TX_APP_IF, NET_APP, "OpenConn Rsp", to_net_app_open_conn_rsp.to_string());

        fsm_state = IDLE;
      }
      break;
    case WAIT_FOR_TDEST_PASSIVE_OPEN:
      if (!slookup_to_tx_app_check_tdest_rsp.empty()) {
        NetAXISDest temp_dest        = slookup_to_tx_app_check_tdest_rsp.read();
        to_net_app_new_client_notify = NewClientNotification(passive_open_session_reg, temp_dest);
        net_app_new_client_notification.write(to_net_app_new_client_notify.to_net_axis());
        logger.Info(SLUP_CTRL, TX_APP_IF, "TDEST Rsp", temp_dest.to_string(16));
        logger.Info(
            TX_APP_IF, NET_APP, "NewClient Notify", to_net_app_new_client_notify.to_string());

        fsm_state = IDLE;
      }
      break;
    case CLOSE_CONN:
      if (!sttable_to_tx_app_rsp.empty()) {
        sttable_to_tx_app_rsp.read(sttable_rsp);
        logger.Info(STAT_TBLE, TX_APP_IF, "Lup State Rsp", state_to_string(sttable_rsp));
        // like close() in socket
        if ((sttable_rsp == ESTABLISHED) || (sttable_rsp == FIN_WAIT_1)) {
          to_sttable_req     = StateTableReq(app_close_session_reg.data, FIN_WAIT_1, 1);
          to_event_eng_event = Event(FIN, app_close_session_reg.data);
          tx_app_conn_handler_to_event_engine.write(to_event_eng_event);
          logger.Info(TX_APP_IF, EVENT_ENG, "Sent-FIN", to_event_eng_event.to_string());

        } else {
          // Have to release lock
          to_sttable_req = StateTableReq(app_close_session_reg.data, sttable_rsp, 1);
        }
        logger.Info(TX_APP_IF, STAT_TBLE, "Upd/Release State", to_sttable_req.to_string());
        tx_app_to_sttable_req.write(to_sttable_req);
        fsm_state = IDLE;
      }
      break;
  }
}

/** @ingroup tx_app_stream_if
 *  Reads the request from the application and loads the necessary metadata,
 *  the FSM decides if the packet is written to the TX buffer or discarded.
 *
 * NOTE: it should diable the pipeline
 */
void TxAppDataHandler(
    // net app
    stream<NetAXISAppTransDataReq> &net_app_to_tx_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_trans_data_rsp,
    stream<NetAXIS> &               net_app_trans_data,
    // tx app table
    stream<TxAppToTxAppTableReq> &tx_app_to_tx_app_table_req,
    stream<TxAppTableToTxAppRsp> &tx_app_table_to_tx_app_rsp,
    // state table
    stream<TcpSessionID> &tx_app_to_sttable_lup_req,
    stream<SessionState> &sttable_to_tx_app_lup_rsp,
    // to event eng
    stream<Event> &tx_app_to_event_eng_set_event,
    // to datamover
    stream<MemBufferRWCmd> &tx_app_to_mover_write_cmd,
    stream<NetAXISWord> &   tx_app_to_mover_write_data) {
#pragma HLS PIPELINE off

#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_trans_data_req
#pragma HLS INTERFACE axis register both port = tx_app_to_net_app_trans_data_rsp
#pragma HLS INTERFACE axis register both port = net_app_trans_data
#pragma HLS INTERFACE axis register both port = tx_app_to_mover_write_cmd
#pragma HLS INTERFACE axis register both port = tx_app_to_mover_write_data

  enum TxAppDataFsmState { READ_REQUEST, READ_META, READ_DATA };
  static TxAppDataFsmState fsm_state = READ_REQUEST;
  // lock step for one net app: recv request, lock, check availability, sending data/cmd to
  // datamover, unlock
  static bool trans_data_lock = false;

  static AppTransDataReq      trans_data_req;
  AppTransDataRsp             trans_data_rsp;
  static TxAppTableToTxAppRsp tx_app_table_rsp;
  static TcpSessionBuffer     max_write_length;
  TcpSessionBuffer            used_length;
  TcpSessionBuffer            useable_win;
  ap_uint<32>                 payload_mem_addr;
  TxAppToTxAppTableReq        to_tx_app_table_req;
  Event                       to_event_eng_event;
  MemBufferRWCmd              to_data_mover_cmd;
  switch (fsm_state) {
    case READ_REQUEST:
      if (!net_app_to_tx_app_trans_data_req.empty() && !trans_data_lock) {
        trans_data_req = net_app_to_tx_app_trans_data_req.read();
        logger.Info(NET_APP, TX_APP_IF, "TransData Req", trans_data_req.to_string());
        // get session state
        tx_app_to_sttable_lup_req.write(trans_data_req.id);
        logger.Info(TX_APP_IF, STAT_TBLE, "Lup State", trans_data_req.id.to_string(16));
        // get ack ptr
        to_tx_app_table_req = TxAppToTxAppTableReq(trans_data_req.id);
        tx_app_to_tx_app_table_req.write(to_tx_app_table_req);
        logger.Info("TX APPTable Req", to_tx_app_table_req.to_string());

        // disable pipeline, waiting for data
        trans_data_lock = true;
        fsm_state       = READ_META;
      }
      break;
    case READ_META:
      if (!sttable_to_tx_app_lup_rsp.empty() && !tx_app_table_to_tx_app_rsp.empty() &&
          trans_data_lock) {
        SessionState cur_session_state = sttable_to_tx_app_lup_rsp.read();
        logger.Info(STAT_TBLE, TX_APP_IF, "Session State", state_to_string(cur_session_state));

        tx_app_table_to_tx_app_rsp.read(tx_app_table_rsp);
        max_write_length = (tx_app_table_rsp.ackd - tx_app_table_rsp.app_written_ideal) - 1;
        logger.Info("TX AppTable Rsp", tx_app_table_rsp.to_string());
        logger.Info("Max Write Len", max_write_length.to_string(16));
#if (TCP_NODELAY)
        used_length = tx_app_table_rsp.app_written_ideal - tx_app_table_rsp.ackd;
        if (tx_app_table_rsp.min_window > used_length) {
          useable_win = tx_app_table_rsp.min_window - used_length;
        } else {
          useable_win = 0;
        }
#endif
        if (cur_session_state != ESTABLISHED) {
          trans_data_rsp.data.length          = trans_data_req.data;
          trans_data_rsp.data.remaining_space = max_write_length;
          trans_data_rsp.data.error           = ERROR_NOCONNECTION;
          fsm_state                           = READ_REQUEST;
          trans_data_lock                     = false;
        }
#if (TCP_NODELAY)
        else if (trans_data_req.data > useable_win) {
          // Notify app about fail
          trans_data_rsp.data.length          = trans_data_req.data;
          trans_data_rsp.data.remaining_space = useable_win;
          trans_data_rsp.data.error           = ERROR_WINDOW;
          fsm_state                           = READ_REQUEST;
          trans_data_lock                     = false;
        }
#endif
        else if (trans_data_req.data > max_write_length) {
          // Notify app about fail
          trans_data_rsp.data.length          = trans_data_req.data;
          trans_data_rsp.data.remaining_space = max_write_length;
          trans_data_rsp.data.error           = ERROR_NOSPACE;
          fsm_state                           = READ_REQUEST;
          trans_data_lock                     = false;
        } else {
          // normal
          // If DDR is not used in the RX start from the  beginning of the memory
          GetSessionMemAddr<0>(
              trans_data_req.id, tx_app_table_rsp.app_written_ideal, payload_mem_addr);

          to_data_mover_cmd = MemBufferRWCmd(payload_mem_addr, trans_data_req.data);
          tx_app_to_mover_write_cmd.write(to_data_mover_cmd);
          logger.Info(TX_APP_IF, DATA_MVER, "WriteMemCmd", to_data_mover_cmd.to_string());

          trans_data_rsp.data.length          = trans_data_req.data;
          trans_data_rsp.data.remaining_space = max_write_length;
          trans_data_rsp.data.error           = NO_ERROR;
          to_event_eng_event =
              Event(TX, trans_data_req.id, tx_app_table_rsp.app_written_ideal, trans_data_req.data);
          tx_app_to_event_eng_set_event.write(to_event_eng_event);
          logger.Info(TX_APP_IF, EVENT_ENG, "Tx Event", to_event_eng_event.to_string());
          // update tx app table app_written_ideal pointer
          tx_app_to_tx_app_table_req.write(TxAppToTxAppTableReq(
              trans_data_req.id, tx_app_table_rsp.app_written_ideal + trans_data_req.data));
          fsm_state = READ_DATA;
        }
        trans_data_rsp.dest = trans_data_req.dest;
        tx_app_to_net_app_trans_data_rsp.write(trans_data_rsp.to_axis());
        logger.Info(TX_APP_IF, NET_APP, "TransData Rsp", trans_data_rsp.to_string());
      }
      break;
    case READ_DATA:
      if (!net_app_trans_data.empty() && trans_data_lock) {
        NetAXIS cur_word = net_app_trans_data.read();
        tx_app_to_mover_write_data.write(cur_word);
        logger.Info(NET_APP, TX_APP_IF, "TransData", NetAXISWord(cur_word).to_string());
        logger.Info(TX_APP_IF, DATA_MVER, "WriteMem", NetAXISWord(cur_word).to_string());

        if (cur_word.last) {
          fsm_state       = READ_REQUEST;
          trans_data_lock = false;
        }
      }
      break;
  }
}

/**
 * @brief Tx app data handler for data mover status
 *
 * filter all TX event in @p tx_app_to_event_eng_cache, the TX event will write to
 * @p tx_app_to_event_eng_set_event iff the Datamover returned OKAY
 */
void                 TxAppRspHandler(stream<ap_uint<1> > &    mem_buffer_double_access_flag,
                                     stream<DataMoverStatus> &mover_to_tx_app_write_status,
                                     stream<Event> &          tx_app_to_event_eng_cache,
                                     stream<Event> &          tx_app_to_event_eng_set_event,
                                     stream<TxAppToTxSarReq> &tx_app_to_tx_sar_upd_req) {
#pragma HLS PIPELINE II = 1

#pragma HLS INTERFACE axis register both port = mover_to_tx_app_write_status

  enum StatusHandlerFsmState { READ_EV, READ_STATUS_1, READ_STATUS_2 };
  static StatusHandlerFsmState fsm_status = READ_EV;

  static Event cur_event;
  bool         double_access_flag = false;

  DataMoverStatus datamover_sts;
  TxAppToTxSarReq to_tx_sar_req;

  switch (fsm_status) {
    case READ_EV:
      if (!tx_app_to_event_eng_cache.empty()) {
        tx_app_to_event_eng_cache.read(cur_event);
        if (cur_event.type == TX) {
          fsm_status = READ_STATUS_1;
        } else {
          tx_app_to_event_eng_set_event.write(cur_event);
        }
      }
      break;
    case READ_STATUS_1:
      if (!mem_buffer_double_access_flag.empty() && !mover_to_tx_app_write_status.empty()) {
        datamover_sts      = mover_to_tx_app_write_status.read();
        double_access_flag = mem_buffer_double_access_flag.read();
        logger.Info(DATA_MVER, TX_APP_IF, "WriteMemSts", datamover_sts.to_string());

        if (datamover_sts.okay) {
          // overflow, write mem twice
          if (double_access_flag) {
            fsm_status = READ_STATUS_2;
          } else {
            // update the app_written in real
            to_tx_sar_req =
                TxAppToTxSarReq(cur_event.session_id, cur_event.buf_addr + cur_event.length);
            tx_app_to_tx_sar_upd_req.write(to_tx_sar_req);
            logger.Info(TX_APP_IF, TX_SAR_TB, "Upd AppWritten", to_tx_sar_req.to_string());
            fsm_status = READ_EV;
            tx_app_to_event_eng_set_event.write(cur_event);
          }
        } else {
          fsm_status = READ_EV;
        }
      }
      break;
    case READ_STATUS_2:
      if (!mover_to_tx_app_write_status.empty()) {
        // wait for another write status
        mover_to_tx_app_write_status.read(datamover_sts);

        if (datamover_sts.okay) {
          // App pointer update, pointer is released
          to_tx_sar_req =
              TxAppToTxSarReq(cur_event.session_id, cur_event.buf_addr + cur_event.length);
          tx_app_to_tx_sar_upd_req.write(to_tx_sar_req);
          logger.Info(TX_APP_IF, TX_SAR_TB, "Upd AppWritten BreakDown", to_tx_sar_req.to_string());
          tx_app_to_event_eng_set_event.write(cur_event);
        }
        fsm_status = READ_EV;
      }
      break;
    default:
      fsm_status = READ_EV;
      break;
  }
}

void TxAppTableInterface(
    // tx sar update tx app table
    stream<TxSarToTxAppReq> &tx_sar_to_tx_app_upd_req,
    // net app write data, R/W tx app table
    stream<TxAppToTxAppTableReq> &tx_app_to_tx_app_table_req,
    stream<TxAppTableToTxAppRsp> &tx_app_table_to_tx_app_rsp) {
#pragma HLS PIPELINE II = 1

  static TxAppTableEntry tx_app_table[TCP_MAX_SESSIONS];
#pragma HLS bind_storage variable = tx_app_table type = RAM_T2P impl = BRAM

  TxSarToTxAppReq      tx_sar_upd_req;
  TxAppToTxAppTableReq tx_app_req;
  TxAppTableToTxAppRsp to_tx_app_rsp;
  // tx sar write req
  if (!tx_sar_to_tx_app_upd_req.empty()) {
    tx_sar_to_tx_app_upd_req.read(tx_sar_upd_req);
    logger.Info(TX_SAR_TB, TX_APP_IF, "Upd APPtable", tx_sar_upd_req.to_string());
    if (tx_sar_upd_req.init) {
      // At init this is actually not_ackd
      tx_app_table[tx_sar_upd_req.session_id].ackd              = tx_sar_upd_req.ackd - 1;
      tx_app_table[tx_sar_upd_req.session_id].app_written_ideal = tx_sar_upd_req.ackd;
#if (TCP_NODELAY)
      tx_app_table[tx_sar_upd_req.session_id].min_window = tx_sar_upd_req.min_window;
#endif
    } else {
      tx_app_table[tx_sar_upd_req.session_id].ackd = tx_sar_upd_req.ackd;
#if (TCP_NODELAY)
      tx_app_table[tx_sar_upd_req.session_id].min_window = tx_sar_upd_req.min_window;
#endif
    }
  }
  // tx app read/write req
  else if (!tx_app_to_tx_app_table_req.empty()) {
    tx_app_to_tx_app_table_req.read(tx_app_req);
    logger.Info("APPtable R/W req", tx_app_req.to_string());

    if (tx_app_req.write) {
      tx_app_table[tx_app_req.session_id].app_written_ideal = tx_app_req.app_written_ideal;
    } else {
      // tx app read rsp
#if !(TCP_NODELAY)
      to_tx_app_rsp = TxAppTableToTxAppRsp(tx_app_req.session_id,
                                           tx_app_table[tx_app_req.session_id].ackd,
                                           tx_app_table[tx_app_req.session_id].app_written_ideal);
#else
      to_tx_app_rsp = TxAppTableToTxAppRsp(tx_app_req.session_id,
                                           tx_app_table[tx_app_req.session_id].ackd,
                                           tx_app_table[tx_app_req.session_id].app_written_ideal,
                                           tx_app_table[tx_app_req.session_id].min_window);
#endif
      tx_app_table_to_tx_app_rsp.write(to_tx_app_rsp);
      logger.Info("APPtable Read Rsp", to_tx_app_rsp.to_string());
    }
  }
}

void tx_app_intf(
    // net app connection request
    stream<NetAXISAppOpenConnReq> & net_app_to_tx_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp> & tx_app_to_net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_to_tx_app_close_conn_req,
    // rx eng -> net app
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    stream<NetAXISNewClientNotification> &net_app_new_client_notification,
    // rx eng
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // retrans timer
    stream<OpenConnRspNoTDEST> &rtimer_to_tx_app_notification,
    // session lookup, also for TDEST
    stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
    stream<SessionLookupRsp> & slookup_to_tx_app_rsp,
    stream<TcpSessionID> &     tx_app_to_slookup_check_tdest_req,
    stream<NetAXISDest> &      slookup_to_tx_app_check_tdest_rsp,
    // port table req/rsp
    stream<NetAXISDest> &  tx_app_to_ptable_req,
    stream<TcpPortNumber> &ptable_to_tx_app_rsp,
    // state table read/write req/rsp
    stream<StateTableReq> &tx_app_to_sttable_req,
    stream<SessionState> & sttable_to_tx_app_rsp,

    // net app data request
    stream<NetAXISAppTransDataReq> &net_app_to_tx_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_trans_data_rsp,
    stream<NetAXIS> &               net_app_trans_data,
    // to/from tx sar upd req
    stream<TxAppToTxSarReq> &tx_app_to_tx_sar_upd_req,
    stream<TxSarToTxAppReq> &tx_sar_to_tx_app_upd_req,
    // to event eng
    stream<Event> &tx_app_to_event_eng_set_event,
    // state table
    stream<TcpSessionID> &tx_app_to_sttable_lup_req,
    stream<SessionState> &sttable_to_tx_app_lup_rsp,
    // datamover req/rsp
    stream<DataMoverCmd> &   tx_app_to_mover_write_cmd,
    stream<NetAXIS> &        tx_app_to_mover_write_data,
    stream<DataMoverStatus> &mover_to_tx_app_write_status,
    // in big endian
    IpAddr &my_ip_addr) {
//#pragma HLS          INLINE
//#pragma HLS PIPELINE II = 1
#pragma HLS DATAFLOW

#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_open_conn_req
#pragma HLS INTERFACE axis register both port = tx_app_to_net_app_open_conn_rsp
#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_close_conn_req
#pragma HLS INTERFACE axis register both port = net_app_new_client_notification
#pragma HLS INTERFACE axis register both port = net_app_to_tx_app_trans_data_req
#pragma HLS INTERFACE axis register both port = tx_app_to_net_app_trans_data_rsp
#pragma HLS INTERFACE axis register both port = net_app_trans_data
#pragma HLS INTERFACE axis register both port = tx_app_to_mover_write_cmd
#pragma HLS INTERFACE axis register both port = tx_app_to_mover_write_data
#pragma HLS INTERFACE axis register both port = mover_to_tx_app_write_status

  // Fifos
  static stream<TxAppToTxAppTableReq> tx_app_to_tx_app_table_req_fifo(
      "tx_app_to_tx_app_table_req_fifo");
  static stream<TxAppTableToTxAppRsp> tx_app_table_to_tx_app_rsp_fifo(
      "tx_app_table_to_tx_app_rsp_fifo");
#pragma HLS stream variable = tx_app_to_tx_app_table_req_fifo depth = 2
#pragma HLS stream variable = tx_app_table_to_tx_app_rsp_fifo depth = 2
#pragma HLS aggregate variable = tx_app_to_tx_app_table_req_fifo compact = bit
#pragma HLS aggregate variable = tx_app_table_to_tx_app_rsp_fifo compact = bit

  static stream<Event> tx_app_conn_handler_to_event_engine_fifo(
      "tx_app_conn_handler_to_event_engine_fifo");
  static stream<Event> tx_app_data_handler_to_event_engine_fifo(
      "tx_app_data_handler_to_event_engine_fifo");
#pragma HLS stream variable = tx_app_conn_handler_to_event_engine_fifo depth = 4
#pragma HLS stream variable = tx_app_data_handler_to_event_engine_fifo depth = 4
#pragma HLS aggregate variable = tx_app_conn_handler_to_event_engine_fifo compact = bit
#pragma HLS aggregate variable = tx_app_data_handler_to_event_engine_fifo compact = bit

  static stream<Event> tx_app_to_event_eng_cache("tx_app_to_event_eng_cache");
#pragma HLS stream variable = tx_app_to_event_eng_cache depth = 16
#pragma HLS aggregate variable = tx_app_to_event_eng_cache compact = bit

  static stream<MemBufferRWCmd> tx_app_to_mem_write_cmd_fifo("tx_app_to_mem_write_cmd_fifo");
#pragma HLS aggregate variable = tx_app_to_mem_write_cmd_fifo compact = bit
#pragma HLS stream variable = tx_app_to_mem_write_cmd_fifo depth = 4

  static stream<ap_uint<1> > tx_app_to_mem_double_access_fifo("tx_app_to_mem_double_access_fifo");
#pragma HLS stream variable = tx_app_to_mem_double_access_fifo depth = 4

  static stream<NetAXISWord> tx_app_to_mem_write_data_fifo("tx_app_to_mem_write_data_fifo");
#pragma HLS aggregate variable = tx_app_to_mem_write_data_fifo compact = bit
#pragma HLS stream variable = tx_app_to_mem_write_data_fifo depth = 4
  TxAppConnectionHandler(
      // net app
      net_app_to_tx_app_open_conn_req,
      tx_app_to_net_app_open_conn_rsp,
      net_app_to_tx_app_close_conn_req,
      // rx eng
      rx_eng_to_tx_app_new_client_notification,
      net_app_new_client_notification,
      // rx eng
      rx_eng_to_tx_app_notification,
      // retrans timer
      rtimer_to_tx_app_notification,
      // session lookup, also for TDEST
      tx_app_to_slookup_req,
      slookup_to_tx_app_rsp,
      tx_app_to_slookup_check_tdest_req,
      slookup_to_tx_app_check_tdest_rsp,
      // port table req/rsp
      tx_app_to_ptable_req,
      ptable_to_tx_app_rsp,
      // state table read/write req/rsp
      tx_app_to_sttable_req,
      sttable_to_tx_app_rsp,
      tx_app_conn_handler_to_event_engine_fifo,
      my_ip_addr);
  // data handler
  TxAppDataHandler(
      // net app
      net_app_to_tx_app_trans_data_req,
      tx_app_to_net_app_trans_data_rsp,
      net_app_trans_data,
      // tx app table
      tx_app_to_tx_app_table_req_fifo,
      tx_app_table_to_tx_app_rsp_fifo,
      // state table
      tx_app_to_sttable_lup_req,
      sttable_to_tx_app_lup_rsp,
      // to event eng
      tx_app_data_handler_to_event_engine_fifo,
      // to datamover
      tx_app_to_mem_write_cmd_fifo,
      tx_app_to_mem_write_data_fifo);

  WriteDataToMem<0>(tx_app_to_mem_write_cmd_fifo,
                    tx_app_to_mem_write_data_fifo,
                    tx_app_to_mover_write_cmd,
                    tx_app_to_mover_write_data,
                    tx_app_to_mem_double_access_fifo);

  TxAppEventMerger(tx_app_conn_handler_to_event_engine_fifo,
                   tx_app_data_handler_to_event_engine_fifo,
                   tx_app_to_event_eng_cache);

  TxAppRspHandler(tx_app_to_mem_double_access_fifo,
                  mover_to_tx_app_write_status,
                  tx_app_to_event_eng_cache,
                  tx_app_to_event_eng_set_event,
                  tx_app_to_tx_sar_upd_req);

  // TX App Meta Table
  TxAppTableInterface(
      tx_sar_to_tx_app_upd_req, tx_app_to_tx_app_table_req_fifo, tx_app_table_to_tx_app_rsp_fifo);
}