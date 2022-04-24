#include "ack_delay.hpp"

using namespace hls;

void                 ack_delay(stream<EventWithTuple> &any_to_ack_delay_event,
                               stream<EventWithTuple> &ack_delay_to_any_event,
                               stream<ap_uint<1> > &   ack_delay_read_cnt_fifo,
                               stream<ap_uint<1> > &   ack_delay_write_cnt_fifo) {
#pragma HLS PIPELINE II = 1

  static ap_uint<12> ack_table[TCP_MAX_SESSIONS];  // TODO why is it 12
#pragma HLS RESOURCE variable = ack_table core = RAM_2P_BRAM
#pragma HLS DEPENDENCE variable                = ack_table inter false
  static TcpSessionID ack_delay_cur_session_id = 0;
  // static ap_uint<4>	ad_readCounter = 0;
  EventWithTuple event;

  if (!any_to_ack_delay_event.empty()) {
    any_to_ack_delay_event.read(event);
    ack_delay_read_cnt_fifo.write(1);
    // Check if there is a delayed ACK
    if (event.type == ACK && ack_table[event.session_id] == 0) {
      ack_table[event.session_id] = TIME_64us;
    } else {
      // Assumption no SYN/RST
      ack_table[event.session_id] = 0;
      ack_delay_to_any_event.write(event);
      ack_delay_write_cnt_fifo.write(1);
    }
  } else {
    if (ack_table[ack_delay_cur_session_id] > 0 && !ack_delay_to_any_event.full()) {
      if (ack_table[ack_delay_cur_session_id] == 1) {
        ack_delay_to_any_event.write(Event(ACK, ack_delay_cur_session_id));
        ack_delay_write_cnt_fifo.write(1);
      }
      // Decrease value
      ack_table[ack_delay_cur_session_id] -= 1;
    }
    ack_delay_cur_session_id++;
    if (ack_delay_cur_session_id == TCP_MAX_SESSIONS) {
      ack_delay_cur_session_id = 0;
    }
  }
}
