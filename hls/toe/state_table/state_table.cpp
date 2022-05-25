
#include "state_table.hpp"

using namespace hls;

/** @ingroup state_table
 *  Stores the TCP connection state of each session. It is accessed
 *  from the @ref rx_engine, @ref tx_app_if and from @ref tx_engine.
 *  It also receives Session-IDs from the @ref close_timer, those sessions
 *  are closed and the IDs forwarded to the @ref session_lookup_controller which
 *  releases this ID.
 *  @param[in]		rx_eng_to_sttable_req read or write
 *  @param[in]		tx_app_to_sttable_req read or write
 *  @param[in]		tx_app_to_sttable_lup_req only read
 *  @param[in]		sttable_to_slookup_release_state release session id
 *  @param[out]		sttable_to_rx_eng_rsp read response
 *  @param[out]		sttable_to_tx_app_rsp read response
 *  @param[out]		sttable_to_tx_app_lup_rsp
 *  @param[out]		sttable_to_slookup_release_req release session id
 */
void                 state_table(stream<StateTableReq> &rx_eng_to_sttable_req,
                                 stream<StateTableReq> &tx_app_to_sttable_req,
                                 stream<TcpSessionID> & tx_app_to_sttable_lup_req,
                                 stream<TcpSessionID> & sttable_to_slookup_release_state,
                                 stream<SessionState> & sttable_to_rx_eng_rsp,
                                 stream<SessionState> & sttable_to_tx_app_rsp,
                                 stream<SessionState> & sttable_to_tx_app_lup_rsp,
                                 stream<TcpSessionID> & sttable_to_slookup_release_req) {
#pragma HLS PIPELINE II = 1

  static SessionState state_table[TCP_MAX_SESSIONS];
#pragma HLS bind_storage variable = state_table type = RAM_2P impl = BRAM
#pragma HLS DEPENDENCE variable                                    = state_table inter false

  static TcpSessionID tx_app_rw_req_session_id;
  static TcpSessionID rx_eng_rw_req_session_id;
  static bool         rx_eng_rw_req_session_id_locked = false;
  static bool         tx_app_rw_req_session_id_locked = false;

  static StateTableReq tx_app_rw_req;
  static StateTableReq rx_eng_rw_req;
  static bool          tx_app_rw_locked = false;
  static bool          rx_eng_rw_locked = false;

  static TcpSessionID timer_release_req_session_id;
  static bool         timer_release_locked = false;

  TcpSessionID session_id;

  // TX App connection handler, write or write
  if (!tx_app_to_sttable_req.empty() && !tx_app_rw_locked) {
    tx_app_to_sttable_req.read(tx_app_rw_req);
    if ((tx_app_rw_req.session_id == rx_eng_rw_req_session_id) && rx_eng_rw_req_session_id_locked) {
      tx_app_rw_locked = true;
    } else {
      if (tx_app_rw_req.write) {
        state_table[tx_app_rw_req.session_id] = tx_app_rw_req.state;
        tx_app_rw_req_session_id_locked       = false;
      } else {
        sttable_to_tx_app_rsp.write(state_table[tx_app_rw_req.session_id]);
        // lock on every read
        tx_app_rw_req_session_id        = tx_app_rw_req.session_id;
        tx_app_rw_req_session_id_locked = true;
      }
    }
  }
  // TX App Stream If, read only
  else if (!tx_app_to_sttable_lup_req.empty()) {
    tx_app_to_sttable_lup_req.read(session_id);
    sttable_to_tx_app_lup_rsp.write(state_table[session_id]);
  }
  // RX Engine, read or write
  else if (!rx_eng_to_sttable_req.empty() && !rx_eng_rw_locked) {
    rx_eng_to_sttable_req.read(rx_eng_rw_req);
    if ((rx_eng_rw_req.session_id == tx_app_rw_req_session_id) && tx_app_rw_req_session_id_locked) {
      rx_eng_rw_locked = true;
    } else {
      if (rx_eng_rw_req.write) {
        // We check if it was not closed before, not sure if necessary
        if (rx_eng_rw_req.state == CLOSED) {
          sttable_to_slookup_release_req.write(rx_eng_rw_req.session_id);
        }
        state_table[rx_eng_rw_req.session_id] = rx_eng_rw_req.state;
        rx_eng_rw_req_session_id_locked       = false;
      } else {
        sttable_to_rx_eng_rsp.write(state_table[rx_eng_rw_req.session_id]);
        rx_eng_rw_req_session_id        = rx_eng_rw_req.session_id;
        rx_eng_rw_req_session_id_locked = true;
      }
    }
  }
  // Timer to release session
  else if (!sttable_to_slookup_release_state.empty() && !timer_release_locked) {
    sttable_to_slookup_release_state.read(timer_release_req_session_id);
    // Check if locked
    if (((timer_release_req_session_id == rx_eng_rw_req_session_id) &&
         rx_eng_rw_req_session_id_locked) ||
        ((timer_release_req_session_id == tx_app_rw_req_session_id) &&
         tx_app_rw_req_session_id_locked)) {
      timer_release_locked = true;
    } else {
      state_table[timer_release_req_session_id] = CLOSED;
      sttable_to_slookup_release_req.write(timer_release_req_session_id);
    }
  } else if (tx_app_rw_locked) {
    if ((tx_app_rw_req.session_id != rx_eng_rw_req_session_id) ||
        !rx_eng_rw_req_session_id_locked) {
      if (tx_app_rw_req.write) {
        state_table[tx_app_rw_req.session_id] = tx_app_rw_req.state;
        tx_app_rw_req_session_id_locked       = false;
      } else {
        sttable_to_tx_app_rsp.write(state_table[tx_app_rw_req.session_id]);
        tx_app_rw_req_session_id        = tx_app_rw_req.session_id;
        tx_app_rw_req_session_id_locked = true;
      }
      tx_app_rw_locked = false;
    }
  } else if (rx_eng_rw_locked) {
    if ((rx_eng_rw_req.session_id != tx_app_rw_req_session_id) ||
        !tx_app_rw_req_session_id_locked) {
      if (rx_eng_rw_req.write) {
        if (rx_eng_rw_req.state == CLOSED) {
          sttable_to_slookup_release_req.write(rx_eng_rw_req.session_id);
        }
        state_table[rx_eng_rw_req.session_id] = rx_eng_rw_req.state;
        rx_eng_rw_req_session_id_locked       = false;
      } else {
        sttable_to_rx_eng_rsp.write(state_table[rx_eng_rw_req.session_id]);
        rx_eng_rw_req_session_id        = rx_eng_rw_req.session_id;
        rx_eng_rw_req_session_id_locked = true;
      }
      rx_eng_rw_locked = false;
    }
  } else if (timer_release_locked) {
    if (((timer_release_req_session_id != rx_eng_rw_req_session_id) ||
         !rx_eng_rw_req_session_id_locked) &&
        ((timer_release_req_session_id != tx_app_rw_req_session_id) ||
         !tx_app_rw_req_session_id_locked)) {
      state_table[timer_release_req_session_id] = CLOSED;
      sttable_to_slookup_release_req.write(timer_release_req_session_id);
      timer_release_locked = false;
    }
  }
}
