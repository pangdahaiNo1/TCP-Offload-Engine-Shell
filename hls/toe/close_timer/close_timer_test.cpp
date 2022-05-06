
#include "close_timer.hpp"
#include "toe/mock/mock_toe.hpp"
using namespace hls;

void EmptyFifos(std::ofstream &       out_stream,
                stream<TcpSessionID> &ctimer_to_sttable_release_state,
                int                   sim_cycle) {
  TcpSessionID session_id;

  while (!ctimer_to_sttable_release_state.empty()) {
    ctimer_to_sttable_release_state.read(session_id);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Close Timer to State Table for release\n";
    out_stream << session_id.to_string(16) << "\n";
  }
}

int main() {
  stream<TcpSessionID> rx_eng_to_timer_set_ctimer;
  stream<TcpSessionID> ctimer_to_sttable_release_state;

  std::ofstream outfile_stream;

  outfile_stream.open("./out.dat");
  if (!outfile_stream) {
    std::cout << "Error: could not open test output file." << std::endl;
  }
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
    EmptyFifos(outfile_stream, ctimer_to_sttable_release_state, sim_cycle);
    sim_cycle++;
  }

  return 0;
}
