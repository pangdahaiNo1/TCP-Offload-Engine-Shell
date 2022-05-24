#include "toe/toe_conn.hpp"

using namespace hls;

/** @ingroup retransmit_timer
 *
 */
struct RetransmitTimerEntry {
  ap_uint<32> time;
  ap_uint<3>  retries;
  bool        active;
  EventType   type;
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Time: " << time.to_string(10) << "\t";
    sstream << "Retrans: " << retries.to_string(10) << "\t";
    sstream << "Active: " << (active ? "Y" : "N") << "\t";
    sstream << "Event Type: " << time.to_string(10) << "\t";
    return sstream.str();
  }
#endif
};

/** @defgroup retransmit_timer Retransmit Timer
 *
 */
void retransmit_timer(stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
                      stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
                      stream<Event> &                 rtimer_to_event_eng_set_event,
                      stream<TcpSessionID> &          rtimer_to_sttable_release_state,
                      stream<AppNotification> &       rtimer_to_rx_app_notification,
                      stream<OpenSessionStatus> &     rtimer_to_tx_app_notification);
