#include "state_table.hpp"
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"

#include <string>
using namespace hls;

void EmptyFifos(MockLogger &          logger,
                stream<SessionState> &sttable_to_rx_eng_rsp,
                stream<SessionState> &sttable_to_tx_app_lup_rsp,
                stream<SessionState> &sttable_to_tx_app_rsp,
                stream<TcpSessionID> &sttable_to_slookup_release_req) {
  SessionState cur_session_state;
  TcpSessionID to_slookup_req;

  while (!sttable_to_rx_eng_rsp.empty()) {
    sttable_to_rx_eng_rsp.read(cur_session_state);
    logger.Info(STATE_TABLE, RX_ENG, "Session State", state_to_string(cur_session_state));
  }
  while (!sttable_to_tx_app_lup_rsp.empty()) {
    sttable_to_tx_app_lup_rsp.read(cur_session_state);
    logger.Info(STATE_TABLE, TX_APP_INTF, "Lup Session State", state_to_string(cur_session_state));
  }
  while (!sttable_to_tx_app_rsp.empty()) {
    sttable_to_tx_app_rsp.read(cur_session_state);
    logger.Info(STATE_TABLE, TX_APP_INTF, "Session State", state_to_string(cur_session_state));
  }
  while (!sttable_to_slookup_release_req.empty()) {
    sttable_to_slookup_release_req.read(to_slookup_req);
    logger.Info(STATE_TABLE, SLUP_CTRL, "Release Session Req", to_slookup_req.to_string(16));
  }
}
MockLogger logger("./inner_state_table.log", STATE_TABLE);

int main() {
  stream<TcpSessionID>  timer_to_sttable_release_state;
  stream<StateTableReq> rx_eng_to_sttable_req;
  stream<SessionState>  sttable_to_rx_eng_rsp;
  stream<TcpSessionID>  tx_app_to_sttable_lup_req;
  stream<SessionState>  sttable_to_tx_app_lup_rsp;
  stream<StateTableReq> tx_app_to_sttable_req;
  stream<SessionState>  sttable_to_tx_app_rsp;
  stream<TcpSessionID>  sttable_to_slookup_release_req;

  MockLogger top_logger("./state_table.log", STATE_TABLE);
  int        sim_cycle = 0;

  TcpSessionID session_id;
  TcpSessionID session_id2;

  while (sim_cycle < 200) {
    switch (sim_cycle) {
      case 1:
        top_logger.Info("Test 1 rx(x, ESTA); timer(x), rx(x, FIN_WAIT)");
        session_id = 0x10;
        rx_eng_to_sttable_req.write(StateTableReq(session_id, ESTABLISHED, 1));
        break;
      case 2:
        timer_to_sttable_release_state.write(session_id);
        break;
      case 3:
        rx_eng_to_sttable_req.write(StateTableReq(session_id, FIN_WAIT_1, 1));
        break;
      case 10:
        top_logger.Info("Test 1 End");
        break;
      case 11:
        top_logger.Info("Test 2 rx(x, ESTA); rx(y, ESTA); timer(x), time(y)");
        session_id  = 0x20;
        session_id2 = 0x21;
        rx_eng_to_sttable_req.write(StateTableReq(session_id, ESTABLISHED, 1));
        break;
      case 12:
        rx_eng_to_sttable_req.write(StateTableReq(session_id2, ESTABLISHED, 1));
        break;
      case 13:
        timer_to_sttable_release_state.write(session_id2);
        break;
      case 14:
        timer_to_sttable_release_state.write(session_id);
        break;
      case 15:
        timer_to_sttable_release_state.write(session_id);
        break;
      case 20:
        top_logger.Info("Test 2 end");
        break;
      case 21:
        top_logger.Info("Test 3 rx(x); timer(x), rx(x, ESTABLISHED)");
        top_logger.Info("Test 3 rx(y), txApp(y), rx(y, ESTABLISHED), txApp(y, value)");
        session_id  = 0x30;
        session_id2 = 0x31;
        rx_eng_to_sttable_req.write(StateTableReq(session_id));
        break;
      case 22:
        timer_to_sttable_release_state.write(session_id);
        break;
      case 25:
        rx_eng_to_sttable_req.write(StateTableReq(session_id, ESTABLISHED, 1));
        break;
      case 28:
        rx_eng_to_sttable_req.write(StateTableReq(session_id));
        break;
      case 30:
        rx_eng_to_sttable_req.write(StateTableReq(session_id2));
        break;
      case 31:
        tx_app_to_sttable_req.write(StateTableReq(session_id2));
        break;
      case 34:
        rx_eng_to_sttable_req.write(StateTableReq(session_id2, CLOSING, 1));
        break;
      case 35:
        tx_app_to_sttable_req.write(StateTableReq(session_id2, SYN_SENT, 1));
        break;
      case 39:
        tx_app_to_sttable_lup_req.write(session_id2);
        break;
      case 50:
        top_logger.Info("Test 3 end");
        break;
      case 51:
        top_logger.Info("Test 4 txApp(x); rx(x), txApp(x, SYN_SENT), rx(x, ESTAB), txApp2(x)");
        session_id = 0x40;
        tx_app_to_sttable_req.write(StateTableReq(session_id));
        break;
      case 52:
        rx_eng_to_sttable_req.write(StateTableReq(session_id));
        break;
      case 55:
        tx_app_to_sttable_req.write(StateTableReq(session_id, SYN_SENT, 1));
        break;
      case 57:
        rx_eng_to_sttable_req.write(StateTableReq(session_id, ESTABLISHED, 1));
        break;
      case 69:
        tx_app_to_sttable_lup_req.write(session_id);
        break;
      case 70:
        top_logger.Info("Test 4 end");
        break;
      case 71:
        top_logger.Info("Test 5 rxEng(x, ESTABLISED), timer(x), rxEng(x)");
        session_id = 0x50;
        rx_eng_to_sttable_req.write(StateTableReq(session_id, ESTABLISHED, 1));
        break;
      case 75:
        timer_to_sttable_release_state.write(session_id);
        break;
      case 76:
        rx_eng_to_sttable_req.write(StateTableReq(session_id));
        break;
      case 80:
        top_logger.Info("Test 5 end");
        break;
      default:
        break;
    }
    state_table(timer_to_sttable_release_state,
                rx_eng_to_sttable_req,
                sttable_to_rx_eng_rsp,
                tx_app_to_sttable_lup_req,
                sttable_to_tx_app_lup_rsp,
                tx_app_to_sttable_req,
                sttable_to_tx_app_rsp,
                sttable_to_slookup_release_req);
    EmptyFifos(top_logger,
               sttable_to_rx_eng_rsp,
               sttable_to_tx_app_lup_rsp,
               sttable_to_tx_app_rsp,
               sttable_to_slookup_release_req);
    sim_cycle++;
    top_logger.SetSimCycle(sim_cycle);
    logger.SetSimCycle(sim_cycle);
  }

  return 0;
}
