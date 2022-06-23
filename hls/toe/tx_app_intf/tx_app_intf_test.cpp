#include "tx_app_intf.hpp"
// DONOT change the header files sequence
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace hls;
MockLogger logger("./tx_app_intf_inner.log", TX_APP_IF);

void EmptyTxAppConnFifos(MockLogger &                   logger,
                         stream<NetAXISDest> &          tx_app_to_ptable_req,
                         stream<TxAppToSlookupReq> &    tx_app_to_slookup_req,
                         stream<StateTableReq> &        tx_app_to_sttable_req,
                         stream<TcpSessionID> &         tx_app_to_slookup_check_tdest_req,
                         stream<NetAXISAppOpenConnRsp> &tx_app_to_net_app_open_conn_rsp) {
  NetAXISDest           role_id;
  TxAppToSlookupReq     to_slookup_req;
  StateTableReq         to_sttable_req;
  TcpSessionID          to_slookup_checktdest;
  NetAXISAppOpenConnRsp to_net_app_conn_rsp;

  while (!tx_app_to_ptable_req.empty()) {
    tx_app_to_ptable_req.read(role_id);
    logger.Info(TX_APP_IF, PORT_TBLE, "FreePort Req", role_id.to_string(16));
  }
  while (!tx_app_to_slookup_req.empty()) {
    tx_app_to_slookup_req.read(to_slookup_req);
    logger.Info(TX_APP_IF, SLUP_CTRL, "R/W Req", to_slookup_req.to_string());
  }
  while (!tx_app_to_sttable_req.empty()) {
    tx_app_to_sttable_req.read(to_sttable_req);
    logger.Info(TX_APP_IF, STAT_TBLE, "Lup Req", to_sttable_req.to_string());
  }
  while (!tx_app_to_slookup_check_tdest_req.empty()) {
    tx_app_to_slookup_check_tdest_req.read(to_slookup_checktdest);
    logger.Info(TX_APP_IF, SLUP_CTRL, "CheckTDEST Req", to_slookup_checktdest.to_string(16));
  }
  while (!tx_app_to_net_app_open_conn_rsp.empty()) {
    tx_app_to_net_app_open_conn_rsp.read(to_net_app_conn_rsp);
    logger.Info(
        TX_APP_IF, NET_APP, "App Conn Rsp", AppOpenConnRsp(to_net_app_conn_rsp).to_string());
  }
}

void EmptyTxAppDataFifos(MockLogger &                    logger,
                         stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_trans_data_rsp,

                         stream<TcpSessionID> &tx_app_to_sttable_lup_req,
                         stream<Event> &       tx_app_to_event_eng_set_event) {
  NetAXISAppTransDataRsp to_net_app_rsp;

  TcpSessionID to_sttable;
  Event        to_event_engine;
  while (!tx_app_to_net_app_trans_data_rsp.empty()) {
    tx_app_to_net_app_trans_data_rsp.read(to_net_app_rsp);
    logger.Info(TX_APP_IF, NET_APP, "Trans Data Rsp", AppTransDataRsp(to_net_app_rsp).to_string());
  }

  while (!tx_app_to_sttable_lup_req.empty()) {
    tx_app_to_sttable_lup_req.read(to_sttable);
    logger.Info(TX_APP_IF, STAT_TBLE, "Lup Req", to_sttable.to_string(16));
  }
  while (!tx_app_to_event_eng_set_event.empty()) {
    tx_app_to_event_eng_set_event.read(to_event_engine);
    logger.Info(TX_APP_IF, EVENT_ENG, "Set Event", to_event_engine.to_string());
  }
}

void EmptyDataMoverFifos(MockLogger &          logger,
                         stream<DataMoverCmd> &tx_app_to_mover_write_cmd,
                         stream<NetAXIS> &     tx_app_to_mover_write_data) {
  DataMoverCmd cmd_out;
  NetAXISWord  data_out;

  while (!tx_app_to_mover_write_cmd.empty()) {
    tx_app_to_mover_write_cmd.read(cmd_out);
    logger.Info(TX_APP_IF, DATA_MVER, "Write CMD", cmd_out.to_string());
  }
  while (!tx_app_to_mover_write_data.empty()) {
    data_out = tx_app_to_mover_write_data.read();
    logger.Info(TX_APP_IF, DATA_MVER, "Write Data", data_out.to_string());
  }
}

void EmptyTxAppStsFifos(MockLogger &logger, stream<TxAppToTxSarReq> &tx_app_to_tx_sar_upd_req) {
  TxAppToTxSarReq to_tx_sar_req;
  while (!tx_app_to_tx_sar_upd_req.empty()) {
    tx_app_to_tx_sar_upd_req.read(to_tx_sar_req);
    logger.Info(TX_APP_IF, TX_SAR_TB, "Upd TX SAR", to_tx_sar_req.to_string());
  }
}

void EmptyTxAppTableFifos(MockLogger &                  logger,
                          stream<TxAppToTxAppTableReq> &tx_app_to_tx_app_table_req) {
  TxAppToTxAppTableReq to_app_table;
  while (!tx_app_to_tx_app_table_req.empty()) {
    tx_app_to_tx_app_table_req.read(to_app_table);
    logger.Info("Upd Tx APP table req", to_app_table.to_string());
  }
}
void TestTxAppConn() {
  // net app
  stream<NetAXISAppOpenConnReq>  net_app_to_tx_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>  tx_app_to_net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq> net_app_to_tx_app_close_conn_req;
  // passive open
  stream<NewClientNotificationNoTDEST> rx_eng_to_tx_app_new_client_notification;
  stream<NetAXISNewClientNotification> net_app_new_client_notification;
  // rx eng
  stream<OpenConnRspNoTDEST> rx_eng_to_tx_app_notification;
  // retrans timer
  stream<OpenConnRspNoTDEST> rtimer_to_tx_app_notification;
  // session lookup, also for TDEST
  stream<TxAppToSlookupReq> tx_app_to_slookup_req;
  stream<SessionLookupRsp>  slookup_to_tx_app_rsp;
  stream<TcpSessionID>      tx_app_to_slookup_check_tdest_req;
  stream<NetAXISDest>       slookup_to_tx_app_check_tdest_rsp;
  // port table req/rsp
  stream<NetAXISDest>   tx_app_to_ptable_req;
  stream<TcpPortNumber> ptable_to_tx_app_rsp;
  // state table read/write req/rsp
  stream<StateTableReq> tx_app_to_sttable_req;
  stream<SessionState>  sttable_to_tx_app_rsp;
  // event engine
  stream<Event> tx_app_conn_handler_to_event_engine;
  IpAddr        my_ip_addr;

  // open output file
  MockLogger conn_logger("tx_app_conn.log", TX_APP_IF);

  my_ip_addr = mock_src_ip_addr;
  NetAXISAppOpenConnReq net_app_req;

  int sim_cycle = 0;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        // multi app want to open connection
        net_app_req.data.ip_addr  = mock_dst_ip_addr;
        net_app_req.data.tcp_port = mock_dst_tcp_port;
        net_app_req.dest          = 0x1;
        net_app_to_tx_app_open_conn_req.write(net_app_req);

        net_app_req.data.ip_addr  = mock_dst_ip_addr;
        net_app_req.data.tcp_port = mock_dst_tcp_port + 0x0100;  // 32789+1
        net_app_req.dest          = 0x2;
        net_app_to_tx_app_open_conn_req.write(net_app_req);
        break;
      case 2:
        ptable_to_tx_app_rsp.write(0x80);
        break;
      case 3:
        ptable_to_tx_app_rsp.write(0x81);
        break;
      case 4:
        slookup_to_tx_app_rsp.write(SessionLookupRsp(0x1, true, 0x1));
        break;
      case 5:
        rx_eng_to_tx_app_notification.write(OpenConnRspNoTDEST(0x1, true));
        break;
      case 6:
        net_app_to_tx_app_close_conn_req.write(AppCloseConnReq(0x1, 0x1).to_net_axis());
        break;
      case 10:
        slookup_to_tx_app_check_tdest_rsp.write(0x1);
        break;
      default:
        break;
    }
    TxAppConnectionHandler(net_app_to_tx_app_open_conn_req,
                           tx_app_to_net_app_open_conn_rsp,
                           net_app_to_tx_app_close_conn_req,
                           rx_eng_to_tx_app_new_client_notification,
                           net_app_new_client_notification,
                           rx_eng_to_tx_app_notification,
                           rtimer_to_tx_app_notification,
                           tx_app_to_slookup_req,
                           slookup_to_tx_app_rsp,
                           tx_app_to_slookup_check_tdest_req,
                           slookup_to_tx_app_check_tdest_rsp,
                           tx_app_to_ptable_req,
                           ptable_to_tx_app_rsp,
                           tx_app_to_sttable_req,
                           sttable_to_tx_app_rsp,
                           tx_app_conn_handler_to_event_engine,
                           my_ip_addr);
    EmptyTxAppConnFifos(conn_logger,
                        tx_app_to_ptable_req,
                        tx_app_to_slookup_req,
                        tx_app_to_sttable_req,
                        tx_app_to_slookup_check_tdest_req,
                        tx_app_to_net_app_open_conn_rsp);
    sim_cycle++;
    conn_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
}

void TestTxAppData(stream<NetAXIS> &input_tcp_packets) {
  // net app
  stream<NetAXISAppTransDataReq> net_app_to_tx_app_trans_data_req;
  stream<NetAXISAppTransDataRsp> tx_app_to_net_app_trans_data_rsp;
  stream<NetAXIS>                net_app_trans_data;
  // tx app table
  stream<TxAppToTxAppTableReq> tx_app_to_tx_app_table_req;
  stream<TxAppTableToTxAppRsp> tx_app_table_to_tx_app_rsp;
  // state table
  stream<TcpSessionID> tx_app_to_sttable_lup_req;
  stream<SessionState> sttable_to_tx_app_lup_rsp;
  // to event eng
  stream<Event> tx_app_to_event_eng_set_event;
  // to datamover
  stream<MemBufferRWCmd> tx_app_to_mem_write_cmd_fifo("tx_app_to_mem_write_cmd_fifo");
  stream<NetAXISWord>    tx_app_to_mem_write_data_fifo("tx_app_to_mem_write_data_fifo");
  // to mem
  stream<DataMoverCmd> tx_app_to_mover_write_cmd("tx_app_to_mem_write_cmd_fifo");
  stream<NetAXIS>      tx_app_to_mover_write_data("tx_app_to_mem_write_data_fifo");
  // if mem access break down, write true to this fifo
  stream<ap_uint<1> > mem_buffer_double_access_flag;

  MockLogger data_logger("tx_app_data.log", TX_APP_IF);

  NetAXISAppTransDataReq net_app_req{};

  NetAXIS          cur_word{};
  TcpSessionBuffer cur_word_length = 0;
  // consume two words
  input_tcp_packets.read();
  input_tcp_packets.read();
  int sim_cycle = 0;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        // session 1 write 11FF Bytes
        net_app_req.data = 0x11FF;
        net_app_req.id   = 0x1;
        net_app_req.dest = 0xF;
        net_app_to_tx_app_trans_data_req.write(net_app_req);
        // session 2 write 12FF Bytes
        net_app_req.data = 0x12FF;
        net_app_req.id   = 0x2;
        net_app_req.dest = 0xF;
        net_app_to_tx_app_trans_data_req.write(net_app_req);
        // session 3 write 13FF Bytes
        net_app_req.data = 0x1234;
        net_app_req.id   = 0x3;
        net_app_req.dest = 0xD;
        net_app_to_tx_app_trans_data_req.write(net_app_req);
        break;
      case 2:
        // for session 1, write buffer is empty
        sttable_to_tx_app_lup_rsp.write(ESTABLISHED);
        tx_app_table_to_tx_app_rsp.write(TxAppTableToTxAppRsp(0x1, 0x0, 0x0));
        // session 2
        sttable_to_tx_app_lup_rsp.write(ESTABLISHED);
        tx_app_table_to_tx_app_rsp.write(TxAppTableToTxAppRsp(0x2, 0x0, 0xFFFFF));
        // session 3, for session 3 write buffer is full
        sttable_to_tx_app_lup_rsp.write(ESTABLISHED);
        tx_app_table_to_tx_app_rsp.write(TxAppTableToTxAppRsp(0x3, 0x0, 0xFFFFF));
        break;
      case 3:
        break;
      case 4:
        do {
          input_tcp_packets.read(cur_word);
          net_app_trans_data.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
        } while (cur_word.last != 1);
        break;
      case 5:
      case 6:

        do {
          input_tcp_packets.read(cur_word);
          net_app_trans_data.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
        } while (cur_word.last != 1);

      default:
        break;
    }
    TxAppDataHandler(net_app_to_tx_app_trans_data_req,
                     tx_app_to_net_app_trans_data_rsp,
                     net_app_trans_data,
                     tx_app_to_tx_app_table_req,
                     tx_app_table_to_tx_app_rsp,
                     tx_app_to_sttable_lup_req,
                     sttable_to_tx_app_lup_rsp,
                     tx_app_to_event_eng_set_event,
                     tx_app_to_mem_write_cmd_fifo,
                     tx_app_to_mem_write_data_fifo);
    WriteDataToMem<0>(tx_app_to_mem_write_cmd_fifo,
                      tx_app_to_mem_write_data_fifo,
                      tx_app_to_mover_write_cmd,
                      tx_app_to_mover_write_data,
                      mem_buffer_double_access_flag);
    EmptyTxAppDataFifos(data_logger,
                        tx_app_to_net_app_trans_data_rsp,
                        tx_app_to_sttable_lup_req,
                        tx_app_to_event_eng_set_event);
    EmptyTxAppTableFifos(data_logger, tx_app_to_tx_app_table_req);

    EmptyDataMoverFifos(data_logger, tx_app_to_mover_write_cmd, tx_app_to_mover_write_data);

    sim_cycle++;
    data_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
}

void TestTxAppIntf(stream<NetAXIS> &input_tcp_packets) {
  // some fifos
  // net app connection request
  stream<NetAXISAppOpenConnReq>  net_app_to_tx_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>  tx_app_to_net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq> net_app_to_tx_app_close_conn_req;
  // rx eng -> net app
  stream<NewClientNotificationNoTDEST> rx_eng_to_tx_app_new_client_notification;
  stream<NetAXISNewClientNotification> net_app_new_client_notification;
  // rx eng
  stream<OpenConnRspNoTDEST> rx_eng_to_tx_app_notification;
  // retrans timer
  stream<OpenConnRspNoTDEST> rtimer_to_tx_app_notification;
  // session lookup; also for TDEST
  stream<TxAppToSlookupReq> tx_app_to_slookup_req;
  stream<SessionLookupRsp>  slookup_to_tx_app_rsp;
  stream<TcpSessionID>      tx_app_to_slookup_check_tdest_req;
  stream<NetAXISDest>       slookup_to_tx_app_check_tdest_rsp;
  // port table req/rsp
  stream<NetAXISDest>   tx_app_to_ptable_req;
  stream<TcpPortNumber> ptable_to_tx_app_rsp;
  // state table read/write req/rsp
  stream<StateTableReq> tx_app_to_sttable_req;
  stream<SessionState>  sttable_to_tx_app_rsp;

  // net app data request
  stream<NetAXISAppTransDataReq> net_app_to_tx_app_trans_data_req;
  stream<NetAXISAppTransDataRsp> tx_app_to_net_app_trans_data_rsp;
  stream<NetAXIS>                net_app_trans_data;
  // to/from tx sar upd req
  stream<TxAppToTxSarReq> tx_app_to_tx_sar_upd_req;
  stream<TxSarToTxAppReq> tx_sar_to_tx_app_upd_req;
  // to event eng
  stream<Event> tx_app_to_event_eng_set_event;
  // state table
  stream<TcpSessionID> tx_app_to_sttable_lup_req;
  stream<SessionState> sttable_to_tx_app_lup_rsp;
  // datamover req/rsp
  stream<DataMoverCmd>    tx_app_to_mover_write_cmd;
  stream<NetAXIS>         tx_app_to_mover_write_data;
  stream<DataMoverStatus> mover_to_tx_app_write_status;
  // in big endian
  IpAddr my_ip_addr = mock_src_ip_addr;

  MockLogger top_logger("tx_app_intf.log", TX_APP_IF);

  AppOpenConnReq     open_conn_req;
  SessionLookupRsp   slup_ctrl_rsp;
  OpenConnRspNoTDEST open_conn_rsp;
  AppTransDataReq    trans_data_req;
  DataMoverStatus    data_mover_sts;
  NetAXIS            cur_word{};
  int                cur_packet = 0;

  int sim_cycle = 0;
  while (sim_cycle < 200) {
    switch (sim_cycle) {
      case 1:
        // role 1 open a conn
        open_conn_req.data = IpPortTuple(mock_dst_ip_addr, mock_dst_tcp_port);
        open_conn_req.dest = 0x1;
        net_app_to_tx_app_open_conn_req.write(open_conn_req.to_net_axis());
        // role 2 open a conn
        open_conn_req.data = IpPortTuple(mock_dst_ip_addr, mock_dst_tcp_port + 0x0100);
        open_conn_req.dest = 0x2;
        net_app_to_tx_app_open_conn_req.write(open_conn_req.to_net_axis());
        // port table return port = 80 LE
        ptable_to_tx_app_rsp.write(80);
        ptable_to_tx_app_rsp.write(81);

        break;
      case 2:
        // open session 1 success
        slup_ctrl_rsp.session_id = 0x1;
        slup_ctrl_rsp.hit        = true;
        slup_ctrl_rsp.role_id    = 0x1;
        slookup_to_tx_app_rsp.write(slup_ctrl_rsp);

        // open session 2 failed
        slup_ctrl_rsp.session_id = 0x2;
        slup_ctrl_rsp.hit        = false;
        slup_ctrl_rsp.role_id    = 0x2;
        slookup_to_tx_app_rsp.write(slup_ctrl_rsp);
        break;
      case 3:
        // rx engine receive SYN-ACK
        open_conn_rsp.session_id = 0x1;
        open_conn_rsp.success    = true;
        rx_eng_to_tx_app_notification.write(open_conn_rsp);
        // TDEST retrun 1
        slookup_to_tx_app_check_tdest_rsp.write(0x1);
        break;
      case 4:
        // role 1 wants to transfer data
        trans_data_req.data = 0x2b4;
        trans_data_req.id   = 0x1;
        trans_data_req.dest = 0x1;
        net_app_to_tx_app_trans_data_req.write(trans_data_req.to_net_axis());
        // role 2 wants to transfer data in the same time, should start when role 1 got finished
        trans_data_req.data = 0x2b4;
        trans_data_req.id   = 0x2;
        trans_data_req.dest = 0x2;
        net_app_to_tx_app_trans_data_req.write(trans_data_req.to_net_axis());

        break;
      case 5:
        sttable_to_tx_app_lup_rsp.write(ESTABLISHED);
        // read the 4th packet in input
        while (cur_packet < 3) {
          input_tcp_packets.read();
          cur_packet++;
        }
        do {
          input_tcp_packets.read(cur_word);
          net_app_trans_data.write(cur_word);
        } while (cur_word.last != 1);
        break;
      case 6:
        // role 2 failed to transfer data
        sttable_to_tx_app_lup_rsp.write(CLOSED);
        break;
      case 20:
        // datamover sts, it will update TX SAR app_written
        data_mover_sts.okay = 1;
        mover_to_tx_app_write_status.write(data_mover_sts);
        break;

      default:
        break;
    }
    tx_app_intf(net_app_to_tx_app_open_conn_req,
                tx_app_to_net_app_open_conn_rsp,
                net_app_to_tx_app_close_conn_req,
                rx_eng_to_tx_app_new_client_notification,
                net_app_new_client_notification,
                rx_eng_to_tx_app_notification,
                rtimer_to_tx_app_notification,
                tx_app_to_slookup_req,
                slookup_to_tx_app_rsp,
                tx_app_to_slookup_check_tdest_req,
                slookup_to_tx_app_check_tdest_rsp,
                tx_app_to_ptable_req,
                ptable_to_tx_app_rsp,
                tx_app_to_sttable_req,
                sttable_to_tx_app_rsp,
                net_app_to_tx_app_trans_data_req,
                tx_app_to_net_app_trans_data_rsp,
                net_app_trans_data,
                tx_app_to_tx_sar_upd_req,
                tx_sar_to_tx_app_upd_req,
                tx_app_to_event_eng_set_event,
                tx_app_to_sttable_lup_req,
                sttable_to_tx_app_lup_rsp,
                tx_app_to_mover_write_cmd,
                tx_app_to_mover_write_data,
                mover_to_tx_app_write_status,
                my_ip_addr);
    EmptyTxAppConnFifos(top_logger,
                        tx_app_to_ptable_req,
                        tx_app_to_slookup_req,
                        tx_app_to_sttable_req,
                        tx_app_to_slookup_check_tdest_req,
                        tx_app_to_net_app_open_conn_rsp);
    EmptyTxAppDataFifos(top_logger,
                        tx_app_to_net_app_trans_data_rsp,
                        tx_app_to_sttable_lup_req,
                        tx_app_to_event_eng_set_event);
    EmptyTxAppStsFifos(top_logger, tx_app_to_tx_sar_upd_req);

    EmptyDataMoverFifos(top_logger, tx_app_to_mover_write_cmd, tx_app_to_mover_write_data);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *          input_tcp_pcap_file = argv[1];
  stream<NetAXIS> input_tcp_packets("input_tcp_packets");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_packets);

  // TestTxAppConn();
  // TestTxAppData(input_tcp_packets);
  TestTxAppIntf(input_tcp_packets);

  return 0;
}