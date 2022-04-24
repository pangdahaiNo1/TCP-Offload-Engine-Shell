
#include "toe/tcp_conn.hpp"
#include "toe/toe_intf.hpp"

using hls::stream;
/** @defgroup rx_app_stream_if RX Application Stream Interface
 *  @ingroup app_if
 */
void rx_app_stream_intf(stream<NetAXISAppReadReq> &net_app_read_data_req,
                        stream<RxSarAppReqRsp> &   rx_sar_to_rx_app_rsp,
                        stream<NetAXISAppReadRsp> &net_app_read_data_rsp,
                        stream<RxSarAppReqRsp> &   rx_app_to_rx_sar_req,
#if !(TCP_RX_DDR_BYPASS)
                        stream<DataMoverCmd> &rx_buffer_read_cmd
#else
                        stream<ap_uint<1> > &rx_buffer_read_cmd
#endif
);