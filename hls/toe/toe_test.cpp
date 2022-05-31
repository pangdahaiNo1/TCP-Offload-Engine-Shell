#include "mock/mock_cam.hpp"
#include "mock/mock_memory.hpp"
#include "mock/mock_toe.hpp"
#include "toe/toe.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

void EmptyToeRxAppFifo(std::ofstream &out_stream,
                       // rx app
                       stream<NetAXISListenPortRsp> &  rx_app_to_net_app_listen_port_rsp,
                       stream<NetAXISAppReadRsp> &     net_app_read_data_rsp,
                       stream<NetAXISAppNotification> &net_app_notification,
                       int                             sim_cycle) {
  ListenPortRsp   listen_port_rsp;
  AppReadRsp      read_data_rsp;
  AppNotification app_notify;
  while (!(rx_app_to_net_app_listen_port_rsp.empty())) {
    listen_port_rsp = rx_app_to_net_app_listen_port_rsp.read();
    out_stream << "Cycle " << std::dec << sim_cycle << ": TOE to Rx APP ListenPort Rsp\t";
    out_stream << listen_port_rsp.to_string() << "\n";
  }

  while (!(net_app_read_data_rsp.empty())) {
    read_data_rsp = net_app_read_data_rsp.read();
    out_stream << "Cycle " << std::dec << sim_cycle << ": TOE to Rx APP Readdata Rsp\t";
    out_stream << read_data_rsp.to_string() << "\n";
  }
  while (!(net_app_notification.empty())) {
    app_notify = net_app_notification.read();
    out_stream << "Cycle " << std::dec << sim_cycle << ": TOE to Rx APP notify\t";
    out_stream << app_notify.to_string() << "\n";
  }
}

void EmptyToeTxAppFifo(std::ofstream &out_stream,
                       // tx app
                       stream<NetAXISAppOpenConnRsp> &       tx_app_to_net_app_open_conn_rsp,
                       stream<NetAXISNewClientNotification> &net_app_new_client_notification,
                       stream<NetAXISAppTransDataRsp> &      tx_app_to_net_app_tans_data_rsp,
                       int                                   sim_cycle) {
  AppOpenConnRsp        open_conn_rsp;
  NewClientNotification new_client_notify;
  AppTransDataRsp       trans_data_rsp;
  while (!(tx_app_to_net_app_open_conn_rsp.empty())) {
    open_conn_rsp = tx_app_to_net_app_open_conn_rsp.read();
    out_stream << "Cycle " << std::dec << sim_cycle << ": TOE to Tx APP OpenConn Rsp\t";
    out_stream << open_conn_rsp.to_string() << "\n";
  }

  while (!(net_app_new_client_notification.empty())) {
    new_client_notify = net_app_new_client_notification.read();
    out_stream << "Cycle " << std::dec << sim_cycle << ": TOE to Tx App NewClient Rsp\t";
    out_stream << new_client_notify.to_string() << "\n";
  }
  while (!(tx_app_to_net_app_tans_data_rsp.empty())) {
    trans_data_rsp = tx_app_to_net_app_tans_data_rsp.read();
    out_stream << "Cycle " << std::dec << sim_cycle << ": TOE to Tx TransData Rsp\t";
    out_stream << trans_data_rsp.to_string() << "\n";
  }
}

void TestToe(stream<NetAXIS> &input_tcp_packet) {
  // rx engine
  // tx engine
  stream<DataMoverCmd> mover_read_mem_cmd_out;
  stream<NetAXIS>      mover_read_mem_data_in;
  stream<NetAXIS>      tx_app_to_tx_eng_data;
  stream<NetAXIS>      tx_ip_pkt_out;
  // rx app
  stream<NetAXISListenPortReq>   net_app_to_rx_app_listen_port_req;
  stream<NetAXISListenPortRsp>   rx_app_to_net_app_listen_port_rsp;
  stream<NetAXISAppReadReq>      net_app_read_data_req;
  stream<NetAXISAppReadRsp>      net_app_read_data_rsp;
  stream<NetAXIS>                rx_app_to_net_app_data;
  stream<NetAXISAppNotification> net_app_notification;
  // tx app
  stream<NetAXISAppOpenConnReq>        net_app_to_tx_app_open_conn_req;
  stream<NetAXISAppOpenConnRsp>        tx_app_to_net_app_open_conn_rsp;
  stream<NetAXISAppCloseConnReq>       net_app_to_tx_app_close_conn_req;
  stream<NetAXISNewClientNotification> net_app_new_client_notification;
  stream<NetAXISAppTransDataReq>       net_app_to_tx_app_trans_data_req;
  stream<NetAXISAppTransDataRsp>       tx_app_to_net_app_tans_data_rsp;
  stream<NetAXIS>                      net_app_trans_data;
  stream<DataMoverCmd>                 tx_app_to_mem_write_cmd;
  stream<NetAXIS>                      tx_app_to_mem_write_data;
  stream<DataMoverStatus>              mem_to_tx_app_write_status;
  // CAM
  stream<RtlSLookupToCamLupReq> rtl_slookup_to_cam_lookup_req;
  stream<RtlCamToSlookupLupRsp> rtl_cam_to_slookup_lookup_rsp;
  stream<RtlSlookupToCamUpdReq> rtl_slookup_to_cam_update_req;
  stream<RtlCamToSlookupUpdRsp> rtl_cam_to_slookup_update_rsp;
  // registers
  ap_uint<16> reg_session_cnt;
  // in big endian
  IpAddr my_ip_addr = mock_src_ip_addr;

  // mock cam
  MockCam mock_cam;
  // mock mem
  MockMem mock_mem;

  std::ofstream outputFile;

  outputFile.open("./out.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }
  // test variblaes
  ListenPortReq listen_req;

  int sim_cycle = 0;
  while (sim_cycle < 1000) {
    switch (sim_cycle) {
      case 1:
        listen_req.data = 0x80;
        listen_req.dest = 0x0;
        net_app_to_rx_app_listen_port_req.write(listen_req.to_net_axis());
        break;
      case 2:
        listen_req.data = 0x81;
        listen_req.dest = 0x1;
        net_app_to_rx_app_listen_port_req.write(listen_req.to_net_axis());
        break;

      default:
        break;
    }
    toe_top(input_tcp_packet,
            mover_read_mem_cmd_out,
            mover_read_mem_data_in,
            tx_ip_pkt_out,
            net_app_to_rx_app_listen_port_req,
            rx_app_to_net_app_listen_port_rsp,
            net_app_read_data_req,
            net_app_read_data_rsp,
            rx_app_to_net_app_data,
            net_app_notification,
            net_app_to_tx_app_open_conn_req,
            tx_app_to_net_app_open_conn_rsp,
            net_app_to_tx_app_close_conn_req,
            net_app_new_client_notification,
            net_app_to_tx_app_trans_data_req,
            tx_app_to_net_app_tans_data_rsp,
            net_app_trans_data,
            tx_app_to_mem_write_cmd,
            tx_app_to_mem_write_data,
            mem_to_tx_app_write_status,
            rtl_slookup_to_cam_lookup_req,
            rtl_cam_to_slookup_lookup_rsp,
            rtl_slookup_to_cam_update_req,
            rtl_cam_to_slookup_update_rsp,
            reg_session_cnt,
            my_ip_addr);
    mock_cam.MockCamIntf(outputFile,
                         rtl_slookup_to_cam_lookup_req,
                         rtl_cam_to_slookup_lookup_rsp,
                         rtl_slookup_to_cam_update_req,
                         rtl_cam_to_slookup_update_rsp,
                         sim_cycle);
    mock_mem.MockMemIntf(outputFile,
                         mover_read_mem_cmd_out,
                         mover_read_mem_data_in,
                         tx_app_to_mem_write_cmd,
                         tx_app_to_mem_write_data,
                         mem_to_tx_app_write_status,
                         sim_cycle);
    EmptyToeRxAppFifo(outputFile,
                      rx_app_to_net_app_listen_port_rsp,
                      net_app_read_data_rsp,
                      net_app_notification,
                      sim_cycle);
    EmptyToeTxAppFifo(outputFile,
                      tx_app_to_net_app_open_conn_rsp,
                      net_app_new_client_notification,
                      tx_app_to_net_app_tans_data_rsp,
                      sim_cycle);

    sim_cycle++;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *input_tcp_pcap_file = argv[1];
  stream<NetAXIS> input_tcp_ip_pkt_read_in("input_tcp_ip_pkt_read_in");
  // PcapToStream(input_tcp_pcap_file, true, input_tcp_ip_pkt_read_in);
  TestToe(input_tcp_ip_pkt_read_in);
  return 0;
}