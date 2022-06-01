#include "event_engine.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"

using namespace hls;

void EmptyFifos(MockLogger &logger, stream<EventWithTuple> &event_eng_to_ack_delay_event) {
  EventWithTuple out_event;

  while (!event_eng_to_ack_delay_event.empty()) {
    event_eng_to_ack_delay_event.read(out_event);
    logger.Info("EventEngine to Ack delay Event", out_event.to_string(), true);
  }
}

MockLogger logger("./inner_out.dat");
int        main() {
  stream<Event>          tx_app_to_event_eng_set_event;
  stream<EventWithTuple> rx_eng_to_event_eng_set_event;
  stream<Event>          timer_to_event_eng_set_event;
  stream<EventWithTuple> event_eng_to_ack_delay_event;
  stream<ap_uint<1> >    ack_delay_read_count_fifo;
  stream<ap_uint<1> >    ack_delay_write_count_fifo;
  stream<ap_uint<1> >    tx_eng_read_count_fifo;

  MockLogger     top_logger("./top_out.dat");
  EventWithTuple ev;

  int sim_cycle = 0;
  while (sim_cycle < 20) {
    switch (sim_cycle) {
      case 1:
        ev = EventWithTuple(Event(ACK, 1), mock_tuple);
        rx_eng_to_event_eng_set_event.write(ev);
        break;
      case 2:
        ack_delay_read_count_fifo.write(1);
        ev = EventWithTuple(Event(ACK, 2), mock_tuple);
        timer_to_event_eng_set_event.write(ev);
        break;
      case 3:
        ack_delay_read_count_fifo.write(1);
        ev = EventWithTuple(Event(ACK, 2), mock_tuple);
        tx_app_to_event_eng_set_event.write(ev);
        break;
      default:
        break;
    }
    event_engine(tx_app_to_event_eng_set_event,
                 rx_eng_to_event_eng_set_event,
                 timer_to_event_eng_set_event,
                 event_eng_to_ack_delay_event,
                 ack_delay_read_count_fifo,
                 ack_delay_write_count_fifo,
                 tx_eng_read_count_fifo

    );
    EmptyFifos(top_logger, event_eng_to_ack_delay_event);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
  return 0;
}
