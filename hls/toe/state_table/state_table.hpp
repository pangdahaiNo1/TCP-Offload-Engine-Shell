#include "toe/toe_conn.hpp"

using namespace hls;

/** @defgroup state_table State Table
 *  @ingroup tcp_module
 */
void state_table(stream<StateTableReq> &rx_eng_to_sttable_req,
                 stream<StateTableReq> &tx_app_to_sttable_req,
                 stream<TcpSessionID> & tx_app_to_sttable_lup_req,
                 stream<TcpSessionID> & timer_to_sttable_release_state,
                 stream<SessionState> & sttable_to_rx_eng_rsp,
                 stream<SessionState> & sttable_to_tx_app_rsp,
                 stream<SessionState> & sttable_to_tx_app_lup_rsp,
                 stream<TcpSessionID> & sttable_to_slookup_release_req);
