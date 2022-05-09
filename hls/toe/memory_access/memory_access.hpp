#ifndef _MEMORY_ACCESS_HPP_
#define _MEMORY_ACCESS_HPP_

#include "toe/tcp_conn.hpp"
#include "toe/toe_intf.hpp"
#include "utils/axi_utils.hpp"

void TxAppWriteDataToMem(stream<NetAXIS> &     tx_app_to_mem_data_in,
                         stream<DataMoverCmd> &tx_app_to_mem_cmd_in,
                         stream<NetAXIS> &     mover_mem_data_out,
                         stream<DataMoverCmd> &mover_mem_cmd_out);
#endif