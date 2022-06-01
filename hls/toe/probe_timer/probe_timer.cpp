#include "probe_timer.hpp"
using namespace hls;
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/** @ingroup probe_timer
 *  Reads in the Session-ID and activates. a timer with an interval of 0.05 seconds. When the timer
 *times out a RT Event is fired to the @ref tx_engine. In case of a zero-window (or too small
 *window) an RT Event will generate a packet without payload which is the same as a probing packet.
 *	@param[in]		tx_eng_to_timer_set_ptimer
 *	@param[out]		ptimer_to_event_eng_set_event
 */
void                  probe_timer(stream<TcpSessionID> &rx_eng_to_timer_clear_ptimer,
                                  stream<TcpSessionID> &tx_eng_to_timer_set_ptimer,
                                  stream<Event> &       ptimer_to_event_eng_set_event) {
#pragma HLS aggregate variable = ptimer_to_event_eng_set_event compact = bit

#pragma HLS PIPELINE II = 1

  static ProbeTimerEntry probe_timer_table[TCP_MAX_SESSIONS];
#pragma HLS bind_storage variable = probe_timer_table type = RAM_T2P impl = BRAM
#pragma HLS aggregate variable = probe_timer_table compact = bit
#pragma HLS DEPENDENCE variable                            = probe_timer_table inter false

  static TcpSessionID ptimer_cur_check_session_id = 0;
  static TcpSessionID ptimer_set_session_id       = 0;
  static TcpSessionID ptimer_prev_session_id      = 0;
  static bool         ptimer_wait_for_write       = false;

  TcpSessionID ptimer_cur_session_id;
  bool         ptimer_cur_id_to_be_clear = false;
  Event        to_event_eng;

  if (ptimer_wait_for_write) {
    if (ptimer_set_session_id != ptimer_prev_session_id) {
      probe_timer_table[ptimer_set_session_id].time   = TIME_50ms;
      probe_timer_table[ptimer_set_session_id].active = true;
      ptimer_wait_for_write                           = false;
    }
    ptimer_prev_session_id--;
  } else if (!tx_eng_to_timer_set_ptimer.empty()) {
    tx_eng_to_timer_set_ptimer.read(ptimer_set_session_id);
    logger.Info("Tx Engine set PTimer", ptimer_set_session_id.to_string(16), false);
    ptimer_wait_for_write = true;
  } else {
    ptimer_cur_session_id  = ptimer_cur_check_session_id;
    ptimer_prev_session_id = ptimer_cur_check_session_id;

    if (!rx_eng_to_timer_clear_ptimer.empty()) {
      rx_eng_to_timer_clear_ptimer.read(ptimer_cur_session_id);
      logger.Info("Rx Engine clear PTimer", ptimer_set_session_id.to_string(16), false);

      ptimer_cur_id_to_be_clear = true;
    } else {
      ptimer_cur_check_session_id++;
      if (ptimer_cur_check_session_id == TCP_MAX_SESSIONS) {
        ptimer_cur_check_session_id = 0;
      }
    }

    // Check if 0, otherwise decrement
    if (probe_timer_table[ptimer_cur_session_id].active && !ptimer_to_event_eng_set_event.full()) {
      if (probe_timer_table[ptimer_cur_session_id].time == 0 || ptimer_cur_id_to_be_clear) {
        probe_timer_table[ptimer_cur_session_id].time   = 0;
        probe_timer_table[ptimer_cur_session_id].active = false;
        // It's not an RT, we want to resume TX
#if !(TCP_NODELAY)
        to_event_eng = Event(TX, ptimer_cur_session_id);
#else
        to_event_eng = Event(RT, ptimer_cur_session_id);

#endif
        logger.Info("PTimer to Event engine", to_event_eng.to_string(), false);
        ptimer_to_event_eng_set_event.write(to_event_eng);

        ptimer_cur_id_to_be_clear = false;
      } else {
        probe_timer_table[ptimer_cur_session_id].time -= 1;
      }
    }
  }
}
