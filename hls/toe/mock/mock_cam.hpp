#ifndef _MOCK_CAM_HPP_
#define _MOCK_CAM_HPP_
#include "toe/toe_conn.hpp"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <map>
using hls::stream;
using namespace std;

class MockCam {
public:
  map<ThreeTuple, TcpSessionID> _mock_cam;
  MockCam() { _mock_cam = map<ThreeTuple, TcpSessionID>(); }
  void MockCamIntf(std::ofstream &                out_stream,
                   stream<RtlSLookupToCamLupReq> &slookup_to_cam_lup_req,
                   stream<RtlCamToSlookupLupRsp> &cam_to_slookup_lup_rsp,
                   stream<RtlSlookupToCamUpdReq> &slookup_to_cam_upd_req,
                   stream<RtlCamToSlookupUpdRsp> &cam_to_slookup_upd_rsp,
                   int                            sim_cycle) {
    RtlSLookupToCamLupReq lookup_req;
    RtlCamToSlookupLupRsp lookup_rsp;
    RtlSlookupToCamUpdReq update_req;
    RtlCamToSlookupUpdRsp update_rsp;

    std::map<ThreeTuple, TcpSessionID>::const_iterator iter;

    if (!slookup_to_cam_lup_req.empty()) {
      slookup_to_cam_lup_req.read(lookup_req);
      out_stream << "Cycle " << std::dec << sim_cycle << ": Slookup to CAM Lookup Req\t";
      out_stream << lookup_req.to_string() << "\n";

      iter = _mock_cam.find(lookup_req.key);
      if (iter != _mock_cam.end())  // hit
      {
        lookup_rsp = RtlCamToSlookupLupRsp(true, iter->second, lookup_req.source);
      } else {
        lookup_rsp = RtlCamToSlookupLupRsp(false, lookup_req.source);
      }
      cam_to_slookup_lup_rsp.write(lookup_rsp);
      out_stream << "Cycle " << std::dec << sim_cycle << ": CAM to Slookup Lookup Rsp\t";
      out_stream << lookup_rsp.to_string() << "\n";
    }

    if (!slookup_to_cam_upd_req.empty()) {
      slookup_to_cam_upd_req.read(update_req);

      out_stream << "Cycle " << std::dec << sim_cycle << ": Slookup to CAM Update Req\t";
      out_stream << update_req.to_string() << "\n";

      iter = _mock_cam.find(update_req.key);
      if (iter == _mock_cam.end()) {
        if (update_req.op == INSERT) {
          _mock_cam[update_req.key] = update_req.value;
          update_rsp = RtlCamToSlookupUpdRsp(update_req.value, INSERT, update_req.source);
          cout << "Current Update session: " << update_req.value << endl;

        } else {
          cerr << "delete a non-existing one" << endl;
        }
      } else {  // found in map
        if (update_req.op == INSERT) {
          cerr << "insert a existing one" << endl;
        } else {
          _mock_cam.erase(update_req.key);
          update_rsp = RtlCamToSlookupUpdRsp(update_req.value, DELETE, update_req.source);
        }
      }
      cam_to_slookup_upd_rsp.write(update_rsp);
      out_stream << "Cycle " << std::dec << sim_cycle << ": CAM to Slookup Update Rsp\t";
      out_stream << update_rsp.to_string() << "\n";
    }
  }
};

#endif  // _MOCK_CAM_HPP_