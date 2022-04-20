
#include "state_table.hpp"

using namespace hls;

/** @ingroup state_table
 *  Stores the TCP connection state of each session. It is accessed
 *  from the @ref rx_engine, @ref tx_app_if and from @ref tx_engine.
 *  It also receives Session-IDs from the @ref close_timer, those sessions
 *  are closed and the IDs forwarded to the @ref session_lookup_controller which
 *  releases this ID.
 *  @param[in]		rx_eng_to_state_table_upd_req
 *  @param[in]		tx_app_to_state_table_upd_req
 *  @param[in]		tx_app_to_state_table_lup_req
 *  @param[in]		timer_to_state_table_release_session_req
 *  @param[out]		state_table_to_rx_eng_upd_rsp
 *  @param[out]		state_table_to_tx_app_upd_rsp
 *  @param[out]		state_table_to_tx_app_lup_rsp
 *  @param[out]		state_table_to_sessionlup_rsp
 */
void                 state_table(stream<StateTableReq> &rx_eng_to_state_table_upd_req,
                                 stream<StateTableReq> &tx_app_to_state_table_upd_req,
                                 stream<TcpSessionID> & tx_app_to_state_table_lup_req,
                                 stream<TcpSessionID> & timer_to_state_table_release_session_req,
                                 stream<SessionState> & state_table_to_rx_eng_upd_rsp,
                                 stream<SessionState> & state_table_to_tx_app_upd_rsp,
                                 stream<SessionState> & state_table_to_tx_app_lup_rsp,
                                 stream<TcpSessionID> & state_table_to_sessionlup_rsp) {
#pragma HLS PIPELINE II = 1

  static SessionState state_table[TCP_MAX_SESSIONS];
#pragma HLS RESOURCE variable = state_table core = RAM_2P_BRAM
#pragma HLS DEPENDENCE variable                  = state_table inter false

  static TcpSessionID stt_tx_locked_session_id;
  static TcpSessionID stt_rx_locked_session_id;
  static bool         stt_rx_session_locked = false;
  static bool         stt_tx_session_locked = false;

  static StateTableReq stt_tx_req;
  static StateTableReq stt_rx_req;
  static bool          stt_tx_wait = false;
  static bool          stt_rx_wait = false;

  static TcpSessionID stt_close_session_id;
  static bool         stt_close_wait = false;

  TcpSessionID session_id;

  // TX App If
  if (!tx_app_to_state_table_upd_req.empty() && !stt_tx_wait) {
    tx_app_to_state_table_upd_req.read(stt_tx_req);
    if ((stt_tx_req.session_id == stt_rx_locked_session_id) && stt_rx_session_locked)  // delay
    {
      stt_tx_wait = true;
    } else {
      if (stt_tx_req.write) {
        state_table[stt_tx_req.session_id] = stt_tx_req.state;
        stt_tx_session_locked              = false;
      } else {
        state_table_to_tx_app_upd_rsp.write(state_table[stt_tx_req.session_id]);
        // lock on every read
        stt_tx_locked_session_id = stt_tx_req.session_id;
        stt_tx_session_locked    = true;
      }
    }
  }
  // TX App Stream If
  else if (!tx_app_to_state_table_lup_req.empty()) {
    tx_app_to_state_table_lup_req.read(session_id);
    state_table_to_tx_app_lup_rsp.write(state_table[session_id]);
  }
  // RX Engine
  else if (!rx_eng_to_state_table_upd_req.empty() && !stt_rx_wait) {
    rx_eng_to_state_table_upd_req.read(stt_rx_req);
    if ((stt_rx_req.session_id == stt_tx_locked_session_id) && stt_tx_session_locked) {
      stt_rx_wait = true;
    } else {
      if (stt_rx_req.write) {
        // We check if it was not closed before, not sure if necessary
        if (stt_rx_req.state == CLOSED) {
          state_table_to_sessionlup_rsp.write(stt_rx_req.session_id);
        }
        state_table[stt_rx_req.session_id] = stt_rx_req.state;
        stt_rx_session_locked              = false;
      } else {
        state_table_to_rx_eng_upd_rsp.write(state_table[stt_rx_req.session_id]);
        stt_rx_locked_session_id = stt_rx_req.session_id;
        stt_rx_session_locked    = true;
      }
    }
  }
  // Timer release
  else if (!timer_to_state_table_release_session_req.empty() &&
           !stt_close_wait)  // can only be a close
  {
    timer_to_state_table_release_session_req.read(stt_close_session_id);
    // Check if locked
    if (((stt_close_session_id == stt_rx_locked_session_id) && stt_rx_session_locked) ||
        ((stt_close_session_id == stt_tx_locked_session_id) && stt_tx_session_locked)) {
      stt_close_wait = true;
    } else {
      state_table[stt_close_session_id] = CLOSED;
      state_table_to_sessionlup_rsp.write(stt_close_session_id);
    }
  } else if (stt_tx_wait) {
    if ((stt_tx_req.session_id != stt_rx_locked_session_id) || !stt_rx_session_locked) {
      if (stt_tx_req.write) {
        state_table[stt_tx_req.session_id] = stt_tx_req.state;
        stt_tx_session_locked              = false;
      } else {
        state_table_to_tx_app_upd_rsp.write(state_table[stt_tx_req.session_id]);
        stt_tx_locked_session_id = stt_tx_req.session_id;
        stt_tx_session_locked    = true;
      }
      stt_tx_wait = false;
    }
  } else if (stt_rx_wait) {
    if ((stt_rx_req.session_id != stt_tx_locked_session_id) || !stt_tx_session_locked) {
      if (stt_rx_req.write) {
        if (stt_rx_req.state == CLOSED) {
          state_table_to_sessionlup_rsp.write(stt_rx_req.session_id);
        }
        state_table[stt_rx_req.session_id] = stt_rx_req.state;
        stt_rx_session_locked              = false;
      } else {
        state_table_to_rx_eng_upd_rsp.write(state_table[stt_rx_req.session_id]);
        stt_rx_locked_session_id = stt_rx_req.session_id;
        stt_rx_session_locked    = true;
      }
      stt_rx_wait = false;
    }
  } else if (stt_close_wait) {
    if (((stt_close_session_id != stt_rx_locked_session_id) || !stt_rx_session_locked) &&
        ((stt_close_session_id != stt_tx_locked_session_id) || !stt_tx_session_locked)) {
      state_table[stt_close_session_id] = CLOSED;
      state_table_to_sessionlup_rsp.write(stt_close_session_id);
      stt_close_wait = false;
    }
  }
}
