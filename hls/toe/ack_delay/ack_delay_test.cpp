
#include "ack_delay.hpp"
#include "toe/mock/mock_toe.hpp"

#include <iostream>

using namespace hls;

void EmptyFifos(std::ofstream &         out_stream,
                stream<EventWithTuple> &ack_delay_to_tx_eng_event,
                int                     sim_cycle) {
  EventWithTuple out_event;

  while (!ack_delay_to_tx_eng_event.empty()) {
    ack_delay_to_tx_eng_event.read(out_event);
    out_stream << "Cycle " << std::dec << sim_cycle << ": AckDelay to Tx Engine Event\n";
    out_stream << out_event.to_string() << "\n";
  }
}

int main() {
  stream<EventWithTuple> event_eng_to_ack_delay_event;
  stream<EventWithTuple> ack_delay_to_tx_eng_event;
  stream<ap_uint<1> >    ack_delay_read_cnt_fifo;
  stream<ap_uint<1> >    ack_delay_write_cnt_fifo;

  EventWithTuple ev;

  // open output file
  std::ofstream outputFile;

  outputFile.open("./out.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }

  int sim_cycle = 0;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        ev = EventWithTuple(Event(ACK, 1), mock_tuple);
        event_eng_to_ack_delay_event.write(ev);
        break;
      case 2:
        ev = EventWithTuple(Event(ACK, 1), mock_tuple);
        event_eng_to_ack_delay_event.write(ev);
        break;
      case 3:
        ev = EventWithTuple(Event(ACK, 1), mock_tuple);
        event_eng_to_ack_delay_event.write(ev);
        break;
      case 4:
        ev = EventWithTuple(Event(RST, 2), mock_tuple);
        event_eng_to_ack_delay_event.write(ev);
        break;
      default:
        break;
    }
    ack_delay(event_eng_to_ack_delay_event,
              ack_delay_to_tx_eng_event,
              ack_delay_read_cnt_fifo,
              ack_delay_write_cnt_fifo);
    EmptyFifos(outputFile, ack_delay_to_tx_eng_event, sim_cycle);
    sim_cycle++;
  }

  return 0;
}
