#include "event_engine.hpp"
using namespace hls;
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/** @ingroup event_engine
 *  Arbitrates between the different event source FIFOs and forwards the event
 * to the \ref tx_engine
 *  @param[in]		tx_app_to_event_eng_set_event
 *  @param[in]		rx_eng_to_event_eng_set_event
 *  @param[in]		timer_to_event_eng_set_event
 *  @param[out]		event_eng_to_ack_delay_event
 */
void                 event_engine(stream<Event> &         tx_app_to_event_eng_set_event,
                                  stream<EventWithTuple> &rx_eng_to_event_eng_set_event,
                                  stream<Event> &         timer_to_event_eng_set_event,
                                  stream<EventWithTuple> &event_eng_to_ack_delay_event,
                                  stream<ap_uint<1> > &   ack_delay_read_count_fifo,
                                  stream<ap_uint<1> > &   ack_delay_write_count_fifo,
                                  stream<ap_uint<1> > &   tx_eng_read_count_fifo) {
#pragma HLS PIPELINE II = 1

#pragma HLS aggregate variable = tx_app_to_event_eng_set_event compact = bit
#pragma HLS aggregate variable = rx_eng_to_event_eng_set_event compact = bit
#pragma HLS aggregate variable = timer_to_event_eng_set_event compact = bit
#pragma HLS aggregate variable = event_eng_to_ack_delay_event compact = bit

  static ap_uint<8> eveng_write_event_count     = 0;
  static ap_uint<8> eveng_ack_delay_read_count  = 0;  // depends on FIFO depth
  static ap_uint<8> eveng_ack_delay_write_count = 0;  // depends on FIFO depth
  static ap_uint<8> eveng_tx_eng_read_count     = 0;  // depends on FIFO depth
  EventWithTuple    cur_event;

  if (!rx_eng_to_event_eng_set_event.empty()) {
    rx_eng_to_event_eng_set_event.read(cur_event);
    event_eng_to_ack_delay_event.write(cur_event);
    logger.Info(EVENT_ENG, ACK_DELAY, "Event from RxEng", cur_event.to_string());
    eveng_write_event_count++;
  } else if (eveng_write_event_count == eveng_ack_delay_read_count &&
             eveng_ack_delay_write_count == eveng_tx_eng_read_count) {
    if (!timer_to_event_eng_set_event.empty()) {
      timer_to_event_eng_set_event.read(cur_event);
      event_eng_to_ack_delay_event.write(cur_event);
      logger.Info(EVENT_ENG, ACK_DELAY, "Event from timer", cur_event.to_string());
      eveng_write_event_count++;
    } else if (!tx_app_to_event_eng_set_event.empty()) {
      tx_app_to_event_eng_set_event.read(cur_event);
      event_eng_to_ack_delay_event.write(cur_event);
      logger.Info(EVENT_ENG, ACK_DELAY, "Event from TxApp", cur_event.to_string());
      eveng_write_event_count++;
    }
  }

  if (!ack_delay_read_count_fifo.empty()) {
    ack_delay_read_count_fifo.read();
    eveng_ack_delay_read_count++;
  }
  if (!ack_delay_write_count_fifo.empty()) {
    ack_delay_write_count_fifo.read();
    eveng_ack_delay_write_count++;
  }
  if (!tx_eng_read_count_fifo.empty()) {
    tx_eng_read_count_fifo.read();
    eveng_tx_eng_read_count++;
  }
}
