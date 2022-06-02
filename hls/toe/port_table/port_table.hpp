#ifndef _PORT_TABLE_HPP_
#define _PORT_TABLE_HPP_
#include "toe/toe_conn.hpp"

using namespace hls;

/** @defgroup port_table Port Table
 *
 */
void port_table(
    // from other module req
    stream<TcpPortNumber> &slookup_to_ptable_release_port_req,
    // rx app listen port req/rsp
    stream<ListenPortReq> &rx_app_to_ptable_listen_port_req,
    stream<ListenPortRsp> &ptable_to_rx_app_listen_port_rsp,
    // rx eng check req/rsp
    stream<TcpPortNumber> &   rx_eng_to_ptable_check_req,
    stream<PtableToRxEngRsp> &ptable_to_rx_eng_check_rsp,
    // tx app get avail port req/rsp
    stream<NetAXISDest> &  tx_app_to_ptable_req,
    stream<TcpPortNumber> &ptable_to_tx_app_rsp);

#endif