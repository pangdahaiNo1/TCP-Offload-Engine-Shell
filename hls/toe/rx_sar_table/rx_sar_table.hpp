
#include "toe/toe_conn.hpp"

using namespace hls;

/** @defgroup rx_sar_table RX SAR Table
 *  @ingroup tcp_module
 *  @TODO rename why SAR
 */
void rx_sar_table(stream<RxSarAppReqRsp> & rx_app_to_rx_sar_req,
                  stream<RxSarAppReqRsp> & rx_sar_to_rx_app_rsp,
                  stream<RxEngToRxSarReq> &rx_eng_to_rx_sar_req,
                  stream<RxSarTableEntry> &rx_sar_to_rx_eng_rsp,
                  stream<TcpSessionID> &   tx_eng_to_rx_sar_lup_req,
                  stream<RxSarLookupRsp> & rx_sar_to_tx_eng_lup_rsp);