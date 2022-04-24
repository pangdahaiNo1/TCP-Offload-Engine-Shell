
#include "session_lookup_controller.hpp"

using namespace hls;

/** @ingroup session_lookup_controller
 *  Get New Available SessionId
 *  @param[in]		released_id_in, IDs that are released and appended to the SessionID free list
 *  @param[out]		available_id_out, get a new SessionID from the SessionID free list
 */
void                 GetNewAvailableSessionId(stream<ap_uint<14> > &available_id_out,
                                              stream<ap_uint<14> > &released_id_in) {
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
 *  @param[in]		tx_eng_to_slookup_reverse_table_req
 *  @param[in]		cam_to_slookup_rsp
 *  @param[in]		sessionUpdatea_rsp
 *  @param[in]		sttable_to_slookup_release_session_req
 *  @param[in]		lookups
 *  @param[out]		slookup_to_rx_eng_rsp
 *  @param[out]		slookup_to_tx_app_rsp
 *  @param[out]		slookup_reverse_table_to_tx_eng_rsp
 *  @param[out]		slookup_to_cam_req
 *  @param[out]		slookup_to_ptable_release_port_req
 *  @TODO document more
 */
void                 CamLookupRspHandler(stream<RtlCamToSlookupRsp> &          cam_to_slookup_rsp,
                                         stream<RtlCamToSlookupUpdateRsp> &    cam_to_slookup_upd_rsp,
                                         stream<RxEngToSlookupReq> &           rx_eng_to_slookup_req,
                                         stream<TxAppToSlookupReq> &           tx_app_to_slookup_req,
                                         stream<ap_uint<14> > &                slookup_available_id_in,
                                         stream<RtlSLookupToCamReq> &          slookup_to_cam_req,
                                         stream<SessionLookupRsp> &            slookup_to_rx_eng_rsp,
                                         stream<SessionLookupRsp> &            slookup_to_tx_app_rsp,
                                         stream<RtlSlookupToCamUpdateReq> &    slookup_to_cam_insert_req,
                                         stream<SlookupReverseTableInsertReq> &reverse_table_insert_req) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static stream<ReverseTableEntry> slookup_reverse_table_req_cache_fifo(
      "slookup_reverse_table_req_cache_fifo");
#pragma HLS STREAM variable = slookup_reverse_table_req_cache_fifo depth = 4

  static stream<SlookupReqInternal> slookup_to_cam_req_cache_fifo("slookup_to_cam_req_cache_fifo");
#pragma HLS STREAM variable = slookup_to_cam_req_cache_fifo depth = 8

  TxAppToSlookupReq        tx_app_req;
  RxEngToSlookupReq        rx_eng_req;
  ReverseTableEntry        reverse_table_one_entry;
  SlookupReqInternal       req_internal;
  RtlCamToSlookupRsp       cam_rsp;
  RtlCamToSlookupUpdateRsp cam_update_rsp;
  // ap_uint<16>              session_id;
  ap_uint<14> available_session_id = 0;

  enum SlookupFsmState { LUP_REQ, LUP_RSP, UPD_RSP };
  static SlookupFsmState slc_fsm_state = LUP_REQ;

  switch (slc_fsm_state) {
    case LUP_REQ:
      if (!tx_app_to_slookup_req.empty()) {
        tx_app_to_slookup_req.read(tx_app_req);
        // req_internal.tuple.myIp = tx_app_req.four_tuple.src_ip_addr;
        req_internal.tuple.their_ip_addr  = tx_app_req.four_tuple.dst_ip_addr;
        req_internal.tuple.there_tcp_port = tx_app_req.four_tuple.dst_tcp_port;
        req_internal.tuple.here_tcp_port  = tx_app_req.four_tuple.src_tcp_port;
        req_internal.allow_creation       = true;
        req_internal.source               = TX_APP;
        req_internal.role_id              = tx_app_req.role_id;
        slookup_to_cam_req.write(RtlSLookupToCamReq(req_internal.tuple, req_internal.source));
        slookup_to_cam_req_cache_fifo.write(req_internal);
        slc_fsm_state = LUP_RSP;
      } else if (!rx_eng_to_slookup_req.empty()) {
        rx_eng_to_slookup_req.read(rx_eng_req);
        req_internal.tuple.their_ip_addr  = rx_eng_req.four_tuple.src_ip_addr;
        req_internal.tuple.there_tcp_port = rx_eng_req.four_tuple.src_tcp_port;
        // req_internal.tuple.myIp = rx_eng_req.tuple.dst_ip_addr;
        req_internal.tuple.here_tcp_port = rx_eng_req.four_tuple.dst_tcp_port;
        req_internal.allow_creation      = rx_eng_req.allow_creation;
        req_internal.source              = RX;
        req_internal.role_id             = rx_eng_req.role_id;
        slookup_to_cam_req.write(RtlSLookupToCamReq(req_internal.tuple, req_internal.source));
        slookup_to_cam_req_cache_fifo.write(req_internal);
        slc_fsm_state = LUP_RSP;
      }
      break;
    case LUP_RSP:
      if (!cam_to_slookup_rsp.empty() && !slookup_to_cam_req_cache_fifo.empty()) {
        cam_to_slookup_rsp.read(cam_rsp);
        slookup_to_cam_req_cache_fifo.read(req_internal);
        if (!cam_rsp.hit && req_internal.allow_creation && !slookup_available_id_in.empty()) {
          slookup_available_id_in.read(available_session_id);
          slookup_to_cam_insert_req.write(RtlSlookupToCamUpdateReq(
              req_internal.tuple, available_session_id, INSERT, cam_rsp.source));
          slookup_reverse_table_req_cache_fifo.write(
              ReverseTableEntry(req_internal.tuple, req_internal.role_id));
          slc_fsm_state = UPD_RSP;
        } else {
          if (cam_rsp.source == RX) {
            slookup_to_rx_eng_rsp.write(SessionLookupRsp(cam_rsp.session_id, cam_rsp.hit));
          } else {
            slookup_to_tx_app_rsp.write(SessionLookupRsp(cam_rsp.session_id, cam_rsp.hit));
          }
          slc_fsm_state = LUP_REQ;
        }
      }
      break;
    // case UPD_REQ:
    // break;
    case UPD_RSP:
      if (!cam_to_slookup_upd_rsp.empty() && !slookup_reverse_table_req_cache_fifo.empty()) {
        cam_to_slookup_upd_rsp.read(cam_update_rsp);
        slookup_reverse_table_req_cache_fifo.read(reverse_table_one_entry);
        // updateReplies.write(SessionLookupRsp(cam_update_rsp.session_id, true));
        if (cam_update_rsp.source == RX) {
          slookup_to_rx_eng_rsp.write(SessionLookupRsp(cam_update_rsp.session_id, true));
        } else {
          slookup_to_tx_app_rsp.write(SessionLookupRsp(cam_update_rsp.session_id, true));
        }
        reverse_table_insert_req.write(
            SlookupReverseTableInsertReq(cam_update_rsp.session_id,
                                         reverse_table_one_entry.three_tuple,
                                         reverse_table_one_entry.role_id));
        slc_fsm_state = LUP_REQ;
      }
      break;
  }
}

void                 CamUpdateReqSender(stream<RtlSlookupToCamUpdateReq> &slookup_to_cam_insert_req,
                                        stream<RtlSlookupToCamUpdateReq> &slookup_to_cam_delete_req,
                                        stream<RtlSlookupToCamUpdateReq> &rtl_slookup_to_cam_update_req,
                                        stream<ap_uint<14> > &            slookup_released_id,
                                        ap_uint<16> &                     reg_session_cnt) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static ap_uint<16>       used_session_id_cnt = 0;
  RtlSlookupToCamUpdateReq slookup_to_cam_req;

  if (!slookup_to_cam_insert_req.empty()) {
    rtl_slookup_to_cam_update_req.write(slookup_to_cam_insert_req.read());
    used_session_id_cnt++;
    reg_session_cnt = used_session_id_cnt;
  } else if (!slookup_to_cam_delete_req.empty()) {
    slookup_to_cam_delete_req.read(slookup_to_cam_req);
    rtl_slookup_to_cam_update_req.write(slookup_to_cam_req);
    slookup_released_id.write(slookup_to_cam_req.value);
    used_session_id_cnt--;
    reg_session_cnt = used_session_id_cnt;
  }
}

void CamUpdateRspHandler(stream<RtlCamToSlookupUpdateRsp> &rtl_cam_to_slookup_update_rsp,
                         stream<RtlCamToSlookupUpdateRsp> &cam_to_slookup_upd_rsp)
// stream<ap_uint<14> >&					slookup_released_id)
{
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  RtlCamToSlookupUpdateRsp cam_to_slookup_rsp;
  // ThreeTuple               tuple;

  if (!rtl_cam_to_slookup_update_rsp.empty()) {
    rtl_cam_to_slookup_update_rsp.read(cam_to_slookup_rsp);
    if (cam_to_slookup_rsp.op == INSERT) {
      cam_to_slookup_upd_rsp.write(cam_to_slookup_rsp);
    }
    /*else // DELETE
    {
      slookup_released_id.write(cam_to_slookup_rsp.session_id);
    }*/
  }
}

void SlookupReverseTableInterface(
    stream<SlookupReverseTableInsertReq> &reverse_table_insert_req,
    stream<TcpSessionID> &                sttable_to_slookup_release_session_req,
    stream<TcpSessionID> &                tx_eng_to_slookup_reverse_table_req,
    stream<TcpSessionID> &                slookup_to_ptable_release_port_req,
    stream<RtlSlookupToCamUpdateReq> &    slookup_to_cam_delete_req,
    stream<ReverseTableToTxEngRsp> &      slookup_reverse_table_to_tx_eng_rsp,
    IpAddr                                my_ip_addr) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static ReverseTableEntry slookup_reverse_table[TCP_MAX_SESSIONS];
#pragma HLS RESOURCE variable = slookup_reverse_table core = RAM_T2P_BRAM
#pragma HLS DEPENDENCE variable                            = slookup_reverse_table inter false
  static bool valid_tuple[TCP_MAX_SESSIONS];
#pragma HLS DEPENDENCE variable = valid_tuple inter false

  SlookupReverseTableInsertReq insert_req;
  ReverseTableToTxEngRsp       tx_eng_rsp;
  ThreeTuple                   cur_tuple_to_release;
  TcpSessionID                 session_id;

  if (!reverse_table_insert_req.empty()) {
    reverse_table_insert_req.read(insert_req);
    slookup_reverse_table[insert_req.key] =
        ReverseTableEntry(insert_req.tuple_value, insert_req.role_value);
    valid_tuple[insert_req.key] = true;

  }
  // TODO check if else if necessary
  else if (!sttable_to_slookup_release_session_req.empty()) {
    sttable_to_slookup_release_session_req.read(session_id);
    cur_tuple_to_release = slookup_reverse_table[session_id].three_tuple;
    if (valid_tuple[session_id])  // if valid
    {
      slookup_to_ptable_release_port_req.write(cur_tuple_to_release.here_tcp_port);
      slookup_to_cam_delete_req.write(
          RtlSlookupToCamUpdateReq(cur_tuple_to_release, session_id, DELETE, RX));
    }
    valid_tuple[session_id] = false;
  } else if (!tx_eng_to_slookup_reverse_table_req.empty()) {
    tx_eng_to_slookup_reverse_table_req.read(session_id);
    tx_eng_rsp.four_tuple.src_ip_addr = my_ip_addr;
    tx_eng_rsp.four_tuple.dst_ip_addr = slookup_reverse_table[session_id].three_tuple.their_ip_addr;
    tx_eng_rsp.four_tuple.src_tcp_port =
        slookup_reverse_table[session_id].three_tuple.here_tcp_port;
    tx_eng_rsp.four_tuple.dst_tcp_port =
        slookup_reverse_table[session_id].three_tuple.there_tcp_port;
    tx_eng_rsp.role_id = slookup_reverse_table[session_id].role_id;
    slookup_reverse_table_to_tx_eng_rsp.write(tx_eng_rsp);
  }
}

/** @ingroup session_lookup_controller
 *  This module acts as a wrapper for the RTL implementation of the SessionID Table.
 *  It also includes the wrapper for the session_id free list which keeps track of the free
 * SessionIDs
 *  @param[in]		rxLookupIn
 *  @param[in]		sttable_to_slookup_release_session_req
 *  @param[in]		txAppLookupIn
 *  @param[in]		txLookup
 *  @param[in]		lookupIn
 *  @param[in]		updateIn
 *  @param[out]		rxLookupOut
 *  @param[out]		portReleaseOut
 *  @param[out]		txAppLookupOut
 *  @param[out]		txResponse
 *  @param[out]		lookupOut
 *  @param[out]		updateOut
 *  @TODO rename
 */
void session_lookup_controller(stream<RxEngToSlookupReq> &rx_eng_to_slookup_req,
                               stream<SessionLookupRsp> & slookup_to_rx_eng_rsp,
                               stream<ap_uint<16> > &     sttable_to_slookup_release_session_req,
                               stream<ap_uint<16> > &     slookup_to_ptable_release_port_req,
                               stream<TxAppToSlookupReq> &tx_app_to_slookup_req,
                               stream<SessionLookupRsp> & slookup_to_tx_app_rsp,
                               stream<ap_uint<16> > &     tx_eng_to_slookup_reverse_table_req,
                               stream<ReverseTableToTxEngRsp> &slookup_reverse_table_to_tx_eng_rsp,
                               stream<RtlSLookupToCamReq> &    slookup_to_cam_req,
                               stream<RtlCamToSlookupRsp> &    cam_to_slookup_rsp,
                               stream<RtlSlookupToCamUpdateReq> &rtl_slookup_to_cam_update_req,
                               stream<RtlCamToSlookupUpdateRsp> &rtl_cam_to_slookup_update_rsp,
                               ap_uint<16> &                     reg_session_cnt,
                               ap_uint<32>                       my_ip_addr) {
//#pragma HLS DATAFLOW
#pragma HLS INLINE

  // Fifos
  static stream<SlookupReqInternal> slc_lookups("slc_lookups");
#pragma HLS stream variable = slc_lookups depth    = 4
#pragma HLS DATA_PACK                     variable = slc_lookups

  static stream<ap_uint<14> > slookup_available_id_fifo("slookup_available_id_fifo");
  static stream<ap_uint<14> > slookup_released_id_fifo("slookup_released_id_fifo");
#pragma HLS stream variable = slookup_available_id_fifo depth = 16384
#pragma HLS stream variable = slookup_released_id_fifo depth = 2

  static stream<RtlCamToSlookupUpdateRsp> cam_to_slookup_insert_rsp_fifo(
      "cam_to_slookup_insert_rsp_fifo");
#pragma HLS STREAM variable = cam_to_slookup_insert_rsp_fifo depth = 4

  static stream<RtlSlookupToCamUpdateReq> slookup_to_cam_insert_req_fifo(
      "slookup_to_cam_insert_req_fifo");
#pragma HLS STREAM variable = rtl_slookup_to_cam_insert_req_fifo depth = 4

  static stream<RtlSlookupToCamUpdateReq> slookup_to_cam_delete_req_fifo(
      "slookup_to_cam_delete_req_fifo");
#pragma HLS STREAM variable = slookup_to_cam_delete_req_fifo depth = 4

  static stream<SlookupReverseTableInsertReq> reverse_table_insert_req_fifo(
      "reverse_table_insert_req_fifo");
#pragma HLS STREAM variable = reverse_table_insert_req_fifo depth = 4

  GetNewAvailableSessionId(slookup_available_id_fifo, slookup_released_id_fifo);

  CamLookupRspHandler(cam_to_slookup_rsp,
                      cam_to_slookup_insert_rsp_fifo,
                      rx_eng_to_slookup_req,
                      tx_app_to_slookup_req,
                      slookup_available_id_fifo,
                      slookup_to_cam_req,
                      slookup_to_rx_eng_rsp,
                      slookup_to_tx_app_rsp,
                      slookup_to_cam_insert_req_fifo,
                      reverse_table_insert_req_fifo);  // reg_session_cnt);

  CamUpdateReqSender(slookup_to_cam_insert_req_fifo,
                     slookup_to_cam_delete_req_fifo,
                     rtl_slookup_to_cam_update_req,
                     slookup_released_id_fifo,
                     reg_session_cnt);

  CamUpdateRspHandler(rtl_cam_to_slookup_update_rsp, cam_to_slookup_insert_rsp_fifo);

  SlookupReverseTableInterface(reverse_table_insert_req_fifo,
                               sttable_to_slookup_release_session_req,
                               tx_eng_to_slookup_reverse_table_req,
                               slookup_to_ptable_release_port_req,
                               slookup_to_cam_delete_req_fifo,
                               slookup_reverse_table_to_tx_eng_rsp,
                               my_ip_addr);
}
