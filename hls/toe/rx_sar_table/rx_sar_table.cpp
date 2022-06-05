
#include "rx_sar_table.hpp"
using namespace hls;
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/** @ingroup rx_sar_table
 * 	This data structure stores the RX(receiving) sliding window
 *  and handles concurrent access from the @ref rx_engine, @ref rx_app_intf
 *  and @ref tx_engine
 *  @param[in]		rx_eng_to_rx_sar_req
 *  @param[in]		rx_app_to_rx_sar_req
 *  @param[in]		tx_eng_to_rx_sar_lup_req
 *  @param[out]		rx_sar_to_rx_eng_rsp
 *  @param[out]		rx_sar_to_rx_app_rsp
 *  @param[out]		rx_sar_to_tx_eng_lup_rsp
 */
void                 rx_sar_table(stream<RxSarAppReqRsp> & rx_app_to_rx_sar_req,
                                  stream<RxSarAppReqRsp> & rx_sar_to_rx_app_rsp,
                                  stream<RxEngToRxSarReq> &rx_eng_to_rx_sar_req,
                                  stream<RxSarTableEntry> &rx_sar_to_rx_eng_rsp,
                                  stream<TcpSessionID> &   tx_eng_to_rx_sar_lup_req,
                                  stream<RxSarLookupRsp> & rx_sar_to_tx_eng_lup_rsp) {
#pragma HLS PIPELINE II = 1

  static RxSarTableEntry rx_sar_table[TCP_MAX_SESSIONS];
#pragma HLS bind_storage variable = rx_sar_table type = RAM_1P impl = BRAM
#pragma HLS DEPENDENCE variable                                     = rx_sar_table inter false

  // Read only access from the Tx Engine
  if (!tx_eng_to_rx_sar_lup_req.empty()) {
    TcpSessionID    session_id = tx_eng_to_rx_sar_lup_req.read();
    RxSarTableEntry entry      = rx_sar_table[session_id];
    logger.Info(TX_ENG, RX_SAR, "Lup Req", session_id.to_string(16));

    // Pre-calculated usedLength, windowSize to improve timing in metaLoader
    // This works even for wrap around
    // when sending a segment, win_size in tcp header is the actucal size after right shifting
    TcpSessionBuffer session_actual_win_size =
        (entry.app_read - entry.recvd(WINDOW_BITS - 1, 0)) - 1;

    RxSarLookupRsp to_tx_eng_rsp;
    to_tx_eng_rsp.recvd     = entry.recvd;
    to_tx_eng_rsp.win_shift = entry.win_shift;
    to_tx_eng_rsp.win_size  = session_actual_win_size >> entry.win_shift;

    rx_sar_to_tx_eng_lup_rsp.write(to_tx_eng_rsp);
    logger.Info(RX_SAR, TX_ENG, "Lup Rsp", to_tx_eng_rsp.to_string());
  }
  // Read or Write access from the Rx App I/F to update the application pointer
  else if (!rx_app_to_rx_sar_req.empty()) {
    RxSarAppReqRsp rx_app_req = rx_app_to_rx_sar_req.read();
    logger.Info(RX_APP_INTF, RX_SAR, "R/W Req", rx_app_req.to_string());
    if (rx_app_req.write) {
      rx_sar_table[rx_app_req.session_id].app_read = rx_app_req.app_read;
    } else {
      RxSarAppReqRsp rx_app_rsp(rx_app_req.session_id,
                                rx_sar_table[rx_app_req.session_id].app_read);
      logger.Info(RX_SAR, RX_APP_INTF, "Read Rsp", rx_app_rsp.to_string());
      rx_sar_to_rx_app_rsp.write(rx_app_rsp);
    }
  }
  // Read or Write access from the Rx Engine
  else if (!rx_eng_to_rx_sar_req.empty()) {
    RxEngToRxSarReq recv_new_seg_req = rx_eng_to_rx_sar_req.read();
    logger.Info(RX_ENG, RX_SAR, "R/W Req", recv_new_seg_req.to_string());

    if (recv_new_seg_req.write) {
      rx_sar_table[recv_new_seg_req.session_id].recvd = recv_new_seg_req.recvd;
      // rx_sar_table[recv_new_seg_req.session_id].head   = recv_new_seg_req.head;
      // rx_sar_table[recv_new_seg_req.session_id].offset = recv_new_seg_req.offset;
      // rx_sar_table[recv_new_seg_req.session_id].gap    = recv_new_seg_req.gap;
      if (recv_new_seg_req.init) {
        rx_sar_table[recv_new_seg_req.session_id].app_read  = recv_new_seg_req.recvd;
        rx_sar_table[recv_new_seg_req.session_id].win_shift = recv_new_seg_req.win_shift;
      }
    } else {
      logger.Info(RX_SAR,
                  RX_ENG,
                  "Read SAR Entry Rsp",
                  (rx_sar_table[recv_new_seg_req.session_id]).to_string());
      rx_sar_to_rx_eng_rsp.write(rx_sar_table[recv_new_seg_req.session_id]);
    }
  }
}
