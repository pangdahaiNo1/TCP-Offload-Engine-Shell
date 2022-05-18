#include "toe/mock/mock_toe.hpp"
#include "tx_app_intf.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace hls;

void EmptyTxAppConnFifos(std::ofstream &                out_stream,
                         stream<NetAXISDest> &          tx_app_to_ptable_req,
                         stream<TxAppToSlookupReq> &    tx_app_to_slookup_req,
                         stream<StateTableReq> &        tx_app_to_sttable_req,
                         stream<TcpSessionID> &         tx_app_to_slookup_check_tdest_req,
                         stream<NetAXISAppOpenConnRsp> &tx_app_to_net_app_open_conn_rsp,
                         int                            sim_cycle) {
  NetAXISDest           role_id;
  TxAppToSlookupReq     to_slookup_req;
  StateTableReq         to_sttable_req;
  TcpSessionID          to_slookup_checktdest;
  NetAXISAppOpenConnRsp to_net_app_conn_rsp;

  while (!tx_app_to_ptable_req.empty()) {
    tx_app_to_ptable_req.read(role_id);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App to Porttable Req, RoelID: ";
    out_stream << role_id.to_string(16) << "\n";
  }
  while (!tx_app_to_slookup_req.empty()) {
    tx_app_to_slookup_req.read(to_slookup_req);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App to Slookup Req\t";
    out_stream << to_slookup_req.to_string() << "\n";
  }
  while (!tx_app_to_sttable_req.empty()) {
    tx_app_to_sttable_req.read(to_sttable_req);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App to State Table \t";
    out_stream << to_sttable_req.to_string() << "\n";
  }
  while (!tx_app_to_slookup_check_tdest_req.empty()) {
    tx_app_to_slookup_check_tdest_req.read(to_slookup_checktdest);
    out_stream << "Cycle " << std::dec << sim_cycle << ":Tx App to Slookup Check TDEST Req \t";
    out_stream << to_slookup_checktdest.to_string(16) << "\n";
  }
  while (!tx_app_to_net_app_open_conn_rsp.empty()) {
    tx_app_to_net_app_open_conn_rsp.read(to_net_app_conn_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App to Net APP Conn Rsp \t";
    out_stream << to_net_app_conn_rsp.to_string() << "\n";
  }
}

void EmptyTxAppDataFifos(std::ofstream &                 out_stream,
                         stream<NetAXISAppTransDataRsp> &tx_app_to_net_app_tans_data_rsp,
                         stream<TxAppToTxAppTableReq> &  tx_app_to_tx_app_table_req,
                         stream<TcpSessionID> &          tx_app_to_sttable_lup_req,
                         stream<Event> &                 tx_app_to_event_eng_set_event,
                         int                             sim_cycle) {
  NetAXISAppTransDataRsp to_net_app_rsp;
  TxAppToTxAppTableReq   to_app_table;
  TcpSessionID           to_sttable;
  Event                  to_event_engine;
  while (!tx_app_to_net_app_tans_data_rsp.empty()) {
    tx_app_to_net_app_tans_data_rsp.read(to_net_app_rsp);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App to NetAPP Trans Data Rsp: ";
    out_stream << to_net_app_rsp.to_string() << "\n";
  }
  while (!tx_app_to_tx_app_table_req.empty()) {
    tx_app_to_tx_app_table_req.read(to_app_table);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App to Tx APP Table Req\t";
    out_stream << to_app_table.to_string() << "\n";
  }
  while (!tx_app_to_sttable_lup_req.empty()) {
    tx_app_to_sttable_lup_req.read(to_sttable);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App to State Table SessionID: \t";
    out_stream << to_sttable.to_string(16) << "\n";
  }
  while (!tx_app_to_event_eng_set_event.empty()) {
    tx_app_to_event_eng_set_event.read(to_event_engine);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App to Event eng set Event: \t";
    out_stream << to_event_engine.to_string() << "\n";
  }
}

void EmptyDataMoverFifos(std::ofstream &       out_stream,
                         stream<DataMoverCmd> &tx_app_to_mem_write_cmd,
                         stream<NetAXIS> &     tx_app_to_mem_write_data,
                         int                   sim_cycle) {
  DataMoverCmd cmd_out;
  NetAXIS      data_out;

  while (!tx_app_to_mem_write_cmd.empty()) {
    tx_app_to_mem_write_cmd.read(cmd_out);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App write cmd: \t";
    out_stream << cmd_out.to_string() << "\n";
  }
  while (!tx_app_to_mem_write_data.empty()) {
    tx_app_to_mem_write_data.read(data_out);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App write \t";
    out_stream << dec << KeepToLength(data_out.keep) << " Bytes word to mem\n";
  }
}

void TestTxAppConn() {
  // net app
  stream<NetAXISAppOpenConnReq>  net_app_to_tx_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>  tx_app_to_net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq> net_app_to_tx_app_close_conn_req;
  // passive open
  stream<NewClientNotification>       rx_eng_to_tx_app_new_client_notification;
  stream<NetAXISNewClientNotificaion> net_app_new_client_notification;
  // rx eng
  stream<OpenSessionStatus> rx_eng_to_tx_app_notification;
  // retrans timer
  stream<OpenSessionStatus> rtimer_to_tx_app_notification;
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
  std::ofstream outputFile;

  outputFile.open("./out_conn_handler.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }

  my_ip_addr = mock_src_ip_addr;
  NetAXISAppOpenConnReq net_app_req;
  int                   sim_cycle = 0;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        // multi app want to open connection
        net_app_req.data.ip_addr  = mock_dst_ip_addr;
        net_app_req.data.tcp_port = mock_dst_tcp_port;
        net_app_req.dest          = 0x1;
        net_app_to_tx_app_open_conn_req.write(net_app_req);

        net_app_req.data.ip_addr  = mock_dst_ip_addr;
        net_app_req.data.tcp_port = mock_dst_tcp_port + 0x01;  // 32789+1
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
        rx_eng_to_tx_app_notification.write(OpenSessionStatus(0x1, true));
        slookup_to_tx_app_check_tdest_rsp.write(0x2);
        break;
      case 6:
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
    EmptyTxAppConnFifos(outputFile,
                        tx_app_to_ptable_req,
                        tx_app_to_slookup_req,
                        tx_app_to_sttable_req,
                        tx_app_to_slookup_check_tdest_req,
                        tx_app_to_net_app_open_conn_rsp,
                        sim_cycle);
    sim_cycle++;
  }
}

void TestTxAppData(stream<NetAXIS> &input_tcp_packets) {
  // net app
  stream<NetAXISAppTransDataReq> net_app_to_tx_app_trans_data_req;
  stream<NetAXISAppTransDataRsp> tx_app_to_net_app_tans_data_rsp;
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
  stream<DataMoverCmd> tx_app_to_mem_write_cmd;
  stream<NetAXIS>      tx_app_to_mem_write_data;

  // open output file
  std::ofstream outputFile;

  outputFile.open("./out_data_handler.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }
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
                     tx_app_to_net_app_tans_data_rsp,
                     net_app_trans_data,
                     tx_app_to_tx_app_table_req,
                     tx_app_table_to_tx_app_rsp,
                     tx_app_to_sttable_lup_req,
                     sttable_to_tx_app_lup_rsp,
                     tx_app_to_event_eng_set_event,
                     tx_app_to_mem_write_cmd,
                     tx_app_to_mem_write_data);
    EmptyTxAppDataFifos(outputFile,
                        tx_app_to_net_app_tans_data_rsp,
                        tx_app_to_tx_app_table_req,
                        tx_app_to_sttable_lup_req,
                        tx_app_to_event_eng_set_event,
                        sim_cycle);

    EmptyDataMoverFifos(outputFile, tx_app_to_mem_write_cmd, tx_app_to_mem_write_data, sim_cycle);

    sim_cycle++;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *input_tcp_pcap_file = argv[1];
  cout << "Read TCP Packets from " << input_tcp_pcap_file << endl;
  stream<NetAXIS> input_tcp_packets("input_tcp_packets");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_packets);

  TestTxAppConn();
  TestTxAppData(input_tcp_packets);

  return 0;
}