#ifndef _TIMER_WRAPPER_HPP_
#define _TIMER_WRAPPER_HPP_

#include "toe/close_timer/close_timer.hpp"
#include "toe/probe_timer/probe_timer.hpp"
#include "toe/retransmit_timer/retransmit_timer.hpp"
#include "utils/axi_utils.hpp"

void timer_wrapper(
    // close_timer
    stream<TcpSessionID> &rx_eng_to_timer_set_ctimer,
    // retrans_timer
    stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
    stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
    stream<AppNotificationNoTDEST> &rtimer_to_rx_app_notification,
    stream<OpenConnRspNoTDEST> &    rtimer_to_tx_app_notification,
    // probe_timer
    stream<TcpSessionID> &rx_eng_to_timer_clear_ptimer,
    stream<TcpSessionID> &tx_eng_to_timer_set_ptimer,
    // timer to event engine
    stream<Event> &timer_to_event_eng_set_event,
    // timer to state table
    stream<TcpSessionID> &timer_to_sttable_release_state

);
#endif