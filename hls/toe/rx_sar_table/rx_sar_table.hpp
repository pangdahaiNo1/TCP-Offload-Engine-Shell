
#include "toe/tcp_conn.hpp"

using namespace hls;

/** @defgroup rx_sar_table RX SAR Table
 *  @ingroup tcp_module
 *  @TODO rename why SAR
 */
void rx_sar_table(stream<RxSarSegReq> &    rx_eng_to_rx_sar_upd_req,
                  stream<RxSarAppReqRsp> & rx_app_to_rx_sar_upd_req,
                  stream<TcpSessionID> &   tx_eng_to_rx_sar_lookup_req,
                  stream<RxSarTableEntry> &rx_sar_to_rx_eng_upd_rsp,
                  stream<RxSarAppReqRsp> & rx_sar_to_rx_app_upd_rsp,
                  stream<RxSarLookupRsp> & rx_sar_to_tx_eng_lookup_rsp);