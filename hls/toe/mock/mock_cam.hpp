#ifndef _MOCK_CAM_HPP_
#define _MOCK_CAM_HPP_
#include "toe/mock/mock_logger.hpp"
#include "toe/toe_conn.hpp"

#include <map>

using hls::stream;
using std::cerr;
using std::endl;
using std::map;
class MockCam {
public:
  std::map<ThreeTuple, TcpSessionID> _mock_cam;
  MockCam() { _mock_cam = std::map<ThreeTuple, TcpSessionID>(); }
  void MockCamIntf(MockLogger &                   logger,
                   stream<RtlSLookupToCamLupReq> &slookup_to_cam_lup_req,
                   stream<RtlCamToSlookupLupRsp> &cam_to_slookup_lup_rsp,
                   stream<RtlSlookupToCamUpdReq> &slookup_to_cam_upd_req,
                   stream<RtlCamToSlookupUpdRsp> &cam_to_slookup_upd_rsp) {
    RtlSLookupToCamLupReq lookup_req;
    RtlCamToSlookupLupRsp lookup_rsp;
    RtlSlookupToCamUpdReq update_req;
    RtlCamToSlookupUpdRsp update_rsp;

    std::map<ThreeTuple, TcpSessionID>::const_iterator iter;

    if (!slookup_to_cam_lup_req.empty()) {
      slookup_to_cam_lup_req.read(lookup_req);
      logger.Info("Slookup to CAM Lookup Req", lookup_req.to_string(), false);

      iter = _mock_cam.find(lookup_req.key);
      if (iter != _mock_cam.end())  // hit
      {
        lookup_rsp = RtlCamToSlookupLupRsp(true, iter->second, lookup_req.source);
      } else {
        lookup_rsp = RtlCamToSlookupLupRsp(false, lookup_req.source);
      }
      cam_to_slookup_lup_rsp.write(lookup_rsp);
      logger.Info("CAM to Slookup Lookup Rsp", lookup_rsp.to_string(), false);
    }

    if (!slookup_to_cam_upd_req.empty()) {
      slookup_to_cam_upd_req.read(update_req);

      logger.Info("Slookup to CAM Update Req", update_req.to_string(), false);
      iter = _mock_cam.find(update_req.key);
      if (iter == _mock_cam.end()) {
        if (update_req.op == INSERT) {
          _mock_cam[update_req.key] = update_req.value;
          update_rsp = RtlCamToSlookupUpdRsp(update_req.value, INSERT, update_req.source);
          cout << "Current Update session: " << update_req.value << endl;

        } else {
          std::cerr << "delete a non-existing one" << endl;
        }
      } else {  // found in map
        if (update_req.op == INSERT) {
          std::cerr << "insert a existing one" << endl;
        } else {
          _mock_cam.erase(update_req.key);
          update_rsp = RtlCamToSlookupUpdRsp(update_req.value, DELETE, update_req.source);
        }
      }
      cam_to_slookup_upd_rsp.write(update_rsp);
      logger.Info("CAM to Slookup Update Rsp", update_rsp.to_string(), false);
    }
  }
};

#endif  // _MOCK_CAM_HPP_