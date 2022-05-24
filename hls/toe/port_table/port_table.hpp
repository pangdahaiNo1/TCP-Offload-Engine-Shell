#ifndef _PORT_TABLE_HPP_
#define _PORT_TABLE_HPP_
#include "toe/toe_conn.hpp"

using namespace hls;

/** @defgroup port_table Port Table
 *
 */
void port_table(stream<TcpPortNumber> &   rx_eng_to_ptable_check_req,
                stream<ListenPortReq> &   rx_app_to_ptable_listen_port_req,
                stream<TcpPortNumber> &   slup_to_ptable_realease_port,
                stream<NetAXISDest> &     tx_app_to_ptable_port_req,
                stream<PtableToRxEngRsp> &ptable_to_rx_eng_check_rsp,
                stream<ListenPortRsp> &   ptable_to_rx_app_listen_port_rsp,
                stream<TcpPortNumber> &   ptable_to_tx_app_port_rsp);

#endif