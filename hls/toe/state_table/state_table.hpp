#include "toe/tcp_conn.hpp"

using namespace hls;

/** @defgroup state_table State Table
 *  @ingroup tcp_module
 */
void state_table(stream<StateTableReq> &rx_eng_to_state_table_upd_req,
                 stream<StateTableReq> &tx_app_to_state_table_upd_req,
                 stream<TcpSessionID> & tx_app_to_state_table_lup_req,
                 stream<TcpSessionID> & timer_to_state_table_release_session_req,
                 stream<SessionState> & state_table_to_rx_eng_upd_rsp,
                 stream<SessionState> & state_table_to_tx_app_upd_rsp,
                 stream<SessionState> & state_table_to_tx_app_lup_rsp,
                 stream<TcpSessionID> & state_table_to_sessionlup_rsp);
