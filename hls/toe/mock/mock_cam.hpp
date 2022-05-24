#ifndef _MOCK_CAM_HPP_
#define _MOCK_CAM_HPP_
#include "toe/toe_conn.hpp"

#include <algorithm>
#include <map>
using hls::stream;
using namespace std;

class MockCam {
public:
  map<ThreeTuple, TcpSessionID> _mock_cam;
  MockCam() { _mock_cam = map<ThreeTuple, TcpSessionID>(); }
  void MockCamIntf(stream<RtlSLookupToCamLupReq> &slookup_to_cam_lup_req,
                   stream<RtlCamToSlookupLupRsp> &cam_to_slookup_lup_rsp,
                   stream<RtlSlookupToCamUpdReq> &slookup_to_cam_upd_req,
                   stream<RtlCamToSlookupUpdRsp> &cam_to_slookup_upd_rsp) {
    RtlSLookupToCamLupReq lookup_req;
    RtlSlookupToCamUpdReq upddate_req;

    std::map<ThreeTuple, TcpSessionID>::const_iterator iter;

    if (!slookup_to_cam_lup_req.empty()) {
      slookup_to_cam_lup_req.read(lookup_req);
      iter = _mock_cam.find(lookup_req.key);
      if (iter != _mock_cam.end())  // hit
      {
        cout << "Current lookup session: " << iter->second << endl;
        cam_to_slookup_lup_rsp.write(RtlCamToSlookupLupRsp(true, iter->second, lookup_req.source));
      } else {
        cam_to_slookup_lup_rsp.write(RtlCamToSlookupLupRsp(false, lookup_req.source));
      }
    }

    if (!slookup_to_cam_upd_req.empty()) {
      slookup_to_cam_upd_req.read(upddate_req);
      iter = _mock_cam.find(upddate_req.key);
      if (iter == _mock_cam.end()) {
        if (upddate_req.op == INSERT) {
          _mock_cam[upddate_req.key] = upddate_req.value;
          cam_to_slookup_upd_rsp.write(
              RtlCamToSlookupUpdRsp(upddate_req.value, INSERT, upddate_req.source));
          cout << "Current Update session: " << upddate_req.value << endl;

        } else {
          cerr << "delete a non-existing one" << endl;
        }
      } else {  // found in map
        if (upddate_req.op == INSERT) {
          cerr << "insert a existing one" << endl;
        } else {
          _mock_cam.erase(upddate_req.key);
          cam_to_slookup_upd_rsp.write(
              RtlCamToSlookupUpdRsp(upddate_req.value, DELETE, upddate_req.source));
        }
      }
    }
  }
};

#endif  // _MOCK_CAM_HPP_