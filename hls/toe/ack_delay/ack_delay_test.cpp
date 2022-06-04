
#include "ack_delay.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"

#include <iostream>

using namespace hls;

void EmptyFifos(MockLogger &logger, stream<EventWithTuple> &ack_delay_to_tx_eng_event) {
  EventWithTuple out_event;

  while (!ack_delay_to_tx_eng_event.empty()) {
    ack_delay_to_tx_eng_event.read(out_event);
    logger.Info(ACK_DELAY, EVENT_ENG, "Event", out_event.to_string(), false);
  }
}
MockLogger logger("./ack_delay_inner.log", ACK_DELAY);
int        main() {
  stream<EventWithTuple> event_eng_to_ack_delay_event;
  stream<EventWithTuple> ack_delay_to_tx_eng_event;
  stream<ap_uint<1> >    ack_delay_read_cnt_fifo;
  stream<ap_uint<1> >    ack_delay_write_cnt_fifo;

  EventWithTuple ev;

  MockLogger top_logger("./ack_delay_top.log", ACK_DELAY);

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
    EmptyFifos(top_logger, ack_delay_to_tx_eng_event);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}
