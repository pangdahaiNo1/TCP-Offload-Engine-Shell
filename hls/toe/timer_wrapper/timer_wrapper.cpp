#include "timer_wrapper.hpp"

void timer_wrapper(
    // close_timer
    stream<TcpSessionID> &rx_eng_to_timer_set_ctimer,
    // retrans_timer
    stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
    stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
    stream<AppNotification> &       rtimer_to_rx_app_notification,
    stream<OpenSessionStatus> &     rtimer_to_tx_app_notification,
    // probe_timer
    stream<TcpSessionID> &rx_eng_to_timer_clear_ptimer,
    stream<TcpSessionID> &tx_eng_to_timer_set_ptimer,
    // timer to event engine
    stream<Event> &timer_to_event_eng_set_event,
    // timer to state table
    stream<TcpSessionID> &timer_to_sttable_release_state

) {
#pragma HLS        DATAFLOW
#pragma HLS INLINE off

  static stream<TcpSessionID> ctimer_to_sttable_release_state("ctimer_to_sttable_release_state");
#pragma HLS STREAM variable = ctimer_to_sttable_release_state depth = 4

  static stream<Event> rtimer_to_event_eng_set_event("rtimer_to_event_eng_set_event");
#pragma HLS STREAM variable = rtimer_to_event_eng_set_event depth    = 4
#pragma HLS DATA_PACK                                       variable = rtimer_to_event_eng_set_event

  static stream<TcpSessionID> rtimer_to_sttable_release_state("rtimer_to_sttable_release_state");
#pragma HLS STREAM variable = rtimer_to_sttable_release_state depth = 4

  static stream<Event> ptimer_to_event_eng_set_event("ptimer_to_event_eng_set_event");
#pragma HLS STREAM variable = ptimer_to_event_eng_set_event depth    = 4
#pragma HLS DATA_PACK                                       variable = ptimer_to_event_eng_set_event

  close_timer(rx_eng_to_timer_set_ctimer, ctimer_to_sttable_release_state);

  retransmit_timer(rx_eng_to_timer_clear_rtimer,
                   tx_eng_to_timer_set_rtimer,
                   rtimer_to_event_eng_set_event,
                   rtimer_to_sttable_release_state,
                   rtimer_to_rx_app_notification,
                   rtimer_to_tx_app_notification);
  probe_timer(
      rx_eng_to_timer_clear_ptimer, tx_eng_to_timer_set_ptimer, ptimer_to_event_eng_set_event);

  AxiStreamMerger(
      rtimer_to_event_eng_set_event, ptimer_to_event_eng_set_event, timer_to_event_eng_set_event);

  AxiStreamMerger(ctimer_to_sttable_release_state,
                  rtimer_to_sttable_release_state,
                  timer_to_sttable_release_state);
}