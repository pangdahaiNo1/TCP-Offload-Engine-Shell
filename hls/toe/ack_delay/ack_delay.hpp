
#include "toe/toe_conn.hpp"

using namespace hls;
void ack_delay(stream<EventWithTuple> &event_eng_to_ack_delay_event,
               stream<EventWithTuple> &ack_delay_to_tx_eng_event,
               stream<ap_uint<1> > &   ack_delay_read_cnt_fifo,
               stream<ap_uint<1> > &   ack_delay_write_cnt_fifo);
