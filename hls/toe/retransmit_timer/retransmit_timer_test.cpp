
#include "retransmit_timer.hpp"
#include "toe/mock/mock_toe.hpp"

using namespace hls;

void EmptyFifos(std::ofstream &            out_stream,
                stream<Event> &            rtimer_to_event_eng_set_event,
                stream<TcpSessionID> &     rtimer_to_sttable_release_state,
                stream<AppNotification> &  rtimer_to_rx_app_notification,
                stream<OpenSessionStatus> &rtimer_to_tx_app_notification,
                int                        sim_cycle) {
  Event             out_event;
  TcpSessionID      out_id;
  AppNotification   out_rx_app_notify;
  OpenSessionStatus out_tx_app_notify;

  while (!rtimer_to_event_eng_set_event.empty()) {
    rtimer_to_event_eng_set_event.read(out_event);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Retranstimer to Event Engine Event\n";
    out_stream << out_event.to_string() << "\n";
  }
  while (!rtimer_to_sttable_release_state.empty()) {
    rtimer_to_sttable_release_state.read(out_id);
    out_stream << "Cycle " << std::dec << sim_cycle
               << ": Retranstimer to State table for release\n";
    out_stream << out_id.to_string() << "\n";
  }
  while (!rtimer_to_rx_app_notification.empty()) {
    rtimer_to_rx_app_notification.read(out_rx_app_notify);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Retranstimer to Rx app notify\n";
    out_stream << out_rx_app_notify.to_string() << "\n";
  }
  while (!rtimer_to_tx_app_notification.empty()) {
    rtimer_to_tx_app_notification.read(out_tx_app_notify);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Retranstimer to Tx App notify\n";
    out_stream << out_tx_app_notify.to_string() << "\n";
  }
}

int main() {
  stream<RxEngToRetransTimerReq> rx_eng_to_timer_clear_rtimer;
  stream<TxEngToRetransTimerReq> tx_eng_to_timer_set_rtimer;
  stream<Event>                  rtimer_to_event_eng_set_event;
  stream<TcpSessionID>           rtimer_to_sttable_release_state;
  stream<AppNotification>        rtimer_to_rx_app_notification;
  stream<OpenSessionStatus>      rtimer_to_tx_app_notification;

  RxEngToRetransTimerReq rx_eng_req;
  TxEngToRetransTimerReq tx_eng_req;

  // open output file
  std::ofstream outputFile;

  outputFile.open("./out.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }

  int sim_cycle = 0;
  while (sim_cycle < 2000000) {
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
      default:
        break;
    }
    retransmit_timer(rx_eng_to_timer_clear_rtimer,
                     tx_eng_to_timer_set_rtimer,
                     rtimer_to_event_eng_set_event,
                     rtimer_to_sttable_release_state,
                     rtimer_to_rx_app_notification,
                     rtimer_to_tx_app_notification);
    EmptyFifos(outputFile,
               rtimer_to_event_eng_set_event,
               rtimer_to_sttable_release_state,
               rtimer_to_rx_app_notification,
               rtimer_to_tx_app_notification,
               sim_cycle);
    sim_cycle++;
  }

  return 0;
}