#include "close_timer.hpp"

using namespace hls;

/** @ingroup close_timer
 *  Reads in Session-IDs, the corresponding is kept in the TIME-WAIT state for 60s before
 *  it gets closed by writing its ID into the closeTimerReleaseFifo.
 *  @param[in]		rx_eng_to_timer_set_ctimer, FIFO containing Session-ID of the sessions which are
 * in the TIME-WAIT state
 *  @param[out]		ctimer_to_sttable_release_state, write Sessions which are closed into this FIFO
 */
void                 close_timer(stream<TcpSessionID> &rx_eng_to_timer_set_ctimer,
                                 stream<TcpSessionID> &ctimer_to_sttable_release_state) {
#pragma HLS PIPELINE II = 1

  static CloseTimerEntry close_timer_table[TCP_MAX_SESSIONS];
#pragma HLS bind_storage variable = close_timer_table type = RAM_T2P impl = BRAM
#pragma HLS aggregate variable = close_timer_table compact = bit
#pragma HLS DEPENDENCE variable                            = close_timer_table inter false

  static TcpSessionID ctimer_cur_session_id  = 0;
  static TcpSessionID ctimer_set_session_id  = 0;
  static TcpSessionID ctimer_prev_session_id = 0;
  static bool         ctimer_wait_for_write  = false;

  if (ctimer_wait_for_write) {
    if (ctimer_set_session_id != ctimer_prev_session_id) {
      close_timer_table[ctimer_set_session_id].time   = TIME_60s;
      close_timer_table[ctimer_set_session_id].active = true;
      ctimer_wait_for_write                           = false;
    }
    // cout << "Prev "<< ctimer_prev_session_id << " Set "<< ctimer_set_session_id<<endl;
    ctimer_prev_session_id--;
  } else if (!rx_eng_to_timer_set_ctimer.empty()) {
    rx_eng_to_timer_set_ctimer.read(ctimer_set_session_id);
    ctimer_wait_for_write = true;
  } else {
    ctimer_prev_session_id = ctimer_cur_session_id;
    // Check if 0, otherwise decrement
    if (close_timer_table[ctimer_cur_session_id].active) {
      if (close_timer_table[ctimer_cur_session_id].time > 0) {
        close_timer_table[ctimer_cur_session_id].time -= 1;
      } else {
        close_timer_table[ctimer_cur_session_id].time   = 0;
        close_timer_table[ctimer_cur_session_id].active = false;
        ctimer_to_sttable_release_state.write(ctimer_cur_session_id);
      }
    }

    ctimer_cur_session_id++;
    if (ctimer_cur_session_id == TCP_MAX_SESSIONS) {
      ctimer_cur_session_id = 0;
    }
  }
}
