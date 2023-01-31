#ifndef _IPERF2_HPP_
#define _IPERF2_HPP_
#include "toe/toe_conn.hpp"
using namespace hls;

struct IperfRegs {
  ap_uint<1>  run_experiment;
  ap_uint<1>  dual_mode_en;
  ap_uint<1>  use_timer;
  ap_uint<64> run_time;
  ap_uint<14> num_connections;
  ap_uint<32> transfer_size;
  ap_uint<16> packet_mss;
  // in big endian
  ap_uint<32> dst_ip;
  ap_uint<16> dst_port;
  ap_uint<16> max_connections;
  ap_uint<16> current_state;
  ap_uint<1>  error_openning_connection;
  IperfRegs() {
    run_experiment            = 0;
    dual_mode_en              = 0;
    use_timer                 = 0;
    run_time                  = 0;
    num_connections           = 0;
    transfer_size             = 0;
    packet_mss                = 0;
    dst_ip                    = 0;
    dst_port                  = 0;
    max_connections           = 0;
    current_state             = 0;
    error_openning_connection = 0;
  }
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "[IPERF_REGS] Run Experiment? " << (run_experiment ? "Yes" : "No") << "|\t"
            << "Enable Dual mode? " << (dual_mode_en ? "Yes" : "No") << "|\t"
            << "Use Timer? " << (use_timer ? "Yes" : "No") << "|\t"
            << "|Run time " << run_time << " |Number of conn " << num_connections
            << "|Transfer size " << transfer_size << " |Packet MSS " << packet_mss
            << "|Dst IP:Port " << SwapByte(dst_ip) << ":" << SwapByte(dst_port) << "\t"
            << "|Max Conn " << max_connections;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

void iperf2(
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
    stream<NetAXIS>                &net_app_trans_data,
    // tdest constant
    NetAXISDest &tdest_const);
#endif
