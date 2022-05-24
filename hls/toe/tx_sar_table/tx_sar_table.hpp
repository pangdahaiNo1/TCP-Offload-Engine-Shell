
#include "toe/toe_conn.hpp"

using namespace hls;

/** @defgroup tx_sar_table TX SAR Table
 *  @ingroup tcp_module
 *  @TODO rename, why SAR
 */
void tx_sar_table(stream<RxEngToTxSarReq> &rx_eng_to_tx_sar_req,
                  stream<TxSarToRxEngRsp> &tx_sar_to_rx_eng_rsp,
                  stream<TxEngToTxSarReq> &tx_eng_to_tx_sar_req,
                  stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
                  stream<TxAppToTxSarReq> &tx_app_to_tx_sar_req,
                  stream<TxSarToTxAppRsp> &tx_sar_to_tx_app_rsp);
