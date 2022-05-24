
#include "toe/toe_conn.hpp"

using namespace hls;

/** @ingroup probe_timer
 *
 */
struct ProbeTimerEntry {
  ap_uint<32> time;
  bool        active;
};

/** @defgroup probe_timer Probe Timer
 *
 */
void probe_timer(stream<TcpSessionID> &rx_eng_to_timer_clear_ptimer,
                 stream<TcpSessionID> &tx_eng_to_timer_set_ptimer,
                 stream<Event> &       ptimer_to_event_eng_set_event);
