#include "session_lookup_controller.hpp"
using namespace hls;
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/** @ingroup session_lookup_controller
 *  Get New Available SessionId
 *  @param[in]		released_id_in, IDs that are released and appended to the SessionID free list
 *  @param[out]		available_id_out, get a new SessionID from the SessionID free list
 */
void                 GetNewAvailableSessionId(stream<ap_uint<14> > &released_id_in,
                                              stream<ap_uint<14> > &available_id_out) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static ap_uint<14> cur_session_id = 0;
#pragma HLS reset variable = cur_session_id
  ap_uint<14> session_id;

  if (!released_id_in.empty()) {
    released_id_in.read(session_id);
    available_id_out.write(session_id);
  } else if (cur_session_id < TCP_MAX_SESSIONS) {
    available_id_out.write(cur_session_id);
    cur_session_id++;
  }
}

/** @ingroup session_lookup_controller
 *  Handles the Lookup replies from the RTL Lookup Table, if there was no hit,
 *  it checks if the request is allowed to create a new session_id and does so.
 *  If it is a hit, the reply is forwarded to the corresponding source.
 *  It also handles the replies of the Session Updates [Inserts/Deletes], in case
 *  of insert the response with the new session_id is replied to the request source.
 */
void CamLookupRspHandler(
    // other req
    stream<ap_uint<14> > &slookup_available_id_in,
    // rx eng
    stream<RxEngToSlookupReq> &rx_eng_to_slookup_req,
    stream<SessionLookupRsp>  &slookup_to_rx_eng_rsp,
    // tx app
    stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
    stream<SessionLookupRsp>  &slookup_to_tx_app_rsp,
    // cam
    stream<RtlSLookupToCamLupReq> &slookup_to_cam_lup_req,
    stream<RtlCamToSlookupLupRsp> &cam_to_slookup_lup_rsp,
    stream<RtlSlookupToCamUpdReq> &slookup_to_cam_insert_req,
    stream<RtlCamToSlookupUpdRsp> &cam_to_slookup_upd_rsp,
    // insert req
    stream<SlookupToRevTableUpdReq> &reverse_table_insert_req) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  //   static stream<RevTableEntry>
  //   slookup_rev_table_req_cache_fifo("slookup_rev_table_req_cache_fifo");
  // #pragma HLS STREAM variable = slookup_rev_table_req_cache_fifo depth = 4

  //   static stream<SlookupReqInternal>
  //   slookup_to_cam_req_cache_fifo("slookup_to_cam_req_cache_fifo");
  // #pragma HLS STREAM variable = slookup_to_cam_req_cache_fifo depth = 8

  static SlookupReqInternal to_cam_req_cache;
  static bool               to_cam_req_cache_valid = false;
  static RevTableEntry      to_rev_table_req_cache;
  static bool               to_rev_table_req_cache_valid = false;
  TxAppToSlookupReq         tx_app_req;
  RxEngToSlookupReq         rx_eng_req;
  RtlCamToSlookupLupRsp     cam_rsp;
  RtlCamToSlookupUpdRsp     cam_update_rsp;
  RtlSLookupToCamLupReq     cam_lup_req;
  ap_uint<14>               available_session_id = 0;
  SessionLookupRsp          slookup_rsp;

  enum SlookupFsmState { LUP_REQ, LUP_RSP, UPD_RSP };
  static SlookupFsmState slc_fsm_state = LUP_REQ;

  switch (slc_fsm_state) {
    case LUP_REQ:
      if (!tx_app_to_slookup_req.empty()) {
        tx_app_to_slookup_req.read(tx_app_req);
#if MULTI_IP_ADDR
        to_cam_req_cache.tuple.src_ip_addr  = tx_app_req.four_tuple.src_ip_addr;
        to_cam_req_cache.tuple.src_tcp_port = tx_app_req.four_tuple.src_tcp_port;
        to_cam_req_cache.tuple.dst_ip_addr  = tx_app_req.four_tuple.dst_ip_addr;
        to_cam_req_cache.tuple.dst_tcp_port = tx_app_req.four_tuple.dst_tcp_port;
#else
        to_cam_req_cache.tuple.there_ip_addr  = tx_app_req.four_tuple.dst_ip_addr;
        to_cam_req_cache.tuple.there_tcp_port = tx_app_req.four_tuple.dst_tcp_port;
        to_cam_req_cache.tuple.here_tcp_port  = tx_app_req.four_tuple.src_tcp_port;
#endif
        to_cam_req_cache.allow_creation = true;
        to_cam_req_cache.source         = TX_APP;
        to_cam_req_cache.role_id        = tx_app_req.role_id;
        cam_lup_req = RtlSLookupToCamLupReq(to_cam_req_cache.tuple, to_cam_req_cache.source);
        slookup_to_cam_lup_req.write(cam_lup_req);
        logger.Info(SLUP_CTRL, CUKOO_CAM, "Lup Req from TxApp", cam_lup_req.to_string());
        to_cam_req_cache_valid = true;
        slc_fsm_state          = LUP_RSP;
      } else if (!rx_eng_to_slookup_req.empty()) {
        rx_eng_to_slookup_req.read(rx_eng_req);
#if MULTI_IP_ADDR
        to_cam_req_cache.tuple.src_ip_addr  = rx_eng_req.four_tuple.dst_ip_addr;
        to_cam_req_cache.tuple.src_tcp_port = rx_eng_req.four_tuple.dst_tcp_port;
        to_cam_req_cache.tuple.dst_ip_addr  = rx_eng_req.four_tuple.src_ip_addr;
        to_cam_req_cache.tuple.dst_tcp_port = rx_eng_req.four_tuple.src_tcp_port;
#else
        to_cam_req_cache.tuple.there_ip_addr  = rx_eng_req.four_tuple.src_ip_addr;
        to_cam_req_cache.tuple.there_tcp_port = rx_eng_req.four_tuple.src_tcp_port;
        to_cam_req_cache.tuple.here_tcp_port  = rx_eng_req.four_tuple.dst_tcp_port;
#endif
        to_cam_req_cache.allow_creation = rx_eng_req.allow_creation;
        to_cam_req_cache.source         = RX;
        to_cam_req_cache.role_id        = rx_eng_req.role_id;
        cam_lup_req = RtlSLookupToCamLupReq(to_cam_req_cache.tuple, to_cam_req_cache.source);
        slookup_to_cam_lup_req.write(cam_lup_req);
        logger.Info(SLUP_CTRL, CUKOO_CAM, "Lup Req from RxEng", cam_lup_req.to_string());
        to_cam_req_cache_valid = true;
        slc_fsm_state          = LUP_RSP;
      }
      break;
    case LUP_RSP:
      if (!cam_to_slookup_lup_rsp.empty() && to_cam_req_cache_valid) {
        cam_to_slookup_lup_rsp.read(cam_rsp);
        to_cam_req_cache_valid = false;
        if (!cam_rsp.hit && to_cam_req_cache.allow_creation && !slookup_available_id_in.empty()) {
          slookup_available_id_in.read(available_session_id);
          slookup_to_cam_insert_req.write(RtlSlookupToCamUpdReq(
              to_cam_req_cache.tuple, available_session_id, INSERT, cam_rsp.source));
          // Only when it allow create, the role-id insert to the reverse table
          to_rev_table_req_cache = RevTableEntry(to_cam_req_cache.tuple, to_cam_req_cache.role_id);
          to_rev_table_req_cache_valid = true;
          slc_fsm_state                = UPD_RSP;
        } else {
          slookup_rsp = SessionLookupRsp(cam_rsp.session_id, cam_rsp.hit, to_cam_req_cache.role_id);
          if (cam_rsp.source == RX) {
            logger.Info(SLUP_CTRL, RX_ENGINE, "Lup Rsp", slookup_rsp.to_string());
            slookup_to_rx_eng_rsp.write(slookup_rsp);
          } else {
            logger.Info(SLUP_CTRL, TX_APP_IF, "Lup Rsp", slookup_rsp.to_string());
            slookup_to_tx_app_rsp.write(slookup_rsp);
          }
          slc_fsm_state = LUP_REQ;
        }
      }
      break;
    case UPD_RSP:
      if (!cam_to_slookup_upd_rsp.empty() && to_rev_table_req_cache_valid) {
        cam_to_slookup_upd_rsp.read(cam_update_rsp);
        to_rev_table_req_cache_valid = false;
        // update rev table requset
        reverse_table_insert_req.write(SlookupToRevTableUpdReq(cam_update_rsp.session_id,
                                                               to_rev_table_req_cache.tuple,
                                                               to_rev_table_req_cache.role_id));

        // reponse to rx engine or tx app
        slookup_rsp =
            SessionLookupRsp(cam_update_rsp.session_id, true, to_rev_table_req_cache.role_id);
        if (cam_update_rsp.source == RX) {
          logger.Info(SLUP_CTRL, RX_ENGINE, "Update Rsp", slookup_rsp.to_string());
          slookup_to_rx_eng_rsp.write(slookup_rsp);
        } else {
          logger.Info(SLUP_CTRL, TX_APP_IF, "Update Rsp", slookup_rsp.to_string());
          slookup_to_tx_app_rsp.write(slookup_rsp);
        }
        slc_fsm_state = LUP_REQ;
      }
      break;
  }
}

/**
 * @brief: merge the all request to CAM, if the request is the DELETE, notify the @ref
 * GetNewAvailableSessionId for the new free session id
 */
void                 CamUpdateReqSender(stream<RtlSlookupToCamUpdReq> &slookup_to_cam_insert_req,
                                        stream<RtlSlookupToCamUpdReq> &slookup_to_cam_delete_req,
                                        stream<RtlSlookupToCamUpdReq> &rtl_slookup_to_cam_update_req,
                                        stream<ap_uint<14> >          &slookup_released_id,
                                        ap_uint<16>                   &reg_session_cnt) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static ap_uint<16>    used_session_id_cnt = 0;
  RtlSlookupToCamUpdReq cam_upd_req;

  if (!slookup_to_cam_insert_req.empty()) {
    slookup_to_cam_insert_req.read(cam_upd_req);
    logger.Info(SLUP_CTRL, CUKOO_CAM, "Insert Req", cam_upd_req.to_string());
    rtl_slookup_to_cam_update_req.write(cam_upd_req);
    used_session_id_cnt++;
    reg_session_cnt = used_session_id_cnt;
    logger.Info(SLUP_CTRL, "Current Session count", reg_session_cnt.to_string(16));
  } else if (!slookup_to_cam_delete_req.empty()) {
    slookup_to_cam_delete_req.read(cam_upd_req);
    logger.Info(SLUP_CTRL, CUKOO_CAM, "Delete Req", cam_upd_req.to_string());
    rtl_slookup_to_cam_update_req.write(cam_upd_req);
    slookup_released_id.write(cam_upd_req.value(13, 0));
    used_session_id_cnt--;
    reg_session_cnt = used_session_id_cnt;
    logger.Info(SLUP_CTRL, "Current Released Seesion ", cam_upd_req.value(13, 0).to_string(16));
    logger.Info(SLUP_CTRL, "Current Session count", reg_session_cnt.to_string(16));
  }
}

/**
 * @brief: handle the response from the CAM, if the response is insert, notify the @ref
 * CamLookupRspHandler
 */
void CamUpdateRspHandler(stream<RtlCamToSlookupUpdRsp> &rtl_cam_to_slookup_update_rsp,
                         stream<RtlCamToSlookupUpdRsp> &cam_to_slookup_upd_rsp) {
#pragma HLS PIPELINE II = 1

  RtlCamToSlookupUpdRsp cam_upd_rsp;

  if (!rtl_cam_to_slookup_update_rsp.empty()) {
    rtl_cam_to_slookup_update_rsp.read(cam_upd_rsp);
    logger.Info(SLUP_CTRL, CUKOO_CAM, "Update rsp", cam_upd_rsp.to_string());
    if (cam_upd_rsp.op == INSERT) {
      cam_to_slookup_upd_rsp.write(cam_upd_rsp);
    }
  }
}

/**
 * @details:
 * @param[in] reverse_table_insert_req: handle the insert request from Rx Eng or Tx app, @ref
 * CamLookupRspHandler
 * @param[in] sttable_to_slookup_release_req: handle the delete request from State table
 * @param[out] slookup_to_ptable_release_port_req: when delete a session, delete port table
 * @param[out] slookup_to_cam_delete_req: when delete a session, delete the CAM
 * @param[out] slookup_rev_table_to_tx_eng_rsp: handle the lookup request from Tx eng
 * @param[in] rx_app_to_slookup_tdest_lookup_req: handle SessionID to TDEST lookup
 * @param[out] slookup_to_rx_app_tdest_lookup_rsp
 *
 */
void SlookupReverseTableInterface(
#if MULTI_IP_ADDR
#else
    // register
    IpAddr my_ip_addr,
#endif
    // insert req
    stream<SlookupToRevTableUpdReq> &reverse_table_insert_req,
    stream<TcpSessionID>            &sttable_to_slookup_release_req,
    // rx app
    stream<TcpSessionID> &rx_app_to_slookup_tdest_lookup_req,
    stream<NetAXISDest>  &slookup_to_rx_app_tdest_lookup_rsp,
    // tx app
    stream<TcpSessionID> &tx_app_to_slookup_check_tdest_req,
    stream<NetAXISDest>  &slookup_to_tx_app_check_tdest_rsp,
    // tx eng
    stream<TcpSessionID>           &tx_eng_to_slookup_rev_table_req,
    stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
    // to other req
    stream<TcpSessionID>          &slookup_to_ptable_release_port_req,
    stream<RtlSlookupToCamUpdReq> &slookup_to_cam_delete_req) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static RevTableEntry slookup_rev_table[TCP_MAX_SESSIONS];
#pragma HLS bind_storage variable = slookup_rev_table type = RAM_T2P impl = BRAM
#pragma HLS DEPENDENCE variable = slookup_rev_table inter false
  static bool valid_tuple[TCP_MAX_SESSIONS];
#pragma HLS DEPENDENCE variable = valid_tuple inter false

  SlookupToRevTableUpdReq insert_req;
  ReverseTableToTxEngRsp  tx_eng_rsp;
  CamTuple                cur_tuple_to_release;
  TcpSessionID            session_id;
  NetAXISDest             ret_role_id;

  if (!reverse_table_insert_req.empty()) {
    reverse_table_insert_req.read(insert_req);
    logger.Info(MISC_MDLE, SLUP_CTRL, "Insert Rev Table", insert_req.to_string());
    slookup_rev_table[insert_req.key] =
        RevTableEntry(insert_req.tuple_value, insert_req.role_value);
    valid_tuple[insert_req.key] = true;

  } else if (!tx_eng_to_slookup_rev_table_req.empty()) {
    tx_eng_to_slookup_rev_table_req.read(session_id);
    logger.Info(
        TX_ENGINE, SLUP_CTRL, "Rev Table Req for tuple and TDEST", session_id.to_string(16));
#if MULTI_IP_ADDR
    tx_eng_rsp.four_tuple.src_ip_addr  = slookup_rev_table[session_id].tuple.src_ip_addr;
    tx_eng_rsp.four_tuple.dst_ip_addr  = slookup_rev_table[session_id].tuple.dst_ip_addr;
    tx_eng_rsp.four_tuple.src_tcp_port = slookup_rev_table[session_id].tuple.src_tcp_port;
    tx_eng_rsp.four_tuple.dst_tcp_port = slookup_rev_table[session_id].tuple.dst_tcp_port;
#else
    tx_eng_rsp.four_tuple.src_ip_addr = my_ip_addr;
    tx_eng_rsp.four_tuple.dst_ip_addr = slookup_rev_table[session_id].tuple.there_ip_addr;
    tx_eng_rsp.four_tuple.src_tcp_port = slookup_rev_table[session_id].tuple.here_tcp_port;
    tx_eng_rsp.four_tuple.dst_tcp_port = slookup_rev_table[session_id].tuple.there_tcp_port;
#endif
    tx_eng_rsp.role_id = slookup_rev_table[session_id].role_id;
    logger.Info(SLUP_CTRL, TX_ENGINE, "Rev Table Rsp", tx_eng_rsp.to_string());
    slookup_rev_table_to_tx_eng_rsp.write(tx_eng_rsp);
  } else if (!rx_app_to_slookup_tdest_lookup_req.empty()) {
    rx_app_to_slookup_tdest_lookup_req.read(session_id);
    logger.Info(RX_APP_IF, SLUP_CTRL, "Rev Table Req for TDEST", session_id.to_string(16));
    if (valid_tuple[session_id]) {
      ret_role_id = slookup_rev_table[session_id].role_id;
    } else {
      ret_role_id = INVALID_TDEST;
    }
    logger.Info(SLUP_CTRL, RX_APP_IF, "Rev Table TDEST Rsp", ret_role_id.to_string(16));
    slookup_to_rx_app_tdest_lookup_rsp.write(ret_role_id);
  } else if (!tx_app_to_slookup_check_tdest_req.empty()) {
    tx_app_to_slookup_check_tdest_req.read(session_id);
    logger.Info(TX_APP_IF, SLUP_CTRL, "Rev Table Req for TDEST", session_id.to_string(16));
    if (valid_tuple[session_id]) {
      ret_role_id = slookup_rev_table[session_id].role_id;
    } else {
      ret_role_id = INVALID_TDEST;
    }
    logger.Info(SLUP_CTRL, TX_APP_IF, "Rev Table TDEST Rsp", ret_role_id.to_string(16));
    slookup_to_tx_app_check_tdest_rsp.write(ret_role_id);
  } else if (!sttable_to_slookup_release_req.empty()) {
    sttable_to_slookup_release_req.read(session_id);
    logger.Info(SLUP_CTRL, "Rev Table release Session", session_id.to_string(16));
    cur_tuple_to_release = slookup_rev_table[session_id].tuple;
    if (valid_tuple[session_id])  // if valid
    {
#if MULTI_IP_ADDR
      TcpPortNumber release_port = SwapByte<16>(cur_tuple_to_release.src_tcp_port);
#else
      TcpPortNumber release_port = SwapByte<16>(cur_tuple_to_release.here_tcp_port);
#endif
      slookup_to_ptable_release_port_req.write(release_port);
      slookup_to_cam_delete_req.write(
          RtlSlookupToCamUpdReq(cur_tuple_to_release, session_id, DELETE, RX));
      logger.Info(SLUP_CTRL, PORT_TBLE, "Rev Table release port LE ", release_port.to_string(16));
    }
    valid_tuple[session_id] = false;
  }
}

/** @ingroup session_lookup_controller
 *  This module acts as a wrapper for the RTL implementation of the SessionID Table.
 *  It also includes the wrapper for the session_id free list which keeps track of the free
 * SessionIDs
 */
void session_lookup_controller(
// registers
#if MULTI_IP_ADDR
#else
    IpAddr &my_ip_addr,
#endif
    ap_uint<16> &reg_session_cnt,
    // from sttable
    stream<TcpSessionID> &sttable_to_slookup_release_req,
    // rx app
    stream<TcpSessionID> &rx_app_to_slookup_tdest_lookup_req,
    stream<NetAXISDest>  &slookup_to_rx_app_tdest_lookup_rsp,
    // rx eng
    stream<RxEngToSlookupReq> &rx_eng_to_slookup_req,
    stream<SessionLookupRsp>  &slookup_to_rx_eng_rsp,
    // tx app
    stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
    stream<SessionLookupRsp>  &slookup_to_tx_app_rsp,
    stream<TcpSessionID>      &tx_app_to_slookup_check_tdest_req,
    stream<NetAXISDest>       &slookup_to_tx_app_check_tdest_rsp,
    // tx eng
    stream<ap_uint<16> >           &tx_eng_to_slookup_rev_table_req,
    stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
    // CAM
    stream<RtlSLookupToCamLupReq> &rtl_slookup_to_cam_lookup_req,
    stream<RtlCamToSlookupLupRsp> &rtl_cam_to_slookup_lookup_rsp,
    stream<RtlSlookupToCamUpdReq> &rtl_slookup_to_cam_update_req,
    stream<RtlCamToSlookupUpdRsp> &rtl_cam_to_slookup_update_rsp,
    // to ptable
    stream<TcpPortNumber> &slookup_to_ptable_release_port_req) {
#pragma HLS DATAFLOW
  // #pragma HLS INLINE

#pragma HLS INTERFACE axis register port = rtl_slookup_to_cam_lookup_req
#pragma HLS INTERFACE axis register port = rtl_cam_to_slookup_lookup_rsp
#pragma HLS INTERFACE axis register port = rtl_slookup_to_cam_update_req
#pragma HLS INTERFACE axis register port = rtl_cam_to_slookup_update_rsp
#pragma HLS aggregate variable = rtl_slookup_to_cam_lookup_req compact = bit
#pragma HLS aggregate variable = rtl_cam_to_slookup_lookup_rsp compact = bit
#pragma HLS aggregate variable = rtl_slookup_to_cam_update_req compact = bit
#pragma HLS aggregate variable = rtl_cam_to_slookup_update_rsp compact = bit

  // Fifos
  static stream<ap_uint<14> > slookup_available_id_fifo("slookup_available_id_fifo");
#pragma HLS stream variable = slookup_available_id_fifo depth = 16384
  static stream<ap_uint<14> > slookup_released_id_fifo("slookup_released_id_fifo");
#pragma HLS stream variable = slookup_released_id_fifo depth = 2

  static stream<RtlSlookupToCamUpdReq> slookup_to_cam_insert_req_fifo(
      "slookup_to_cam_insert_req_fifo");
#pragma HLS STREAM variable = slookup_to_cam_insert_req_fifo depth = 16

  static stream<RtlSlookupToCamUpdReq> slookup_to_cam_delete_req_fifo(
      "slookup_to_cam_delete_req_fifo");
#pragma HLS STREAM variable = slookup_to_cam_delete_req_fifo depth = 4

  static stream<RtlCamToSlookupUpdRsp> cam_to_slookup_insert_rsp_fifo(
      "cam_to_slookup_insert_rsp_fifo");
#pragma HLS STREAM variable = cam_to_slookup_insert_rsp_fifo depth = 4

  static stream<SlookupToRevTableUpdReq> reverse_table_insert_req_fifo(
      "reverse_table_insert_req_fifo");
#pragma HLS STREAM variable = reverse_table_insert_req_fifo depth = 4

  GetNewAvailableSessionId(slookup_released_id_fifo, slookup_available_id_fifo);

  CamLookupRspHandler(slookup_available_id_fifo,
                      rx_eng_to_slookup_req,
                      slookup_to_rx_eng_rsp,
                      tx_app_to_slookup_req,
                      slookup_to_tx_app_rsp,
                      rtl_slookup_to_cam_lookup_req,
                      rtl_cam_to_slookup_lookup_rsp,
                      slookup_to_cam_insert_req_fifo,
                      cam_to_slookup_insert_rsp_fifo,
                      reverse_table_insert_req_fifo);

  CamUpdateReqSender(slookup_to_cam_insert_req_fifo,
                     slookup_to_cam_delete_req_fifo,
                     rtl_slookup_to_cam_update_req,
                     slookup_released_id_fifo,
                     reg_session_cnt);

  CamUpdateRspHandler(rtl_cam_to_slookup_update_rsp, cam_to_slookup_insert_rsp_fifo);

  SlookupReverseTableInterface(
#if MULTI_IP_ADDR
#else
      my_ip_addr,
#endif
      reverse_table_insert_req_fifo,
      sttable_to_slookup_release_req,
      rx_app_to_slookup_tdest_lookup_req,
      slookup_to_rx_app_tdest_lookup_rsp,
      tx_app_to_slookup_check_tdest_req,
      slookup_to_tx_app_check_tdest_rsp,
      tx_eng_to_slookup_rev_table_req,
      slookup_rev_table_to_tx_eng_rsp,
      slookup_to_ptable_release_port_req,
      slookup_to_cam_delete_req_fifo);
}
