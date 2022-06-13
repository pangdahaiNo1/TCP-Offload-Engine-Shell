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
      logger.Info(SLUP_CTRL, CUKOO_CAM, "Lup Req", lookup_req.to_string());

      iter = _mock_cam.find(lookup_req.key);
      if (iter != _mock_cam.end())  // hit
      {
        lookup_rsp = RtlCamToSlookupLupRsp(true, iter->second, lookup_req.source);
      } else {
        lookup_rsp = RtlCamToSlookupLupRsp(false, lookup_req.source);
      }
      cam_to_slookup_lup_rsp.write(lookup_rsp);
      logger.Info(CUKOO_CAM, SLUP_CTRL, "Lup Rsp", lookup_rsp.to_string());
    }

    if (!slookup_to_cam_upd_req.empty()) {
      slookup_to_cam_upd_req.read(update_req);

      logger.Info(SLUP_CTRL, CUKOO_CAM, "Upd Req", update_req.to_string());
      iter                  = _mock_cam.find(update_req.key);
      update_rsp.op         = update_req.op;
      update_rsp.key        = update_req.key;
      update_rsp.session_id = update_req.value;
      update_rsp.source     = update_req.source;
      update_rsp.success    = false;
      if (update_req.op == INSERT) {
        if (iter == _mock_cam.end()) {
          _mock_cam[update_req.key] = update_req.value;
          update_rsp.success        = true;
        } else {
          // insert a existing one, insert error
          update_rsp.success = false;
        }
      } else if (update_req.op == DELETE) {
        if (iter == _mock_cam.end()) {
          // delete a non-existing one , delete error
          update_rsp.success = false;
        } else {
          _mock_cam.erase(update_req.key);
          update_rsp.success = true;
        }
      }
      cam_to_slookup_upd_rsp.write(update_rsp);
      logger.Info(CUKOO_CAM, SLUP_CTRL, "Upd Rsp", update_rsp.to_string());
    }
  }
};

#endif  // _MOCK_CAM_HPP_