#include "rx_engine.hpp"
#include "toe/memory_access/memory_access.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"
#include "utils/axi_utils.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

MockLogger logger("./rx_eng_inner.log", RX_ENGINE);

void EmptyTcpPayloadFifos(MockLogger &logger, stream<NetAXISWord> &tcp_payload_fifo) {
  NetAXISWord tcp_payload;
  while (!tcp_payload_fifo.empty()) {
    tcp_payload_fifo.read(tcp_payload);
    logger.Info(RX_ENGINE, NET_APP, "Recv Payload", tcp_payload.to_string(), false);
  }
}

void EmptyTcpPayloadWithMemFifos(MockLogger &          logger,
                                 stream<DataMoverCmd> &rx_eng_to_mem_write_cmd,
                                 stream<NetAXIS> &     rx_eng_to_mem_write_data) {
  NetAXISWord  to_mem_data;
  DataMoverCmd to_mem_cmd;

  while (!rx_eng_to_mem_write_cmd.empty()) {
    to_mem_cmd = rx_eng_to_mem_write_cmd.read();
    logger.Info(RX_ENGINE, DATA_MVER, "Write CMD", to_mem_cmd.to_string(), false);
  }
  while (!rx_eng_to_mem_write_data.empty()) {
    to_mem_data = rx_eng_to_mem_write_data.read();
    logger.Info(RX_ENGINE, DATA_MVER, "Write Data", to_mem_data.to_string(), false);
  }
}

void EmptyTcpRxPseudoHeaderFifos(MockLogger &                 logger,
                                 stream<TcpPseudoHeaderMeta> &tcp_pseudo_header_meta_parsed,
                                 stream<ap_uint<16> > &       tcp_pseudo_packet_checksum) {
  TcpPseudoHeaderMeta meta_header;

  ap_uint<16> tcp_checksum;
  while (!tcp_pseudo_header_meta_parsed.empty()) {
    tcp_pseudo_header_meta_parsed.read(meta_header);
    logger.Info(TOE_TOP, RX_ENGINE, "Recv Seg", meta_header.to_string(), true);
  }
  while (!tcp_pseudo_packet_checksum.empty()) {
    tcp_pseudo_packet_checksum.read(tcp_checksum);
    logger.Info(TOE_TOP, RX_ENGINE, "Recv Seg Checksum", tcp_checksum.to_string(16), false);
  }
}

void EmptyTcpRxEngCheckPortFifo(MockLogger &logger,
                                // port table check open and TDEST
                                stream<TcpPortNumber> &rx_eng_to_ptable_check_req) {
  TcpPortNumber port_number;
  while (!rx_eng_to_ptable_check_req.empty()) {
    rx_eng_to_ptable_check_req.read(port_number);
    logger.Info(RX_ENGINE, PORT_TBLE, "CheckPort Req", port_number.to_string(16));
  }
}

void EmptyTcpRxEngFSMFifo(
    MockLogger &logger,
    // to session lookup R/W
    stream<RxEngToSlookupReq> &rx_eng_to_slookup_req,
    // Rx SAR R/W
    stream<RxEngToRxSarReq> &rx_eng_to_rx_sar_req,
    // TX SAR R/W
    stream<RxEngToTxSarReq> &rx_eng_to_tx_sar_req,
    // state table
    stream<StateTableReq> &rx_eng_to_sttable_req,
    // update timer state
    stream<TcpSessionID> &          rx_eng_to_timer_set_ctimer,
    stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
    stream<TcpSessionID> &          rx_eng_to_timer_clear_ptimer,
    // to app connection notify, when net app active open a connection
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // to app connection notify, when net app passive open a conection
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    // to app data notify
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification,

    // to event engine
    stream<EventWithTuple> &rx_eng_to_event_eng_set_event) {
  RxEngToSlookupReq            to_slup_req;
  RxEngToRxSarReq              to_rx_sar_req;
  RxEngToTxSarReq              to_tx_sar_req;
  StateTableReq                to_sttable_req;
  TcpSessionID                 session_id;
  RxEngToRetransTimerReq       to_rtimer;
  OpenConnRspNoTDEST           to_tx_app_open_conn_notify;
  NewClientNotificationNoTDEST to_tx_app_new_client_notify;
  AppNotificationNoTDEST       to_rx_app_notify;
  EventWithTuple               to_event_eng_event;

  while (!rx_eng_to_slookup_req.empty()) {
    rx_eng_to_slookup_req.read(to_slup_req);
    logger.Info(RX_ENGINE, SLUP_CTRL, "Session Lup or Creaate", to_slup_req.to_string());
  }
  while (!rx_eng_to_rx_sar_req.empty()) {
    rx_eng_to_rx_sar_req.read(to_rx_sar_req);
    logger.Info(RX_ENGINE, RX_SAR_TB, "R/W Req", to_rx_sar_req.to_string());
  }
  while (!rx_eng_to_tx_sar_req.empty()) {
    rx_eng_to_tx_sar_req.read(to_tx_sar_req);
    logger.Info(RX_ENGINE, TX_SAR_TB, "R/W Req", to_tx_sar_req.to_string());
  }
  while (!rx_eng_to_sttable_req.empty()) {
    rx_eng_to_sttable_req.read(to_sttable_req);
    logger.Info(RX_ENGINE, STAT_TBLE, "R/W Req", to_slup_req.to_string());
  }
  while (!rx_eng_to_timer_set_ctimer.empty()) {
    rx_eng_to_timer_set_ctimer.read(session_id);
    logger.Info(RX_ENGINE, CLOSE_TMR, "Set CTimer", session_id.to_string(16));
  }
  while (!rx_eng_to_timer_clear_rtimer.empty()) {
    rx_eng_to_timer_clear_rtimer.read(to_rtimer);
    logger.Info(RX_ENGINE, RTRMT_TMR, "Clear RTimer?: ", to_rtimer.to_string());
  }
  while (!rx_eng_to_timer_clear_ptimer.empty()) {
    rx_eng_to_timer_clear_ptimer.read(session_id);
    logger.Info(RX_ENGINE, PROBE_TMR, "Clear PTimer", session_id.to_string(16));
  }
  while (!rx_eng_to_tx_app_notification.empty()) {
    rx_eng_to_tx_app_notification.read(to_tx_app_open_conn_notify);
    logger.Info(RX_ENGINE, TX_APP_IF, "OpenConn Notify", to_tx_app_open_conn_notify.to_string());
  }
  while (!rx_eng_to_tx_app_new_client_notification.empty()) {
    rx_eng_to_tx_app_new_client_notification.read(to_tx_app_new_client_notify);
    logger.Info(RX_ENGINE, TX_APP_IF, "NewClient Notify", to_tx_app_new_client_notify.to_string());
  }
  while (!rx_eng_to_rx_app_notification.empty()) {
    rx_eng_to_rx_app_notification.read(to_rx_app_notify);
    logger.Info(RX_ENGINE, RX_APP_IF, "App Notify", to_rx_app_notify.to_string());
  }
  while (!rx_eng_to_event_eng_set_event.empty()) {
    rx_eng_to_event_eng_set_event.read(to_event_eng_event);
    logger.Info(RX_ENGINE, EVENT_ENG, "Event", to_event_eng_event.to_string());
  }
}

void TestTcpPseudoHeaderParser(stream<NetAXIS> &input_tcp_packet) {
  MockLogger top_logger("rx_eng_pseduo_header.log", RX_ENGINE);

  // some fifos
  stream<NetAXISWord>  tcp_pseudo_packet_for_checksum_fifo("tcp_pseudo_packet_for_checksum_fifo");
  stream<NetAXISWord>  tcp_pseudo_packet_for_rx_eng_fifo("tcp_pseudo_packet_for_rx_eng_fifo");
  stream<SubChecksum>  tcp_pseudo_packet_subchecksum_fifo("tcp_pseudo_packet_subchecksum_fifo");
  stream<ap_uint<16> > tcp_pseudo_packet_checksum_fifo("tcp_pseudo_packet_checksum_fifo");
  stream<TcpPseudoHeaderMeta> tcp_pseudo_header_meta_fifo("tcp_pseudo_header_meta_fifo");
  stream<TcpPseudoHeaderMeta> tcp_pseudo_header_meta_parsed_fifo(
      "tcp_pseudo_header_meta_parsed_fifo");
  stream<NetAXISWord> tcp_payload_fifo("tcp_payload_fifo");
  stream<NetAXIS>     tcp_payload_fifo_for_save("tcp_payload_fifo2");
  stream<NetAXISWord> tcp_payload_fifo2("tcp_payload_fifo2");

  // simulation
  int sim_cycle = 0;
  while (sim_cycle < 200) {
    RxEngTcpPseudoHeaderInsert(
        input_tcp_packet, tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_for_rx_eng_fifo);
    ComputeSubChecksum<1>(tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_subchecksum_fifo);
    CheckChecksum(tcp_pseudo_packet_subchecksum_fifo, tcp_pseudo_packet_checksum_fifo);
    RxEngParseTcpHeader(
        tcp_pseudo_packet_for_rx_eng_fifo, tcp_pseudo_header_meta_fifo, tcp_payload_fifo);
    RxEngParseTcpHeaderOptions(tcp_pseudo_header_meta_fifo, tcp_pseudo_header_meta_parsed_fifo);
    if (!tcp_payload_fifo.empty()) {
      NetAXISWord cur_word = tcp_payload_fifo.read();
      tcp_payload_fifo_for_save.write(cur_word.to_net_axis());
      tcp_payload_fifo2.write(cur_word);
    }
    EmptyTcpRxPseudoHeaderFifos(
        top_logger, tcp_pseudo_header_meta_parsed_fifo, tcp_pseudo_packet_checksum_fifo);
    EmptyTcpPayloadFifos(top_logger, tcp_payload_fifo2);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
  }
  SaveNetAXISToFile(tcp_payload_fifo_for_save, "tcp_payload.dat");
}

// with or without RX_DDR_BYPASS
void TestRxEngine(stream<NetAXIS> &input_tcp_packet, int input_tcp_packet_cnt) {
  // rx engine fifos
  // port table check open and TDEST
  stream<TcpPortNumber>    rx_eng_to_ptable_check_req("rx_eng_to_ptable_check_req");
  stream<PtableToRxEngRsp> ptable_to_rx_eng_check_rsp("ptable_to_rx_eng_check_rsp");
  // to session lookup R/W
  stream<RxEngToSlookupReq> rx_eng_to_slookup_req("rx_eng_to_slookup_req");
  stream<SessionLookupRsp>  slookup_to_rx_eng_rsp("slookup_to_rx_eng_rsp");
  // FSM
  // Rx SAR R/W
  stream<RxEngToRxSarReq> rx_eng_to_rx_sar_req("rx_eng_to_rx_sar_req");
  stream<RxSarTableEntry> rx_sar_to_rx_eng_rsp("rx_sar_to_rx_eng_rsp");
  // TX SAR R/W
  stream<RxEngToTxSarReq> rx_eng_to_tx_sar_req("rx_eng_to_tx_sar_req");
  stream<TxSarToRxEngRsp> tx_sar_to_rx_eng_rsp("tx_sar_to_rx_eng_rsp");
  // state table
  stream<StateTableReq> rx_eng_to_sttable_req("rx_eng_to_sttable_req");
  stream<SessionState>  sttable_to_rx_eng_rsp("sttable_to_rx_eng_rsp");
  // update timer state
  stream<TcpSessionID>           rx_eng_to_timer_set_ctimer("rx_eng_to_timer_set_ctimer");
  stream<RxEngToRetransTimerReq> rx_eng_to_timer_clear_rtimer("rx_eng_to_timer_clear_rtimer");
  stream<TcpSessionID>           rx_eng_to_timer_clear_ptimer("rx_eng_to_timer_clear_ptimer");
  // to app connection notify, when net app active open a connection
  stream<OpenConnRspNoTDEST> rx_eng_to_tx_app_notification("rx_eng_to_tx_app_notification");
  // to app connection notify, when net app passive open a conection
  stream<NewClientNotificationNoTDEST> rx_eng_to_tx_app_new_client_notification(
      "rx_eng_to_tx_app_new_client_notification");
  // to app data notify
  stream<AppNotificationNoTDEST> rx_eng_to_rx_app_notification("rx_eng_to_rx_app_notification");

  // to event engine
  stream<EventWithTuple> rx_eng_to_event_eng_set_event("rx_eng_to_event_eng_set_event");
  // tcp payload to rx app
  stream<NetAXISWord> rx_eng_to_rx_app_data("rx_eng_to_rx_app_data");
  // tcp payload to mem
  stream<DataMoverCmd>    rx_eng_to_mem_write_cmd;
  stream<NetAXIS>         rx_eng_to_mem_write_data;
  stream<DataMoverStatus> mem_to_rx_eng_write_status;

  MockLogger       top_logger("rx_eng.log", RX_ENGINE);
  int              sim_cycle       = 0;
  int              total_sim_cycle = 2000;
  PtableToRxEngRsp ptable_rsp;
  SessionLookupRsp slup_rsp;
  RxSarTableEntry  rx_sar_rsp;
  TxSarToRxEngRsp  tx_sar_rsp;
  DataMoverStatus  datamover_sts;
  cout << "Test Rx eng" << endl;
  while (sim_cycle < total_sim_cycle) {
    switch (sim_cycle) {
      case 1:
        // 1st packet - SYN
        ptable_rsp.is_open = true;
        ptable_rsp.role_id = 0x1;
        ptable_to_rx_eng_check_rsp.write(ptable_rsp);
        break;
      case 2:
        // 1st packet
        slup_rsp.hit        = true;
        slup_rsp.session_id = 0x1;
        slup_rsp.role_id    = INVALID_TDEST;
        slookup_to_rx_eng_rsp.write(slup_rsp);
        break;
      case 3:
        // 1st packet
        sttable_to_rx_eng_rsp.write(CLOSED);
        rx_sar_rsp.app_read  = 0x0;
        rx_sar_rsp.recvd     = 0;
        rx_sar_rsp.win_shift = 0;
        rx_sar_to_rx_eng_rsp.write(rx_sar_rsp);
        break;
      case 4:
        // 2nd packet - ACK, no data
        ptable_rsp.is_open = true;
        ptable_rsp.role_id = 0x1;
        ptable_to_rx_eng_check_rsp.write(ptable_rsp);
        break;
      case 5:
        // 2nd packet
        slup_rsp.hit        = true;
        slup_rsp.session_id = 0x1;
        slup_rsp.role_id    = INVALID_TDEST;
        slookup_to_rx_eng_rsp.write(slup_rsp);
        break;
      case 6:
        // 2nd packet
        // it will trigger a RST event
        // sttable_to_rx_eng_rsp.write(CLOSED);
        sttable_to_rx_eng_rsp.write(SYN_RECEIVED);
        // the 1st packet sequence + 1
        rx_sar_rsp.app_read  = 0xa135f3ce;
        rx_sar_rsp.recvd     = 0xa135f3ce;
        rx_sar_rsp.win_shift = 2;
        rx_sar_to_rx_eng_rsp.write(rx_sar_rsp);
        // 2nd packet
        tx_sar_rsp.perv_ack  = 0x0;
        tx_sar_rsp.next_byte = 0xff73ce99;  // ! import
        tx_sar_rsp.win_shift = 2;
        tx_sar_to_rx_eng_rsp.write(tx_sar_rsp);
        break;
      case 7:
        // 3rd packet ACK + data
        ptable_rsp.is_open = true;
        ptable_rsp.role_id = 0x1;
        ptable_to_rx_eng_check_rsp.write(ptable_rsp);
        break;
      case 8:
        // 3nd packet
        slup_rsp.hit        = true;
        slup_rsp.session_id = 0x1;
        slup_rsp.role_id    = INVALID_TDEST;
        slookup_to_rx_eng_rsp.write(slup_rsp);
        break;
      case 9:
        // 3rd packet
        sttable_to_rx_eng_rsp.write(ESTABLISHED);
        // the 1st packet sequence + 1
        rx_sar_rsp.app_read  = 0xa135f3ce;
        rx_sar_rsp.recvd     = 0xa135f3ce;
        rx_sar_rsp.win_shift = 2;
        rx_sar_to_rx_eng_rsp.write(rx_sar_rsp);
        // 3rd packet
        tx_sar_rsp.perv_ack  = 0x0;
        tx_sar_rsp.next_byte = 0xff73ce99;  // ! import
        tx_sar_rsp.win_shift = 2;
        tx_sar_to_rx_eng_rsp.write(tx_sar_rsp);
        break;
      case 10:
        // 4th packet - ACK  + no data
        ptable_rsp.is_open = true;
        ptable_rsp.role_id = 0x1;
        ptable_to_rx_eng_check_rsp.write(ptable_rsp);
        // 4th packet
        slup_rsp.hit        = true;
        slup_rsp.session_id = 0x1;
        slup_rsp.role_id    = INVALID_TDEST;
        slookup_to_rx_eng_rsp.write(slup_rsp);
        break;
      case 11:
        // 4th packet -
        sttable_to_rx_eng_rsp.write(ESTABLISHED);
        // the 1st packet sequence + 1
        rx_sar_rsp.app_read  = 0xa135f3ce + 0x2b4;
        rx_sar_rsp.recvd     = 0xa135f3ce + 0x2b4;
        rx_sar_rsp.win_shift = 2;
        rx_sar_to_rx_eng_rsp.write(rx_sar_rsp);
        // 3rd packet
        tx_sar_rsp.perv_ack  = 0x0;
        tx_sar_rsp.next_byte = 0xff73ce99 + 0x2b4;  // ! import
        tx_sar_rsp.win_shift = 2;
        tx_sar_to_rx_eng_rsp.write(tx_sar_rsp);
        break;
      case 30:
#if !TCP_RX_DDR_BYPASS
        datamover_sts.okay = 1;
        mem_to_rx_eng_write_status.write(datamover_sts);
#endif
        // 5th packet - FIN + ACK no data
        ptable_rsp.is_open = true;
        ptable_rsp.role_id = 0x1;
        ptable_to_rx_eng_check_rsp.write(ptable_rsp);
        // 5th packet
        slup_rsp.hit        = true;
        slup_rsp.session_id = 0x1;
        slup_rsp.role_id    = INVALID_TDEST;
        slookup_to_rx_eng_rsp.write(slup_rsp);
        break;
      case 31:
        // 5th packet - FIN + ACK
        sttable_to_rx_eng_rsp.write(ESTABLISHED);
        // the 1st packet sequence + data length
        rx_sar_rsp.app_read  = 0xa135f3ce + 0x2b4;
        rx_sar_rsp.recvd     = 0xa135f3ce + 0x2b4;
        rx_sar_rsp.win_shift = 2;
        rx_sar_to_rx_eng_rsp.write(rx_sar_rsp);
        // 5th packet
        tx_sar_rsp.perv_ack  = 0x0;
        tx_sar_rsp.next_byte = 0xff73ce99 + 0x2b4;  // ! import
        tx_sar_rsp.win_shift = 2;
        tx_sar_to_rx_eng_rsp.write(tx_sar_rsp);
        break;
      case 40:
        // 6th packet - FIN + ACK no data
        ptable_rsp.is_open = true;
        ptable_rsp.role_id = 0x1;
        ptable_to_rx_eng_check_rsp.write(ptable_rsp);
        // 5th packet
        slup_rsp.hit        = true;
        slup_rsp.session_id = 0x1;
        slup_rsp.role_id    = INVALID_TDEST;
        slookup_to_rx_eng_rsp.write(slup_rsp);
        break;
      case 41:
        // 6th packet - ACK
        // the session will be close
        sttable_to_rx_eng_rsp.write(LAST_ACK);
        // 6th packet - the 1st packet sequence + data length + FIN
        rx_sar_rsp.app_read  = 0xa135f3ce + 0x2b4;
        rx_sar_rsp.recvd     = 0xa135f3ce + 0x2b4 + 1;
        rx_sar_rsp.win_shift = 2;
        rx_sar_to_rx_eng_rsp.write(rx_sar_rsp);
        // 6th packet
        tx_sar_rsp.perv_ack  = 0x0;
        tx_sar_rsp.next_byte = 0xff73ce99 + 0x2b4 + 1;  // ! import
        tx_sar_rsp.win_shift = 2;
        tx_sar_to_rx_eng_rsp.write(tx_sar_rsp);
        break;
      default:
        break;
    }
    rx_engine(input_tcp_packet,
              rx_eng_to_ptable_check_req,
              ptable_to_rx_eng_check_rsp,
              rx_eng_to_slookup_req,
              slookup_to_rx_eng_rsp,
              rx_eng_to_rx_sar_req,
              rx_sar_to_rx_eng_rsp,
              rx_eng_to_tx_sar_req,
              tx_sar_to_rx_eng_rsp,
              rx_eng_to_sttable_req,
              sttable_to_rx_eng_rsp,
              rx_eng_to_timer_set_ctimer,
              rx_eng_to_timer_clear_rtimer,
              rx_eng_to_timer_clear_ptimer,
              rx_eng_to_tx_app_notification,
              rx_eng_to_tx_app_new_client_notification,
              rx_eng_to_rx_app_notification,
              rx_eng_to_event_eng_set_event,
#if !TCP_RX_DDR_BYPASS
              // tcp payload to mem
              rx_eng_to_mem_write_cmd,
              rx_eng_to_mem_write_data,
              mem_to_rx_eng_write_status
#else
              // tcp payload to rx app
              rx_eng_to_rx_app_data
#endif
    );
    EmptyTcpRxEngCheckPortFifo(top_logger, rx_eng_to_ptable_check_req);
    EmptyTcpRxEngFSMFifo(top_logger,
                         rx_eng_to_slookup_req,
                         rx_eng_to_rx_sar_req,
                         rx_eng_to_tx_sar_req,
                         rx_eng_to_sttable_req,
                         rx_eng_to_timer_set_ctimer,
                         rx_eng_to_timer_clear_rtimer,
                         rx_eng_to_timer_clear_ptimer,
                         rx_eng_to_tx_app_notification,
                         rx_eng_to_tx_app_new_client_notification,
                         rx_eng_to_rx_app_notification,
                         rx_eng_to_event_eng_set_event);
#if !TCP_RX_DDR_BYPASS
    EmptyTcpPayloadWithMemFifos(top_logger, rx_eng_to_mem_write_cmd, rx_eng_to_mem_write_data);
#else
    EmptyTcpPayloadFifos(top_logger, rx_eng_to_rx_app_data);
#endif
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *golden_input_file_tcp = argv[1];
  // stream<NetAXIS> input_golden_tcp_packets("input_golden_tcp_packets");
  stream<NetAXIS> input_golden_tcp_packets2("input_golden_tcp_packets2");

  // int packet_cnt  = PcapToStream(golden_input_file_tcp, true, input_golden_tcp_packets);
  int packet2_cnt = PcapToStream(golden_input_file_tcp, true, input_golden_tcp_packets2);

  // TestTcpPseudoHeaderParser(input_golden_tcp_packets);
  // NetAXIStreamWordToNetAXIStream(tcp_pseudo_packet, tcp_pseudo_packet_for_save);
  // SaveNetAXISToFile(tcp_pseudo_packet_for_save, "tcp_pseudo_packet.dat");
  TestRxEngine(input_golden_tcp_packets2, packet2_cnt);
  return 0;
}
