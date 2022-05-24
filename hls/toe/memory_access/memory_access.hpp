#ifndef _MEMORY_ACCESS_HPP_
#define _MEMORY_ACCESS_HPP_

#include "toe/toe_conn.hpp"
#include "utils/axi_utils.hpp"

void TxEngReadDataSendCmd(stream<MemBufferRWCmd> &            tx_eng_to_mem_cmd_in,
                          stream<DataMoverCmd> &              mover_read_mem_cmd_out,
                          stream<MemBufferRWCmdDoubleAccess> &mem_buffer_double_access);

void TxEngReadDataFromMem(stream<NetAXIS> &                   mover_read_mem_data_in,
                          stream<MemBufferRWCmdDoubleAccess> &mem_buffer_double_access,
#if (TCP_NODELAY)
                          stream<bool> &   tx_eng_is_ddr_bypass,
                          stream<NetAXIS> &tx_eng_read_data_in,
#endif
                          stream<NetAXISWord> &to_tx_eng_read_data);

void TxAppWriteDataToMem(stream<NetAXISWord> & tx_app_to_mem_data_in,
                         stream<DataMoverCmd> &tx_app_to_mem_cmd_in,
                         stream<NetAXIS> &     mover_write_mem_data_out,
                         stream<DataMoverCmd> &mover_write_mem_cmd_out);
#endif