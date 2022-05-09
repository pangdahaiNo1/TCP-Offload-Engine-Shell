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

void TestTxAppConn() {
  // net app
  stream<NetAXISAppOpenConnReq>  net_app_to_tx_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>  tx_app_to_net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq> net_app_to_tx_app_close_conn_req;
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
        slookup_to_tx_app_check_tdest_rsp.write(0x1);
        break;
      case 6:
        break;
      default:
        break;
    }
    TxAppConnectionHandler(net_app_to_tx_app_open_conn_req,
                           tx_app_to_net_app_open_conn_rsp,
                           net_app_to_tx_app_close_conn_req,
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

int main(int argc, char **argv) {
  TestTxAppConn();

  return 0;
}