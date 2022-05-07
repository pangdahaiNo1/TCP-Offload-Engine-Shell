#include "session_lookup_controller.hpp"
#include "toe/mock/mock_toe.hpp"

#include <map>

using namespace hls;
using namespace std;

static std::map<ThreeTuple, TcpSessionID> mock_cam;
void MockCam(stream<RtlSLookupToCamLupReq> &slookup_to_cam_lup_req,
             stream<RtlCamToSlookupLupRsp> &cam_to_slookup_lup_rsp,
             stream<RtlSlookupToCamUpdReq> &slookup_to_cam_upd_req,
             stream<RtlCamToSlookupUpdRsp> &cam_to_slookup_upd_rsp) {

  RtlSLookupToCamLupReq lookup_req;
  RtlSlookupToCamUpdReq upddate_req;

  std::map<ThreeTuple, TcpSessionID>::const_iterator iter;

  if (!slookup_to_cam_lup_req.empty()) {
    slookup_to_cam_lup_req.read(lookup_req);
    iter = mock_cam.find(lookup_req.key);
    if (iter != mock_cam.end())  // hit
    {
      cout << "Current lookup session: "<< iter->second << endl;
      cam_to_slookup_lup_rsp.write(RtlCamToSlookupLupRsp(true, iter->second, lookup_req.source));
    } else {
      cam_to_slookup_lup_rsp.write(RtlCamToSlookupLupRsp(false, lookup_req.source));
    }
  }

  if (!slookup_to_cam_upd_req.empty()) {
    slookup_to_cam_upd_req.read(upddate_req);
    iter = mock_cam.find(upddate_req.key);
    if (iter == mock_cam.end()) {
      if (upddate_req.op == INSERT) {
        mock_cam[upddate_req.key] = upddate_req.value;
        cam_to_slookup_upd_rsp.write(
            RtlCamToSlookupUpdRsp(upddate_req.value, INSERT, upddate_req.source));
        cout << "Current Update session: "<< upddate_req.value << endl;

      } else {
        cerr << "delete a non-existing one" << endl;
      }
    } else {  // found in map
      if (upddate_req.op == INSERT) {
        cerr << "insert a existing one" << endl;
      } else {
        mock_cam.erase(upddate_req.key);
        cam_to_slookup_upd_rsp.write(
            RtlCamToSlookupUpdRsp(upddate_req.value, DELETE, upddate_req.source));
      }
    }
  }
}

void CamUpdateReqMerger(stream<RtlSlookupToCamUpdReq> &in1,
                        stream<RtlSlookupToCamUpdReq> &in2,
                        stream<RtlSlookupToCamUpdReq> &out) {
  if (!in1.empty()) {
    out.write(in1.read());
  } else if (!in2.empty()) {
    out.write(in2.read());
  }
}

void EmptyFifos(std::ofstream &                 out_stream,
                stream<NetAXISDest> &           slookup_to_rx_app_check_tdset_rsp,
                stream<SessionLookupRsp> &      slookup_to_rx_eng_rsp,
                stream<SessionLookupRsp> &      slookup_to_tx_app_rsp,
                stream<NetAXISDest> &           slookup_to_tx_app_check_tdest_rsp,
                stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
                stream<TcpPortNumber> &         slookup_to_ptable_release_port_req,
                int                             sim_cycle) {
  NetAXISDest            rx_or_tx_app_tdest;
  SessionLookupRsp       slookup_rsp;
  ReverseTableToTxEngRsp tx_eng_rsp;
  while (!slookup_to_rx_app_check_tdset_rsp.empty()) {
    slookup_to_rx_app_check_tdset_rsp.read(rx_or_tx_app_tdest);
    out_stream << "Cycle " << sim_cycle << ": Slookup to Rx app TDEST rsp\t";
    out_stream << dec << rx_or_tx_app_tdest << "\n";
  }
  while (!slookup_to_rx_eng_rsp.empty()) {
    slookup_to_rx_eng_rsp.read(slookup_rsp);
    out_stream << "Cycle " << sim_cycle << ": Slookup to Rx engine rsp\t";
    out_stream << slookup_rsp.to_string() << "\n";
  }
  while (!slookup_to_tx_app_rsp.empty()) {
    slookup_to_tx_app_rsp.read(slookup_rsp);
    out_stream << "Cycle " << sim_cycle << ": Slookup to Tx APP rsp\t";
    out_stream << slookup_rsp.to_string() << "\n";
  }
  while (!slookup_to_tx_app_check_tdest_rsp.empty()) {
    slookup_to_tx_app_check_tdest_rsp.read(rx_or_tx_app_tdest);
    out_stream << "Cycle " << sim_cycle << ": Slookup to Tx App TDEST rsp\t";
    out_stream << dec << rx_or_tx_app_tdest << "\n";
  }
  while (!slookup_rev_table_to_tx_eng_rsp.empty()) {
    slookup_rev_table_to_tx_eng_rsp.read(tx_eng_rsp);
    out_stream << "Cycle " << sim_cycle << ": Slookup to Tx App TDEST rsp\t";
    out_stream << tx_eng_rsp.to_string() << "\n";
  }
}

int main() {
  // from sttable
  stream<TcpSessionID> sttable_to_slookup_release_session_req;
  // rx app
  stream<TcpSessionID> rx_app_to_slookup_check_tdest_req;
  stream<NetAXISDest>  slookup_to_rx_app_check_tdset_rsp;
  // rx eng
  stream<RxEngToSlookupReq> rx_eng_to_slookup_req;
  stream<SessionLookupRsp>  slookup_to_rx_eng_rsp;
  // tx app
  stream<TxAppToSlookupReq> tx_app_to_slookup_req;
  stream<SessionLookupRsp>  slookup_to_tx_app_rsp;
  stream<TcpSessionID>      tx_app_to_slookup_check_tdest_req;
  stream<NetAXISDest>       slookup_to_tx_app_check_tdest_rsp;
  // tx eng
  stream<ap_uint<16> >           tx_eng_to_slookup_rev_table_req;
  stream<ReverseTableToTxEngRsp> slookup_rev_table_to_tx_eng_rsp;
  // CAM
  stream<RtlSLookupToCamLupReq> rtl_slookup_to_cam_lookup_req;
  stream<RtlCamToSlookupLupRsp> rtl_cam_to_slookup_lookup_rsp;
  stream<RtlSlookupToCamUpdReq> rtl_slookup_to_cam_update_req;
  stream<RtlCamToSlookupUpdRsp> rtl_cam_to_slookup_update_rsp;
  // to ptable
  stream<TcpPortNumber> slookup_to_ptable_release_port_req;
  // registers
  ap_uint<16> reg_session_cnt;
  IpAddr      my_ip_addr;

  std::ofstream outputFile;

  outputFile.open("./out.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }
  int sim_cycle = 0;

  TxAppToSlookupReq tx_app_req;
  RxEngToSlookupReq rx_eng_req;
  TcpSessionID      session_id;
  while (sim_cycle < 100) {
    switch (sim_cycle) {
      case 1:
        tx_app_req.four_tuple = mock_tuple;
        tx_app_req.role_id    = 0x1;
        tx_app_to_slookup_req.write(tx_app_req);
        break;
      case 2:
        rx_eng_req.four_tuple     = reverser_mock_tuple;
        rx_eng_req.role_id        = 0x3;
        rx_eng_req.allow_creation = false;
        rx_eng_to_slookup_req.write(rx_eng_req);
        break;
      case 3:
        break;
      case 4:
        rx_eng_req.allow_creation = true;
        rx_eng_req.four_tuple     = mock_tuple; // is the another key
        rx_eng_req.role_id        = 0x3;
        rx_eng_to_slookup_req.write(rx_eng_req);
        tx_app_to_slookup_check_tdest_req.write(0);

        break;
      case 5:
        break;
      case 13:
        tx_app_to_slookup_check_tdest_req.write(0);
        break;
      default:
        break;
    }
    session_lookup_controller(sttable_to_slookup_release_session_req,
                              rx_app_to_slookup_check_tdest_req,
                              slookup_to_rx_app_check_tdset_rsp,
                              rx_eng_to_slookup_req,
                              slookup_to_rx_eng_rsp,
                              tx_app_to_slookup_req,
                              slookup_to_tx_app_rsp,
                              tx_app_to_slookup_check_tdest_req,
                              slookup_to_tx_app_check_tdest_rsp,
                              tx_eng_to_slookup_rev_table_req,
                              slookup_rev_table_to_tx_eng_rsp,
                              rtl_slookup_to_cam_lookup_req,
                              rtl_cam_to_slookup_lookup_rsp,
                              rtl_slookup_to_cam_update_req,
                              rtl_cam_to_slookup_update_rsp,
                              slookup_to_ptable_release_port_req,
                              reg_session_cnt,
                              my_ip_addr);
    MockCam(rtl_slookup_to_cam_lookup_req,
            rtl_cam_to_slookup_lookup_rsp,
            rtl_slookup_to_cam_update_req,
            rtl_cam_to_slookup_update_rsp);
    EmptyFifos(outputFile,
               slookup_to_rx_app_check_tdset_rsp,
               slookup_to_rx_eng_rsp,
               slookup_to_tx_app_rsp,
               slookup_to_tx_app_check_tdest_rsp,
               slookup_rev_table_to_tx_eng_rsp,
               slookup_to_ptable_release_port_req,
               sim_cycle);
    sim_cycle++;
  }

  return 0;
}
