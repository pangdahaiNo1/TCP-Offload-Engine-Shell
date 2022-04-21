#ifndef _PORT_TABLE_HPP_
#define _PORT_TABLE_HPP_
#include "toe/tcp_conn.hpp"

using namespace hls;

/** @defgroup port_table Port Table
 *
 */
void port_table(stream<TcpPortNumber> &       rx_eng_to_port_table_req,
                stream<NetAXISListenPortReq> &rx_app_to_ptable_listen_port_req,
                stream<TcpPortNumber> &       slup_to_ptable_realease_port,
                stream<bool> &                ptable_to_rx_eng_check_rsp,
                stream<NetAXISListenPortRsp> &ptable_to_rx_app_listen_port_rsp,
                stream<TcpPortNumber> &       ptable_to_tx_app_port_rsp);

#endif