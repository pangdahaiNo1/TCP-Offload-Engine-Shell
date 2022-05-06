
#include "probe_timer.hpp"
#include "toe/mock/mock_toe.hpp"

#include <iostream>

using namespace hls;

void EmptyFifos(std::ofstream &out_stream,
                stream<Event> &ptimer_to_event_eng_set_event,
                int            sim_cycle) {
  Event ev;

  while (!ptimer_to_event_eng_set_event.empty()) {
    ptimer_to_event_eng_set_event.read(ev);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Probe Timer to Event Eng\n";
    out_stream << ev.to_string() << "\n";
  }
}

int main() {
  stream<TcpSessionID> rx_eng_to_timer_clear_ptimer;
  stream<TcpSessionID> tx_eng_to_timer_set_ptimer;
  stream<Event>        ptimer_to_event_eng_set_event;

  std::ofstream outfile_stream;

  outfile_stream.open("./out.dat");
  if (!outfile_stream) {
    std::cout << "Error: could not open test output file." << std::endl;
  }
  int          sim_cycle   = 0;
  TcpSessionID to_timer_id = 1;
  while (sim_cycle < 12800) {
    switch (sim_cycle) {
      case 1:
        rx_eng_to_timer_clear_ptimer.write(to_timer_id);
        break;
      case 2:
        to_timer_id = 0x2;
        tx_eng_to_timer_set_ptimer.write(to_timer_id);
        break;
      case 10:
        to_timer_id = 0x2;
        rx_eng_to_timer_clear_ptimer.write(to_timer_id);
        break;

      default:
        break;
    }
    probe_timer(
        rx_eng_to_timer_clear_ptimer, tx_eng_to_timer_set_ptimer, ptimer_to_event_eng_set_event);
    EmptyFifos(outfile_stream, ptimer_to_event_eng_set_event, sim_cycle);

    sim_cycle++;
  }

  return 0;
}
