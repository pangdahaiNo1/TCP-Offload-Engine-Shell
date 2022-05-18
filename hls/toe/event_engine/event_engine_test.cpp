#include "event_engine.hpp"

#include <iostream>

using namespace hls;

int main() {
  stream<Event>          txApp2eventEng_setEvent("txApp2eventEng_setEvent");
  stream<EventWithTuple> rxEng2eventEng_setEvent("rxEng2eventEng_setEvent");
  stream<Event>          timer2eventEng_setEvent("timer2eventEng_setEvent");
  stream<EventWithTuple> eventEng2txEng_event("eventEng2txEng_event");
  stream<ap_uint<1> >    ackDelayFifoReadCount("ackDelayFifoReadCount");
  stream<ap_uint<1> >    ackDelayFifoWriteCount("ackDelayFifoWriteCount");
  stream<ap_uint<1> >    txEngFifoReadCount("txEngFifoReadCount");

  EventWithTuple ev;
  int            count = 0;
  while (count < 50) {
    event_engine(txApp2eventEng_setEvent,
                 rxEng2eventEng_setEvent,
                 timer2eventEng_setEvent,
                 eventEng2txEng_event,
                 ackDelayFifoReadCount,
                 ackDelayFifoWriteCount,
                 txEngFifoReadCount);

    if (count == 20) {
      FourTuple tuple;
      tuple.src_ip_addr  = 0x0101010a;
      tuple.src_tcp_port = 12;
      tuple.dst_ip_addr  = 0x0101010b;
      tuple.dst_tcp_port = 788789;
      txApp2eventEng_setEvent.write(Event(TX, 23));
      rxEng2eventEng_setEvent.write(EventWithTuple(Event(RST, 0x8293479023), tuple));
      timer2eventEng_setEvent.write(Event(RT, 22));
    }
    if (!eventEng2txEng_event.empty()) {
      eventEng2txEng_event.read(ev);
      std::cout << ev.type << std::endl;
    }
    count++;
  }
  return 0;
}
