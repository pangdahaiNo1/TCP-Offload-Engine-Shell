
#include "probe_timer.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"

using namespace hls;

void EmptyFifos(MockLogger &logger, stream<Event> &ptimer_to_event_eng_set_event) {
  Event ev;

  while (!ptimer_to_event_eng_set_event.empty()) {
    ptimer_to_event_eng_set_event.read(ev);
    logger.Info(PROBE_TIMER, EVENT_ENG, "Event", ev.to_string(), false);
  }
}
MockLogger logger("./ptimer_inner.log", PROBE_TIMER);

int main() {
  stream<TcpSessionID> rx_eng_to_timer_clear_ptimer;
  stream<TcpSessionID> tx_eng_to_timer_set_ptimer;
  stream<Event>        ptimer_to_event_eng_set_event;

  MockLogger top_logger("./ptimer_top.log", PROBE_TIMER);

  int          sim_cycle   = 0;
  TcpSessionID to_timer_id = 0;
  while (sim_cycle < 12800) {
    switch (sim_cycle) {
      case 1:
        // tx engine set
        to_timer_id = 0x1;
        tx_eng_to_timer_set_ptimer.write(to_timer_id);
        break;
      case 2:
        // tx engine set
        to_timer_id = 0x2;
        tx_eng_to_timer_set_ptimer.write(to_timer_id);
        break;
      case 10:
        // rx engine clear session 2
        to_timer_id = 0x2;
        rx_eng_to_timer_clear_ptimer.write(to_timer_id);
        break;

      default:
        break;
    }
    probe_timer(
        rx_eng_to_timer_clear_ptimer, tx_eng_to_timer_set_ptimer, ptimer_to_event_eng_set_event);
    EmptyFifos(top_logger, ptimer_to_event_eng_set_event);

    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}
