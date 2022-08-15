
#include "retransmit_timer.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"

using namespace hls;

void EmptyFifos(MockLogger                     &logger,
                stream<Event>                  &rtimer_to_event_eng_set_event,
                stream<TcpSessionID>           &rtimer_to_sttable_release_state,
                stream<AppNotificationNoTDEST> &rtimer_to_rx_app_notification,
                stream<OpenConnRspNoTDEST>     &rtimer_to_tx_app_notification) {
  Event                  out_event;
  TcpSessionID           out_id;
  AppNotificationNoTDEST out_rx_app_notify;
  OpenConnRspNoTDEST     out_tx_app_notify;

  while (!rtimer_to_event_eng_set_event.empty()) {
    rtimer_to_event_eng_set_event.read(out_event);
    logger.Info("Retranstimer to Event Engine Event", out_event.to_string(), false);
  }
  while (!rtimer_to_sttable_release_state.empty()) {
    rtimer_to_sttable_release_state.read(out_id);
    logger.Info("Retranstimer to state table release Session", out_id.to_string(16), false);
  }
  while (!rtimer_to_rx_app_notification.empty()) {
    rtimer_to_rx_app_notification.read(out_rx_app_notify);
    logger.Info("Retranstimer to Rx app notify NoTDEST", out_rx_app_notify.to_string(), false);
  }
  while (!rtimer_to_tx_app_notification.empty()) {
    rtimer_to_tx_app_notification.read(out_tx_app_notify);
    logger.Info("Retranstimer to Tx app notify NoTDEST", out_tx_app_notify.to_string(), false);
  }
}
MockLogger logger("./rtimer_inner.log");

int main() {
  stream<RxEngToRetransTimerReq> rx_eng_to_timer_clear_rtimer;
  stream<TxEngToRetransTimerReq> tx_eng_to_timer_set_rtimer;
  stream<Event>                  rtimer_to_event_eng_set_event;
  stream<TcpSessionID>           rtimer_to_sttable_release_state;
  stream<AppNotificationNoTDEST> rtimer_to_rx_app_notification;
  stream<OpenConnRspNoTDEST>     rtimer_to_tx_app_notification;

  RxEngToRetransTimerReq rx_eng_req;
  TxEngToRetransTimerReq tx_eng_req;

  // open output file
  MockLogger top_logger("rtimer_top.log");

  int sim_cycle = 0;
  while (sim_cycle < 10000) {
    switch (sim_cycle) {
      case 1:
        rx_eng_req.session_id = 0x1;
        rx_eng_req.stop       = false;
        rx_eng_to_timer_clear_rtimer.write(rx_eng_req);
        break;
      case 2:
        tx_eng_req.session_id = 0x2;
        tx_eng_req.type       = SYN;
        tx_eng_to_timer_set_rtimer.write(tx_eng_req);
        break;
      case 3:
        tx_eng_req.session_id = 0x3;
        tx_eng_req.type       = ACK;
        tx_eng_to_timer_set_rtimer.write(tx_eng_req);
        break;
      case 4:
        rx_eng_req.session_id = 0x2;
        rx_eng_req.stop       = true;
        rx_eng_to_timer_clear_rtimer.write(rx_eng_req);
        break;
      case 5:
        tx_eng_req.session_id = 0x2;
        tx_eng_req.type       = SYN;
        tx_eng_to_timer_set_rtimer.write(tx_eng_req);
        break;
      case 6:
        tx_eng_req.session_id = 0x3;
        tx_eng_req.type       = SYN;
        tx_eng_to_timer_set_rtimer.write(tx_eng_req);
        break;
      case 300:
        tx_eng_req.session_id = 0x3;
        tx_eng_req.type       = SYN;
        tx_eng_to_timer_set_rtimer.write(tx_eng_req);
        break;
      case 1500:
        tx_eng_req.session_id = 0x3;
        tx_eng_req.type       = SYN;
        tx_eng_to_timer_set_rtimer.write(tx_eng_req);
        break;
      case 3000:
        tx_eng_req.session_id = 0x3;
        tx_eng_req.type       = SYN;
        tx_eng_to_timer_set_rtimer.write(tx_eng_req);
        break;
      case 6000:
        tx_eng_req.session_id = 0x3;
        tx_eng_req.type       = SYN;
        tx_eng_to_timer_set_rtimer.write(tx_eng_req);
        break;
      default:
        break;
    }
    retransmit_timer(rx_eng_to_timer_clear_rtimer,
                     tx_eng_to_timer_set_rtimer,
                     rtimer_to_event_eng_set_event,
                     rtimer_to_sttable_release_state,
                     rtimer_to_rx_app_notification,
                     rtimer_to_tx_app_notification);
    EmptyFifos(top_logger,
               rtimer_to_event_eng_set_event,
               rtimer_to_sttable_release_state,
               rtimer_to_rx_app_notification,
               rtimer_to_tx_app_notification);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}