#include "tx_sar_table.hpp"
using namespace hls;
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/** @ingroup tx_sar_table
 *  This data structure stores the TX(transmitting) sliding window
 *  and handles concurrent access from the @ref rx_engine, @ref tx_app_if
 *  and @ref tx_engine
 */
void tx_sar_table(
    // rx eng r/w req/rsp
    stream<RxEngToTxSarReq> &rx_eng_to_tx_sar_req,
    stream<TxSarToRxEngRsp> &tx_sar_to_rx_eng_rsp,
    // tx eng r/w req/rsp
    stream<TxEngToTxSarReq> &tx_eng_to_tx_sar_req,
    stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
    // tx app update tx sar app_written
    stream<TxAppToTxSarReq> &tx_app_to_tx_sar_req,
    // tx sar update tx app table
    // TODO: this is not a response to tx app, should be rename here
    stream<TxSarToTxAppRsp> &tx_sar_to_tx_app_rsp) {
#pragma HLS LATENCY  max = 3
#pragma HLS PIPELINE II  = 1

  static TxSarTableEntry tx_sar_table[TCP_MAX_SESSIONS];
#pragma HLS bind_storage variable = tx_sar_table type = RAM_T2P impl = BRAM
#pragma HLS DEPENDENCE variable                                      = tx_sar_table inter false

  TxEngToTxSarReq        tx_eng_req;
  TxEngToTxSarRetransReq tx_eng_to_tx_sar_rt_req;

  RxEngToTxSarReq rx_eng_req;
  TxAppToTxSarReq tx_app_req;

  TxSarTableEntry  tmp_entry;
  TxSarToTxEngRsp  to_tx_eng_rsp;
  TcpSessionBuffer tx_sar_min_win;
  TcpSessionBuffer scaled_recv_window = 0;
  TxSarToTxAppRsp  to_tx_app_rsp;
  TxSarToRxEngRsp  to_rx_eng_rsp;
  // TX Engine read or write
  if (!tx_eng_to_tx_sar_req.empty()) {
    tx_eng_to_tx_sar_req.read(tx_eng_req);
    logger.Info(TX_ENGINE, TX_SAR_TB, "R/W Req", tx_eng_req.to_string());
    if (tx_eng_req.write) {
      if (!tx_eng_req.retrans_req) {
        tx_sar_table[tx_eng_req.session_id].not_ackd = tx_eng_req.not_ackd;
        if (tx_eng_req.init) {
          tx_sar_table[tx_eng_req.session_id].app_written         = tx_eng_req.not_ackd;
          tx_sar_table[tx_eng_req.session_id].ackd                = tx_eng_req.not_ackd - 1;
          tx_sar_table[tx_eng_req.session_id].cong_window         = TCP_MSS_TEN_TIMES;
          tx_sar_table[tx_eng_req.session_id].slowstart_threshold = (BUFFER_SIZE - 1);
          tx_sar_table[tx_eng_req.session_id].fin_is_ready        = tx_eng_req.fin_is_ready;
          tx_sar_table[tx_eng_req.session_id].fin_is_sent         = tx_eng_req.fin_is_sent;
          // Init ACK to tx app table
#if !(TCP_NODELAY)
          to_tx_app_rsp = TxSarToTxAppRsp(tx_eng_req.session_id, tx_eng_req.not_ackd, 1);
#else
          to_tx_app_rsp =
              TxSarToTxAppRsp(tx_eng_req.session_id, tx_eng_req.not_ackd, TCP_MSS_TEN_TIMES, 1);

#endif
          logger.Info(TX_SAR_TB, TX_APP_IF, "Upd TxAppTable", to_tx_app_rsp.to_string());
          tx_sar_to_tx_app_rsp.write(to_tx_app_rsp);
        }
        if (tx_eng_req.fin_is_ready) {
          tx_sar_table[tx_eng_req.session_id].fin_is_ready = tx_eng_req.fin_is_ready;
        }
        if (tx_eng_req.fin_is_sent) {
          tx_sar_table[tx_eng_req.session_id].fin_is_sent = tx_eng_req.fin_is_sent;
        }
      } else {
        tx_eng_to_tx_sar_rt_req = tx_eng_req;
        tx_sar_table[tx_eng_req.session_id].slowstart_threshold =
            tx_eng_to_tx_sar_rt_req.get_threshold();
        // maybe less
        tx_sar_table[tx_eng_req.session_id].cong_window = TCP_MSS_TEN_TIMES;
      }
    } else
    // tx engine read
    {
      tmp_entry = tx_sar_table[tx_eng_req.session_id];

      to_tx_eng_rsp.ackd         = tmp_entry.ackd;
      to_tx_eng_rsp.not_ackd     = tmp_entry.not_ackd;
      to_tx_eng_rsp.app_written  = tmp_entry.app_written;
      to_tx_eng_rsp.fin_is_ready = tmp_entry.fin_is_ready;
      to_tx_eng_rsp.fin_is_sent  = tmp_entry.fin_is_sent;
      to_tx_eng_rsp.curr_length  = tmp_entry.app_written - tmp_entry.not_ackd(WINDOW_BITS - 1, 0);
      to_tx_eng_rsp.used_length  = tmp_entry.not_ackd(WINDOW_BITS - 1, 0) - tmp_entry.ackd;
      to_tx_eng_rsp.used_length_rst =
          tmp_entry.not_ackd(WINDOW_BITS - 1, 0) - tmp_entry.ackd - tmp_entry.fin_is_sent;

      to_tx_eng_rsp.ackd_eq_not_ackd  = (tmp_entry.ackd == tmp_entry.not_ackd);
      to_tx_eng_rsp.not_ackd_plus_mss = tmp_entry.not_ackd + TCP_MSS;

      scaled_recv_window(tmp_entry.win_shift + 15, tmp_entry.win_shift) = tmp_entry.recv_window;
      if (tmp_entry.cong_window < scaled_recv_window) {
        tx_sar_min_win = tmp_entry.cong_window;
      } else {
        tx_sar_min_win = scaled_recv_window;
      }

      to_tx_eng_rsp.win_shift = tmp_entry.win_shift;

      if (tx_sar_min_win > to_tx_eng_rsp.used_length) {
        to_tx_eng_rsp.usable_window = tx_sar_min_win - to_tx_eng_rsp.used_length;
      } else {
        to_tx_eng_rsp.usable_window = 0;
      }
      to_tx_eng_rsp.min_window     = tx_sar_min_win;
      to_tx_eng_rsp.not_ackd_short = tmp_entry.not_ackd + to_tx_eng_rsp.curr_length;
      logger.Info(TX_SAR_TB, TX_ENGINE, "Lup rsp", to_tx_eng_rsp.to_string());
      tx_sar_to_tx_eng_rsp.write(to_tx_eng_rsp);
    }
  }
  // TX App update app written only
  else if (!tx_app_to_tx_sar_req.empty()) {
    tx_app_to_tx_sar_req.read(tx_app_req);
    logger.Info(TX_APP_IF, TX_SAR_TB, "Upd AppRead Req", tx_app_req.to_string());

    tx_sar_table[tx_app_req.session_id].app_written = tx_app_req.app_written;
  }
  // RX Engine read or write
  else if (!rx_eng_to_tx_sar_req.empty()) {
    rx_eng_to_tx_sar_req.read(rx_eng_req);
    logger.Info(RX_ENGINE, TX_SAR_TB, "R/W Req", rx_eng_req.to_string());

    if (rx_eng_req.write) {
      if (rx_eng_req.win_shift_write) {
        tx_sar_table[rx_eng_req.session_id].win_shift = rx_eng_req.win_shift;
      }

      tx_sar_table[rx_eng_req.session_id].ackd          = rx_eng_req.ackd;
      tx_sar_table[rx_eng_req.session_id].recv_window   = rx_eng_req.recv_window;
      tx_sar_table[rx_eng_req.session_id].cong_window   = rx_eng_req.cong_window;
      tx_sar_table[rx_eng_req.session_id].retrans_count = rx_eng_req.retrans_count;
      tx_sar_table[rx_eng_req.session_id].fast_retrans  = rx_eng_req.fast_retrans;

#if (!TCP_NODELAY)
      to_tx_app_rsp = TxSarToTxAppRsp(rx_eng_req.session_id, rx_eng_req.ackd);
#else
      scaled_recv_window(rx_eng_req.win_shift + 15, rx_eng_req.win_shift) = rx_eng_req.recv_window;
      if (rx_eng_req.cong_window < scaled_recv_window) {
        tx_sar_min_win = rx_eng_req.cong_window;
      } else {
        tx_sar_min_win = scaled_recv_window;
      }
      to_tx_app_rsp = TxSarToTxAppRsp(rx_eng_req.session_id, rx_eng_req.ackd, tx_sar_min_win);
#endif
      logger.Info(TX_SAR_TB, TX_APP_IF, "Upd TxAppTable", to_tx_app_rsp.to_string());
      tx_sar_to_tx_app_rsp.write(to_tx_app_rsp);
    } else {
      to_rx_eng_rsp = TxSarToRxEngRsp(tx_sar_table[rx_eng_req.session_id].ackd,
                                      tx_sar_table[rx_eng_req.session_id].not_ackd,
                                      tx_sar_table[rx_eng_req.session_id].cong_window,
                                      tx_sar_table[rx_eng_req.session_id].slowstart_threshold,
                                      tx_sar_table[rx_eng_req.session_id].retrans_count,
                                      tx_sar_table[rx_eng_req.session_id].fast_retrans,
                                      tx_sar_table[rx_eng_req.session_id].win_shift);
      logger.Info(TX_SAR_TB, RX_ENGINE, "Lup Rsp", to_rx_eng_rsp.to_string());
      tx_sar_to_rx_eng_rsp.write(to_rx_eng_rsp);
    }
  }
}
