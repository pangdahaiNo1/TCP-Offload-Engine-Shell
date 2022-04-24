#include "tx_sar_table.hpp"

using namespace hls;

/** @ingroup tx_sar_table
 *  This data structure stores the TX(transmitting) sliding window
 *  and handles concurrent access from the @ref rx_engine, @ref tx_app_if
 *  and @ref tx_engine
 *  @TODO check if locking is actually required, especially for rxOut
 *  @param[in] rx_eng_to_tx_sar_upd_req
 *  @param[in] tx_eng_to_tx_sar_req  read or write
 *  @param[in] tx_app_to_tx_sar_update_req
 *  @param[out] tx_sar_to_rx_eng_lookup_rsp
 *  @param[out] tx_sar_to_tx_eng_lookup_rsp
 *  @param[out] tx_sar_to_app_rsp
 */
void                 tx_sar_table(stream<TxSarUpdateFromAckedSegReq> &rx_eng_to_tx_sar_upd_req,
                                  stream<TxEngToTxSarReq> &           tx_eng_to_tx_sar_req,
                                  stream<TxSarUpdateAppReq> &         tx_app_to_tx_sar_update_req,
                                  stream<TxSarToRxEngRsp> &           tx_sar_to_rx_eng_lookup_rsp,
                                  stream<TxSarToTxEngRsp> &           tx_sar_to_tx_eng_lookup_rsp,
                                  stream<TxSarToAppRsp> &             tx_sar_to_app_rsp) {
#pragma HLS PIPELINE II = 1

  static TxSarTableEntry tx_sar_table[TCP_MAX_SESSIONS];
#pragma HLS DEPENDENCE variable = tx_sar_table inter false
#pragma HLS RESOURCE variable = tx_sar_table core = RAM_T2P_BRAM

  // TX Engine
  if (!tx_eng_to_tx_sar_req.empty()) {
    TxEngToTxSarReq tx_eng_req = tx_eng_to_tx_sar_req.read();
    if (tx_eng_req.write) {
      if (!tx_eng_req.isRtQuery) {
        tx_sar_table[tx_eng_req.sessionID].not_ackd = tx_eng_req.not_ackd;
        if (tx_eng_req.init) {
          tx_sar_table[tx_eng_req.sessionID].app_written         = tx_eng_req.not_ackd;
          tx_sar_table[tx_eng_req.sessionID].ackd                = tx_eng_req.not_ackd - 1;
          tx_sar_table[tx_eng_req.sessionID].cong_window         = TCP_MSS_TEN_TIMES;
          tx_sar_table[tx_eng_req.sessionID].slowstart_threshold = 0xFFFF;
          tx_sar_table[tx_eng_req.sessionID].fin_is_ready        = tx_eng_req.fin_is_ready;
          tx_sar_table[tx_eng_req.sessionID].fin_is_sent         = tx_eng_req.fin_is_sent;
          // Init ACK to txAppInterface
#if !(TCP_NODELAY)
          tx_sar_to_app_rsp.write(TxSarToAppRsp(tx_eng_req.sessionID, tx_eng_req.not_ackd, 1));
#else
          tx_sar_to_app_rsp.write(
              TxSarToAppRsp(tx_eng_req.sessionID, tx_eng_req.not_ackd, TCP_MSS_TEN_TIMES, 1));
#endif
        }
        if (tx_eng_req.fin_is_ready) {
          tx_sar_table[tx_eng_req.sessionID].fin_is_ready = tx_eng_req.fin_is_ready;
        }
        if (tx_eng_req.fin_is_sent) {
          tx_sar_table[tx_eng_req.sessionID].fin_is_sent = tx_eng_req.fin_is_sent;
        }
      } else {
        TxSarRtReq txEngRtUpdate                               = tx_eng_req;
        tx_sar_table[tx_eng_req.sessionID].slowstart_threshold = txEngRtUpdate.getThreshold();
        // TODO is this correct or less, eg. 1/2 * MSS
        tx_sar_table[tx_eng_req.sessionID].cong_window = TCP_MSS_TEN_TIMES;
      }
    } else  // Read
    {
      TxSarTableEntry entry = tx_sar_table[tx_eng_req.sessionID];
      // Pre-calculated usedLength, min_transmit_window to improve timing in metaLoader
      // When calculating the usedLength we also consider if the FIN was already sent
      ap_uint<WINDOW_BITS> usedLength        = ((ap_uint<WINDOW_BITS>)entry.not_ackd - entry.ackd);
      ap_uint<WINDOW_BITS> usedLengthWithFIN = ((ap_uint<WINDOW_BITS>)entry.not_ackd - entry.ackd);
      if (entry.fin_is_sent) {
        usedLengthWithFIN--;
      }
      ap_uint<WINDOW_BITS> min_transmit_window;
#if !(WINDOW_SCALE)
      if (entry.cong_window < entry.recv_window) {
        min_transmit_window = entry.cong_window;
      } else {
        min_transmit_window = entry.recv_window;
      }
#else
      ap_uint<4> win_shift = entry.win_shift;
      ap_uint<30> scaled_recv_window;
      scaled_recv_window(win_shift + 15, win_shift) = entry.recv_window;
      if (entry.cong_window < scaled_recv_window) {
        min_transmit_window = entry.cong_window;
      } else {
        min_transmit_window = scaled_recv_window;
      }
#endif
      ap_uint<WINDOW_BITS> usableWindow = 0;
      if (min_transmit_window < usedLength) {
        usableWindow = min_transmit_window - usedLength;
      }
      tx_sar_to_tx_eng_lookup_rsp.write(TxSarToTxEngRsp(entry.ackd,
                                                        entry.not_ackd,
                                                        usableWindow,  // min_transmit_window,
                                                        entry.app_written,
                                                        usedLength,
                                                        entry.fin_is_ready,
                                                        entry.fin_is_sent));
    }
  }
  // TX App Stream If
  else if (!tx_app_to_tx_sar_update_req.empty())  // write only
  {
    TxSarUpdateAppReq tx_sar_upd_app_req                    = tx_app_to_tx_sar_update_req.read();
    tx_sar_table[tx_sar_upd_app_req.session_id].app_written = tx_sar_upd_app_req.app_written;
  }
  // RX Engine
  else if (!rx_eng_to_tx_sar_upd_req.empty()) {
    TxSarUpdateFromAckedSegReq new_acked_upd_req = rx_eng_to_tx_sar_upd_req.read();
    if (new_acked_upd_req.write) {
      tx_sar_table[new_acked_upd_req.session_id].ackd         = new_acked_upd_req.ackd;
      tx_sar_table[new_acked_upd_req.session_id].recv_window  = new_acked_upd_req.recv_window;
      tx_sar_table[new_acked_upd_req.session_id].cong_window  = new_acked_upd_req.cong_window;
      tx_sar_table[new_acked_upd_req.session_id].count        = new_acked_upd_req.count;
      tx_sar_table[new_acked_upd_req.session_id].fast_retrans = new_acked_upd_req.fast_retrans;

      TcpSessionBufferScale win_shift;
      if (new_acked_upd_req.init) {
        win_shift                                            = new_acked_upd_req.win_shift;
        tx_sar_table[new_acked_upd_req.session_id].win_shift = new_acked_upd_req.win_shift;
      } else {
        win_shift = tx_sar_table[new_acked_upd_req.session_id].win_shift;
      }

      // Push ACK to txAppInterface
#if !(TCP_NODELAY)
      tx_sar_to_app_rsp.write(TxSarToAppRsp(new_acked_upd_req.sessionID, new_acked_upd_req.ackd));
#else
      // right shift recv window
      ap_uint<30> scaled_recv_window;
      scaled_recv_window(win_shift + 15, win_shift) = new_acked_upd_req.recv_window;

      TcpSessionBuffer min_transmit_window;
      if (new_acked_upd_req.cong_window < scaled_recv_window)  // new_acked_upd_req.recv_window)
      {
        min_transmit_window = new_acked_upd_req.cong_window;
      } else {
        min_transmit_window = scaled_recv_window;
      }
      tx_sar_to_app_rsp.write(
          TxSarToAppRsp(new_acked_upd_req.session_id, new_acked_upd_req.ackd, min_transmit_window));
#endif
    } else {
      tx_sar_to_rx_eng_lookup_rsp.write(
          TxSarToRxEngRsp(tx_sar_table[new_acked_upd_req.session_id].ackd,
                          tx_sar_table[new_acked_upd_req.session_id].not_ackd,
                          tx_sar_table[new_acked_upd_req.session_id].cong_window,
                          tx_sar_table[new_acked_upd_req.session_id].slowstart_threshold,
                          tx_sar_table[new_acked_upd_req.session_id].count,
                          tx_sar_table[new_acked_upd_req.session_id].fast_retrans));
    }
  }
}
