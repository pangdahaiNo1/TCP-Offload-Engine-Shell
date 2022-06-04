
#include "close_timer.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"
using namespace hls;

void EmptyFifos(MockLogger &logger, stream<TcpSessionID> &ctimer_to_sttable_release_state) {
  TcpSessionID session_id;

  while (!ctimer_to_sttable_release_state.empty()) {
    ctimer_to_sttable_release_state.read(session_id);
    logger.Info(CLOSE_TIMER, STATE_TABLE, "Release Session", session_id.to_string(16), false);
  }
}

MockLogger logger("./closet_timer_inner.log", CLOSE_TIMER);

int main() {
  stream<TcpSessionID> rx_eng_to_timer_set_ctimer;
  stream<TcpSessionID> ctimer_to_sttable_release_state;

  MockLogger   top_logger("./closet_timer_top.log", CLOSE_TIMER);
  int          sim_cycle    = 0;
  TcpSessionID to_ctimer_id = 1;
  while (sim_cycle < 12800) {
    switch (sim_cycle) {
      case 1:
        rx_eng_to_timer_set_ctimer.write(to_ctimer_id);
        break;
      case 2:
        to_ctimer_id = 0x2;
        rx_eng_to_timer_set_ctimer.write(to_ctimer_id);
        break;

      default:
        break;
    }
    close_timer(rx_eng_to_timer_set_ctimer, ctimer_to_sttable_release_state);
    EmptyFifos(top_logger, ctimer_to_sttable_release_state);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}
