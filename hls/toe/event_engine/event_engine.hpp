#include "toe/toe_conn.hpp"

using namespace hls;

/** @defgroup event_engine Event Engine
 *  @ingroup tcp_module
 */
void event_engine(stream<Event>          &tx_app_to_event_eng_set_event,
                  stream<EventWithTuple> &rx_eng_to_event_eng_set_event,
                  stream<Event>          &timer_to_event_eng_set_event,
                  stream<EventWithTuple> &event_eng_to_ack_delay_event,
                  stream<ap_uint<1> >    &ack_delay_read_count_fifo,
                  stream<ap_uint<1> >    &ack_delay_write_count_fifo,
                  stream<ap_uint<1> >    &tx_eng_read_count_fifo);
