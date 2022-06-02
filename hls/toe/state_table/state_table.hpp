#include "toe/toe_conn.hpp"

using namespace hls;

/** @defgroup state_table State Table
 *  @ingroup tcp_module
 */
void state_table(
    // from other module req
    stream<TcpSessionID> &timer_to_sttable_release_state,
    // rx engine R/W req/rsp
    stream<StateTableReq> &rx_eng_to_sttable_req,
    stream<SessionState> & sttable_to_rx_eng_rsp,
    // tx app read only req/rsp
    stream<TcpSessionID> &tx_app_to_sttable_lup_req,
    stream<SessionState> &sttable_to_tx_app_lup_rsp,
    // tx app R/W req/rsp
    stream<StateTableReq> &tx_app_to_sttable_req,
    stream<SessionState> & sttable_to_tx_app_rsp,
    // to other module req
    stream<TcpSessionID> &sttable_to_slookup_release_req);
