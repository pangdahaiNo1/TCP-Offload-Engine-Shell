#include "toe/toe_conn.hpp"

using namespace hls;

/** @ingroup close_timer
 *
 */
struct CloseTimerEntry {
  ap_uint<32> time;
  bool        active;
};

/** @defgroup close_timer Close Timer
 *
 */
void close_timer(stream<TcpSessionID> &rx_eng_to_timer_set_ctimer,
                 stream<TcpSessionID> &ctimer_to_sttable_release_state);
