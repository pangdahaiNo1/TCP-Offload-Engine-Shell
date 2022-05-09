
#include "tx_app_intf.hpp"

using namespace hls;

void TxAppEventMerger(stream<Event> &tx_app_conn_handler_to_event_engine,
                      stream<Event> &tx_app_data_handler_to_event_engine,
#if (TCP_NODELAY)
                      stream<Event> &tx_app_data_handler_event_cache,
#endif
                      stream<Event> &tx_app_to_event_eng_set_event) {
#pragma HLS PIPELINE II = 1

  Event ev;
  // Merge Events
  if (!tx_app_conn_handler_to_event_engine.empty()) {
    tx_app_to_event_eng_set_event.write(tx_app_conn_handler_to_event_engine.read());
  } else if (!tx_app_data_handler_to_event_engine.empty()) {
    tx_app_data_handler_to_event_engine.read(ev);
    tx_app_to_event_eng_set_event.write(ev);
#if (TCP_NODELAY)
    if (ev.type == TX) {
      tx_app_data_handler_event_cache.write(ev);
    }
#endif
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
    // rx eng
    stream<OpenSessionStatus> &rx_eng_to_tx_app_notification,
    // retrans timer
    stream<OpenSessionStatus> &rtimer_to_tx_app_notification,
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

  enum TxAppConnFsmState { IDLE, CLOSE_CONN, WAIT_FOR_TDEST };
  static TxAppConnFsmState fsm_state = IDLE;

  static NetAXISAppOpenConnReq app_open_req_reg;
  static bool                  tx_app_wait_for_free_port_lock = false;
  // get free port number from port table, request to ptable for recording the TDSET
  if (!net_app_to_tx_app_open_conn_req.empty() && !tx_app_wait_for_free_port_lock) {
    app_open_req_reg = net_app_to_tx_app_open_conn_req.read();
    tx_app_to_ptable_req.write(app_open_req_reg.dest);
    tx_app_wait_for_free_port_lock = true;
  } else if (!ptable_to_tx_app_rsp.empty() && tx_app_wait_for_free_port_lock) {
    TcpPortNumber free_port = ptable_to_tx_app_rsp.read();
    // ip is in the big endian, the port number from port table is little endian
    tx_app_to_slookup_req.write(
        TxAppToSlookupReq(FourTuple(my_ip_addr,
                                    app_open_req_reg.data.ip_addr,
                                    SwapByte<16>(free_port),
                                    SwapByte<16>(app_open_req_reg.data.tcp_port)),
                          app_open_req_reg.dest));
    tx_app_wait_for_free_port_lock = false;
  }

  SessionLookupRsp              slookup_rsp;
  SessionState                  sttable_rsp;
  static OpenSessionStatus      open_session_reg;
  static NetAXISAppCloseConnReq app_close_session_reg;
  // handle net app open/close connection request
  switch (fsm_state) {
    case IDLE:
      if (!slookup_to_tx_app_rsp.empty()) {
        slookup_to_tx_app_rsp.read(slookup_rsp);
        // get the session state
        cout << slookup_rsp.to_string() << endl;
        if (slookup_rsp.hit) {
          tx_app_to_sttable_req.write(StateTableReq(slookup_rsp.session_id, SYN_SENT, 1));
          tx_app_conn_handler_to_event_engine.write(Event(SYN, slookup_rsp.session_id));
        } else {
          tx_app_to_net_app_open_conn_rsp.write(NetAXISAppOpenConnRsp(
              OpenSessionStatus(slookup_rsp.session_id, false), slookup_rsp.role_id));
        }
      } else if (!rx_eng_to_tx_app_notification.empty()) {
        rx_eng_to_tx_app_notification.read(open_session_reg);
        tx_app_to_slookup_check_tdest_req.write(open_session_reg.session_id);
        fsm_state = WAIT_FOR_TDEST;
      } else if (!rtimer_to_tx_app_notification.empty()) {
        rtimer_to_tx_app_notification.read(open_session_reg);
        tx_app_to_slookup_check_tdest_req.write(open_session_reg.session_id);
        fsm_state = WAIT_FOR_TDEST;
      } else if (!net_app_to_tx_app_close_conn_req.empty()) {
        net_app_to_tx_app_close_conn_req.read(app_close_session_reg);
        // read current session state
        tx_app_to_sttable_req.write(StateTableReq(app_close_session_reg.data));
        fsm_state = CLOSE_CONN;
      }
      break;
    case WAIT_FOR_TDEST:
      if (!slookup_to_tx_app_check_tdest_rsp.empty()) {
        // TODO: need a lock for session lookup zelin 22-05-08
        NetAXISDest temp_dest = slookup_to_tx_app_check_tdest_rsp.read();
        tx_app_to_net_app_open_conn_rsp.write(NetAXISAppOpenConnRsp(open_session_reg, temp_dest));
        fsm_state = IDLE;
      }
      break;
    case CLOSE_CONN:
      if (!sttable_to_tx_app_rsp.empty()) {
        sttable_to_tx_app_rsp.read(sttable_rsp);
        // TODO might add CLOSE_WAIT here???
        std::cout << "State " << std::dec << sttable_rsp;
        if ((sttable_rsp == ESTABLISHED) || (sttable_rsp == FIN_WAIT_2) ||
            (sttable_rsp == FIN_WAIT_1)) {
          tx_app_to_sttable_req.write(StateTableReq(app_close_session_reg.data, FIN_WAIT_1, 1));
          tx_app_conn_handler_to_event_engine.write(Event(FIN, app_close_session_reg.data));
          std::cout << " set event FIN " << std::endl << std::endl;
        } else {
          // Have to release lock
          tx_app_to_sttable_req.write(StateTableReq(app_close_session_reg.data, sttable_rsp, 1));
        }
        fsm_state = IDLE;
      }
      break;
  }
}

/** @ingroup tx_app_stream_if
 *  Reads the request from the application and loads the necessary metadata,
 *  the FSM decides if the packet is written to the TX buffer or discarded.
 */
void TxAppDataHandler(
    // net app
    stream<NetAXISAppTransDataReq> &net_app_to_tx_app_trans_data_req,
    stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_tans_data_rsp,
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
    stream<NetAXIS> &     tx_app_to_mem_write_data,
    stream<DataMoverCmd> &tx_app_to_mem_write_cmd) {
  // #pragma HLS pipeline II = 1

  enum TxAppDataFsmState { READ_REQUEST, READ_META, READ_DATA };
  static TxAppDataFsmState fsm_state = READ_META;
  // lock step for one net app: recv request, lock, check availability, sending data/cmd to
  // datamover, unlock
  static bool trans_data_lock = false;

  static NetAXISAppTransDataReq trans_data_req;
  AppTransDataRsp               trans_data_rsp;
  static TxAppTableToTxAppRsp   tx_app_table_rsp;
  static TcpSessionBuffer       max_write_length;
  TcpSessionBuffer              used_length;
  TcpSessionBuffer              useable_win;
  ap_uint<32>                   trans_data_mem_addr;

  switch (fsm_state) {
    case READ_REQUEST:
      if (!net_app_to_tx_app_trans_data_req.empty() && !trans_data_lock) {
        net_app_to_tx_app_trans_data_req.read(trans_data_req);
        // get session state
        tx_app_to_sttable_lup_req.write(trans_data_req.id);
        // get ack ptr
        tx_app_to_tx_app_table_req.write(TxAppToTxAppTableReq(trans_data_req.id));
        // disable pipeline, waiting for data
        trans_data_lock = true;
        fsm_state       = READ_META;
      }
      break;
    case READ_META:
      if (!sttable_to_tx_app_lup_rsp.empty() && !tx_app_table_to_tx_app_rsp.empty()) {
        SessionState cur_session_state = sttable_to_tx_app_lup_rsp.read();
        tx_app_table_to_tx_app_rsp.read(tx_app_table_rsp);
        max_write_length = (tx_app_table_rsp.ackd - tx_app_table_rsp.mempt) - 1;
#if (TCP_NODELAY)
        used_length = tx_app_table_rsp.mempt - tx_app_table_rsp.ackd;
        if (tx_app_table_rsp.min_window > used_length) {
          useable_win = tx_app_table_rsp.min_window - used_length;
        } else {
          useable_win = 0;
        }
#endif
        if (cur_session_state != ESTABLISHED) {
          trans_data_rsp.length          = trans_data_req.data;
          trans_data_rsp.remaining_space = max_write_length;
          trans_data_rsp.error           = ERROR_NOCONNECTION;
          fsm_state                      = READ_REQUEST;
          trans_data_lock                = false;
        }
#if (TCP_NODELAY)
        else if (trans_data_req.data > useable_win) {
          // Notify app about fail
          trans_data_rsp.length          = trans_data_req.data;
          trans_data_rsp.remaining_space = useable_win;
          trans_data_rsp.error           = ERROR_WINDOW;
          fsm_state                      = READ_REQUEST;
          trans_data_lock                = false;
        }
#endif
        else if (trans_data_req.data > max_write_length) {
          // Notify app about fail
          trans_data_rsp.length          = trans_data_req.data;
          trans_data_rsp.remaining_space = max_write_length;
          trans_data_rsp.error           = ERROR_NOSPACE;
          fsm_state                      = READ_REQUEST;
          trans_data_lock                = false;
        } else {
          // normal
          // If DDR is not used in the RX start from the  beginning of the memory
          trans_data_mem_addr(31, 30)             = (!TCP_RX_DDR_BYPASS);
          trans_data_mem_addr(29, WINDOW_BITS)    = trans_data_req.id(13, 0);
          trans_data_mem_addr(WINDOW_BITS - 1, 0) = tx_app_table_rsp.mempt;
          tx_app_to_mem_write_cmd.write(DataMoverCmd(trans_data_mem_addr, trans_data_req.data));
          trans_data_rsp.length          = trans_data_req.data;
          trans_data_rsp.remaining_space = max_write_length;
          trans_data_rsp.error           = NO_ERROR;
          tx_app_to_event_eng_set_event.write(
              Event(TX, trans_data_req.id, tx_app_table_rsp.mempt, trans_data_req.data));
          tx_app_to_tx_app_table_req.write(TxAppToTxAppTableReq(
              trans_data_req.id, tx_app_table_rsp.mempt + trans_data_req.data));
          fsm_state = READ_DATA;
        }
        tx_app_to_net_app_tans_data_rsp.write(
            NetAXISAppTransDataRsp(trans_data_rsp, trans_data_req.id));
        break;
      }
    case READ_DATA:
      if (!net_app_trans_data.empty()) {
        tx_app_to_mem_write_data.write(net_app_trans_data.read());
        fsm_state       = READ_REQUEST;
        trans_data_lock = false;
      }
      break;
  }
}

// Tx app data handler for data mover status
void TxAppRspHandler(stream<DataMoverStatus> &mover_to_tx_app_sts,
                     stream<Event> &          tx_app_data_handler_event_cache,
#if (!TCP_NODELAY)
                     stream<Event> &tx_app_data_handler_to_event_engine,
#endif
                     stream<TxAppToTxSarReq> &tx_app_to_tx_sar_req) {
#pragma HLS pipeline II = 1

  enum StatusHandlerFsmState { READ_EV, READ_STATUS_1, READ_STATUS_2 };
  static StatusHandlerFsmState fsm_status = READ_EV;

  static Event event_i;

  DataMoverStatus          datamover_sts;
  ap_uint<WINDOW_BITS + 1> temp_length;

  switch (fsm_status) {
    case READ_EV:
      if (!tx_app_data_handler_event_cache.empty()) {
        tx_app_data_handler_event_cache.read(event_i);
        if (event_i.type == TX) {
          fsm_status = READ_STATUS_1;
        }
#if (!TCP_NODELAY)
        else {
          tx_app_data_handler_to_event_engine.write(event_i);
        }
#endif
      }
      break;
    case READ_STATUS_1:
      if (!mover_to_tx_app_sts.empty()) {
        mover_to_tx_app_sts.read(datamover_sts);

        temp_length = event_i.buf_addr + event_i.length;

        if (datamover_sts.okay) {
          if (temp_length.bit(WINDOW_BITS)) {
            fsm_status = READ_STATUS_2;
          } else {
            tx_app_to_tx_sar_req.write(TxAppToTxSarReq(
                event_i.session_id,
                event_i.buf_addr + event_i.length));  // App pointer update, pointer is released
            fsm_status = READ_EV;
#if (!TCP_NODELAY)
            tx_app_data_handler_to_event_engine.write(event_i);
#endif
          }
        }
      }
      break;
    case READ_STATUS_2:
      if (!mover_to_tx_app_sts.empty()) {
        mover_to_tx_app_sts.read(datamover_sts);

        if (datamover_sts.okay) {
          tx_app_to_tx_sar_req.write(TxAppToTxSarReq(
              event_i.session_id,
              event_i.buf_addr + event_i.length));  // App pointer update, pointer is released
#if (!TCP_NODELAY)
          tx_app_data_handler_to_event_engine.write(event_i);
#endif
          fsm_status = READ_EV;
        }
      }
      break;
    default:
      fsm_status = READ_EV;
      break;
  }
}

void                 TxAppTableInterface(stream<TxSarToTxAppRsp> &     tx_sar_to_tx_app_rsp,
                                         stream<TxAppToTxAppTableReq> &tx_app_to_tx_app_table_req,
                                         stream<TxAppTableToTxAppRsp> &tx_app_table_to_tx_app_rsp) {
#pragma HLS PIPELINE II = 1

  static TxAppTableEntry tx_app_table[TCP_MAX_SESSIONS];
#pragma HLS RESOURCE variable = tx_app_table core = RAM_T2P_BRAM

  TxSarToTxAppRsp      ackPush;
  TxAppToTxAppTableReq txAppUpdate;

  if (!tx_sar_to_tx_app_rsp.empty()) {
    tx_sar_to_tx_app_rsp.read(ackPush);
    if (ackPush.init) {
      // At init this is actually not_ackd
      tx_app_table[ackPush.session_id].ackd  = ackPush.ackd - 1;
      tx_app_table[ackPush.session_id].mempt = ackPush.ackd;
      std::cout << "tx_app_table  .ackd " << std::hex << tx_app_table[ackPush.session_id].ackd
                << "\t.mempt " << tx_app_table[ackPush.session_id].mempt << std::endl;
#if (TCP_NODELAY)
      tx_app_table[ackPush.session_id].min_window = ackPush.min_window;
#endif
    } else {
      tx_app_table[ackPush.session_id].ackd = ackPush.ackd;
#if (TCP_NODELAY)
      tx_app_table[ackPush.session_id].min_window = ackPush.min_window;
#endif
    }
  } else if (!tx_app_to_tx_app_table_req.empty()) {
    tx_app_to_tx_app_table_req.read(txAppUpdate);
    // Write
    if (txAppUpdate.write) {
      tx_app_table[txAppUpdate.session_id].mempt = txAppUpdate.mempt;
    } else {  // Read

#if !(TCP_NODELAY)
      tx_app_table_to_tx_app_rsp.write(
          TxAppTableToTxAppRsp(txAppUpdate.session_id,
                               tx_app_table[txAppUpdate.session_id].ackd,
                               tx_app_table[txAppUpdate.session_id].mempt));
#else
      tx_app_table_to_tx_app_rsp.write(
          TxAppTableToTxAppRsp(txAppUpdate.session_id,
                               tx_app_table[txAppUpdate.session_id].ackd,
                               tx_app_table[txAppUpdate.session_id].mempt,
                               tx_app_table[txAppUpdate.session_id].min_window));
#endif
    }
  }
}

void tx_app_intf(
    // net app connection request
    stream<NetAXISAppOpenConnReq> & net_app_to_tx_app_open_conn_req,
    stream<NetAXISAppOpenConnRsp> & tx_app_to_net_app_open_conn_rsp,
    stream<NetAXISAppCloseConnReq> &net_app_to_tx_app_close_conn_req,
    // rx eng
    stream<OpenSessionStatus> &rx_eng_to_tx_app_notification,
    // retrans timer
    stream<OpenSessionStatus> &rtimer_to_tx_app_notification,
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
    stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_tans_data_rsp,
    stream<NetAXIS> &               net_app_trans_data,
    // tx sar req/rsp
    stream<TxAppToTxSarReq> &tx_app_to_tx_sar_req,
    stream<TxSarToTxAppRsp> &tx_sar_to_tx_app_rsp,
    // to event eng
    stream<Event> &tx_app_to_event_eng_set_event,
    // state table
    stream<TcpSessionID> &tx_app_to_sttable_lup_req,
    stream<SessionState> &sttable_to_tx_app_lup_rsp,
    // datamover req/rsp
    stream<NetAXIS> &        tx_app_to_mem_write_data,
    stream<DataMoverCmd> &   tx_app_to_mem_write_cmd,
    stream<DataMoverStatus> &mem_to_tx_app_write_status,
    // in big endian
    IpAddr &my_ip_addr) {
#pragma HLS INLINE

  // Fifos
  static stream<TxAppToTxAppTableReq> tx_app_to_tx_app_table_req_fifo(
      "tx_app_to_tx_app_table_req_fifo");
  static stream<TxAppTableToTxAppRsp> tx_app_table_to_tx_app_rsp_fifo(
      "tx_app_table_to_tx_app_rsp_fifo");
#pragma HLS stream variable = tx_app_to_tx_app_table_req_fifo depth = 2
#pragma HLS stream variable = tx_app_table_to_tx_app_rsp_fifo depth = 2
#pragma HLS DATA_PACK variable = tx_app_to_tx_app_table_req_fifo
#pragma HLS DATA_PACK variable = tx_app_table_to_tx_app_rsp_fifo

  static stream<Event> tx_app_conn_handler_to_event_engine_fifo(
      "tx_app_conn_handler_to_event_engine_fifo");
  static stream<Event> tx_app_data_handler_to_event_engine_fifo(
      "tx_app_data_handler_to_event_engine_fifo");
#pragma HLS stream variable = tx_app_conn_handler_to_event_engine_fifo depth = 2
#pragma HLS stream variable = tx_app_data_handler_to_event_engine_fifo depth = 2
#pragma HLS DATA_PACK variable = tx_app_conn_handler_to_event_engine_fifo
#pragma HLS DATA_PACK variable = tx_app_data_handler_to_event_engine_fifo

  static stream<Event> tx_app_to_event_engine_fifo("tx_app_to_event_engine_fifo");
#pragma HLS stream variable = tx_app_to_event_engine_fifo depth    = 2
#pragma HLS DATA_PACK                                     variable = tx_app_to_event_engine_fifo

  // only enable in TCP_NODELAY
  static stream<Event> tx_app_to_status_handler_fifo("tx_app_to_status_handler_fifo");
#pragma HLS stream variable = tx_app_to_status_handler_fifo depth    = 64
#pragma HLS DATA_PACK                                       variable = tx_app_to_status_handler_fifo

  static stream<DataMoverCmd> tx_app_to_mem_write_cmd_fifo("tx_app_to_mem_write_cmd_fifo");
#pragma HLS DATA_PACK variable = tx_app_to_mem_write_cmd_fifo
#pragma HLS stream variable = tx_app_to_mem_write_cmd_fifo depth = 4

  static stream<NetAXIS> tx_app_to_mem_write_data_fifo("tx_app_to_mem_write_data_fifo");
#pragma HLS DATA_PACK variable = tx_app_to_mem_write_data_fifo
#pragma HLS stream variable = tx_app_to_mem_write_data_fifo depth = 4
  TxAppConnectionHandler(
      // net app
      net_app_to_tx_app_open_conn_req,
      tx_app_to_net_app_open_conn_rsp,
      net_app_to_tx_app_close_conn_req,
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
      my_ip_addr

  );
  // data handler
  TxAppDataHandler(
      // net app
      net_app_to_tx_app_trans_data_req,
      tx_app_to_net_app_tans_data_rsp,
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
      tx_app_to_mem_write_data_fifo,
      tx_app_to_mem_write_cmd_fifo);

  TxAppWriteDataToMem(tx_app_to_mem_write_data_fifo,
                      tx_app_to_mem_write_cmd_fifo,
                      tx_app_to_mem_write_data,
                      tx_app_to_mem_write_cmd);

  TxAppEventMerger(tx_app_conn_handler_to_event_engine_fifo,
                   tx_app_data_handler_to_event_engine_fifo,
#if (TCP_NODELAY)
                   tx_app_to_status_handler_fifo,
                   tx_app_to_event_eng_set_event
#else

                   tx_app_to_event_engine_fifo
#endif
  );

  TxAppRspHandler(mem_to_tx_app_write_status,
#if (TCP_NODELAY)
                  tx_app_to_status_handler_fifo,
#else
                  tx_app_to_event_engine_fifo,
                  tx_app_to_event_eng_set_event,
#endif
                  tx_app_to_tx_sar_req);

  // TX App Meta Table
  TxAppTableInterface(
      tx_sar_to_tx_app_rsp, tx_app_to_tx_app_table_req_fifo, tx_app_table_to_tx_app_rsp_fifo);
}