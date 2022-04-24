
#include "toe/tcp_conn.hpp"

using namespace hls;

/** @defgroup tx_sar_table TX SAR Table
 *  @ingroup tcp_module
 *  @TODO rename, why SAR
 */
void tx_sar_table(stream<TxSarUpdateFromAckedSegReq> &rx_eng_to_tx_sar_upd_req,
                  stream<TxEngToTxSarReq> &           tx_eng_to_tx_sar_req,
                  stream<TxSarUpdateAppReq> &         tx_app_to_tx_sar_update_req,
                  stream<TxSarToRxEngRsp> &           tx_sar_to_rx_eng_lookup_rsp,
                  stream<TxSarToTxEngRsp> &           tx_sar_to_tx_eng_lookup_rsp,
                  stream<TxSarToAppRsp> &             tx_sar_to_app_rsp);
