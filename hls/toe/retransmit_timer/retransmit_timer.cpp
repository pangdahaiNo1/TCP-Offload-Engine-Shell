#include "retransmit_timer.hpp"
using namespace hls;
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/** @ingroup retransmit_timer
 *  The @ref tx_engine sends the Session-ID and Eventy type through the @param
 * tx_eng_to_timer_set_rtimer . If the timer is unactivated for this session it is activated, the
 * time-out interval is set depending on how often the session already time-outed. The @ref
 *rx_engine indicates when a timer for a specific session has to be stopped. If a timer times-out
 *the corresponding Event is fired back to the @ref tx_engine. If a session times-out more than 4
 *times in a row, it is aborted. The session is released through @param
 *rtimer_to_state_table_release_state and the application is notified through @param
 *rtimer_to_rx_app_notification. Currently the II is 2 which means that the interval take twice as
 *much time as defined.
 *  @param[in]		rx_eng_to_timer_clear_rtimer
 *  @param[in]		tx_eng_to_timer_set_rtimer
 *  @param[out]		rtimer_to_event_eng_set_event
 *  @param[out]		rtimer_to_state_table_release_state
 *  @param[out]		rtimer_to_rx_app_notification
 */
void                 retransmit_timer(stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
                                      stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
                                      stream<Event>                  &rtimer_to_event_eng_set_event,
                                      stream<TcpSessionID>           &rtimer_to_state_table_release_state,
                                      stream<AppNotificationNoTDEST> &rtimer_to_rx_app_notification,
                                      stream<OpenConnRspNoTDEST>     &rtimer_to_tx_app_notification) {
#pragma HLS PIPELINE II = 1

#pragma HLS aggregate variable = rx_eng_to_timer_clear_rtimer compact = bit
#pragma HLS aggregate variable = tx_eng_to_timer_set_rtimer compact = bit
#pragma HLS aggregate variable = rtimer_to_event_eng_set_event compact = bit
#pragma HLS aggregate variable = rtimer_to_state_table_release_state compact = bit
#pragma HLS aggregate variable = rtimer_to_rx_app_notification compact = bit
#pragma HLS aggregate variable = rtimer_to_tx_app_notification compact = bit

  static RetransmitTimerEntry retrans_timer_table[TCP_MAX_SESSIONS];
#pragma HLS bind_storage variable = retrans_timer_table type = RAM_T2P impl = BRAM
#pragma HLS aggregate variable = retrans_timer_table compact = bit
#pragma HLS DEPENDENCE variable                              = retrans_timer_table inter false

  static TcpSessionID           rtimer_cur_session_id  = 0;
  static bool                   rtimer_wait_for_write  = false;
  static TcpSessionID           rtimer_prev_session_id = 0;
  static RxEngToRetransTimerReq rx_eng_req;

  RetransmitTimerEntry rtimer_cur_entry;
  TcpSessionID         rtimer_cur_enrty_id;
  // 0 = default, 1 = tx engine
  ap_uint<1>             cur_op_is_tx_eng_or_default = 0;
  TxEngToRetransTimerReq tx_eng_req;

  if (rtimer_wait_for_write && rx_eng_req.session_id != rtimer_prev_session_id) {
    if (!rx_eng_req.stop) {
      retrans_timer_table[rx_eng_req.session_id].time = TIME_1s;
    } else {
      retrans_timer_table[rx_eng_req.session_id].time   = 0;
      retrans_timer_table[rx_eng_req.session_id].active = false;
    }
    retrans_timer_table[rx_eng_req.session_id].retries = 0;
    rtimer_wait_for_write                              = false;
  } else if (!rx_eng_to_timer_clear_rtimer.empty() && !rtimer_wait_for_write) {
    rx_eng_to_timer_clear_rtimer.read(rx_eng_req);
    logger.Info(RX_ENGINE, RTRMT_TMR, "Clr RTimer", rx_eng_req.to_string());
    rtimer_wait_for_write = true;
  } else {
    rtimer_cur_enrty_id = rtimer_cur_session_id;

    if (!tx_eng_to_timer_set_rtimer.empty()) {
      tx_eng_to_timer_set_rtimer.read(tx_eng_req);
      logger.Info(TX_ENGINE, RTRMT_TMR, "Set RTimer", tx_eng_req.to_string());
      rtimer_cur_enrty_id         = tx_eng_req.session_id;
      cur_op_is_tx_eng_or_default = 1;
      if (tx_eng_req.session_id - 3 < rtimer_cur_session_id &&
          rtimer_cur_session_id <= tx_eng_req.session_id) {
        rtimer_cur_session_id += 5;
      }
    } else {
      // increment position
      rtimer_cur_session_id++;
      if (rtimer_cur_session_id >= TCP_MAX_SESSIONS) {
        rtimer_cur_session_id = 0;
      }
      cur_op_is_tx_eng_or_default = 0;
    }

    // Get entry from table
    rtimer_cur_entry = retrans_timer_table[rtimer_cur_enrty_id];
    switch (cur_op_is_tx_eng_or_default) {
      case 1:
        rtimer_cur_entry.type = tx_eng_req.type;
        if (!rtimer_cur_entry.active) {
          switch (rtimer_cur_entry.retries) {
            case 0:
              rtimer_cur_entry.time = TIME_1s;
              break;
            case 1:
              rtimer_cur_entry.time = TIME_5s;
              break;
            case 2:
              rtimer_cur_entry.time = TIME_10s;
              break;
            case 3:
              rtimer_cur_entry.time = TIME_15s;
              break;
            default:
              rtimer_cur_entry.time = TIME_30s;
              break;
          }
        }
        rtimer_cur_entry.active = true;
        break;
      case 0:
        if (rtimer_cur_entry.active) {
          if (rtimer_cur_entry.time > 0) {
            rtimer_cur_entry.time--;
          }
          // We need to check if we can generate another event, otherwise we might end up in a
          // Deadlock, since the TX Engine will not be able to set new retransmit timers
          else if (!rtimer_to_event_eng_set_event.full()) {
            rtimer_cur_entry.time   = 0;
            rtimer_cur_entry.active = false;
            if (rtimer_cur_entry.retries < TCP_MAX_RETX_ATTEMPTS) {
              rtimer_cur_entry.retries++;
              Event rt_event(rtimer_cur_entry.type, rtimer_cur_enrty_id, rtimer_cur_entry.retries);
              logger.Info(RTRMT_TMR, EVENT_ENG, "Set RT Event", rt_event.to_string());
              rtimer_to_event_eng_set_event.write(rt_event);
            } else {
              rtimer_cur_entry.retries = 0;
              logger.Info(RTRMT_TMR,
                          STAT_TBLE,
                          "Release State by MAX RT",
                          rtimer_cur_enrty_id.to_string(16));
              rtimer_to_state_table_release_state.write(rtimer_cur_enrty_id);
              if (rtimer_cur_entry.type == SYN) {
                // Open Session Failed
                OpenConnRspNoTDEST open_conn_rsp(rtimer_cur_enrty_id, false);
                logger.Info(RTRMT_TMR,
                            TX_APP_IF,
                            "OpenSession Failed by MAX RT",
                            open_conn_rsp.to_string());
                rtimer_to_tx_app_notification.write(open_conn_rsp);
              } else {
                AppNotificationNoTDEST app_notify(rtimer_cur_enrty_id, true);
                logger.Info(RTRMT_TMR, RX_APP_IF, "CloseConn by MAX RT", app_notify.to_string());

                rtimer_to_rx_app_notification.write(app_notify);
              }
            }
          }
        }  // if active
        break;
    }  // switch
    // write entry back
    retrans_timer_table[rtimer_cur_enrty_id] = rtimer_cur_entry;
    rtimer_prev_session_id                   = rtimer_cur_enrty_id;
  }
}
