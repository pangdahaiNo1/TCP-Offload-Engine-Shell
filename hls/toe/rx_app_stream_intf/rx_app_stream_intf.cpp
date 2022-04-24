#include "rx_app_stream_intf.hpp"

/** @ingroup rx_app_stream_if
 *  This application interface is used to receive data streams of established connections.
 *  The Application polls data from the buffer by sending a readRequest. The module checks
 *  if the readRequest is valid then it sends a read request to the memory. After processing
 *  the request the MetaData containig the Session-ID is also written back.
 *  @param[in]		net_app_read_data_req
 *  @param[in]		rx_sar_to_rx_app_rsp
 *  @param[out]		net_app_read_data_rsp
 *  @param[out]		rx_app_to_rx_sar_req
 *  @param[out]		rx_buffer_read_cmd
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
) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static ap_uint<16> rasi_rx_app_read_length;
  static NetAXISDest rasi_rx_app_role_id;
  static ap_uint<1>  rasi_fsm_state = 0;

  switch (rasi_fsm_state) {
    case 0:
      if (!net_app_read_data_req.empty()) {
        NetAXISAppReadReq net_app_read_request = net_app_read_data_req.read();
        rasi_rx_app_role_id                    = net_app_read_request.dest;
        AppReadReq app_read_req                = net_app_read_request.data;
        // Make sure length is not 0, otherwise Data Mover will hang up
        if (app_read_req.read_length != 0) {
          // Get app pointer
          rx_app_to_rx_sar_req.write(RxSarAppReqRsp(app_read_req.session_id));
          rasi_rx_app_read_length = app_read_req.read_length;
          rasi_fsm_state          = 1;
        }
      }
      break;
    case 1:
      if (!rx_sar_to_rx_app_rsp.empty()) {
        RxSarAppReqRsp rx_app_rsp = rx_sar_to_rx_app_rsp.read();

        net_app_read_data_rsp.write(NetAXISAppReadRsp(rx_app_rsp.session_id, rasi_rx_app_role_id));
#if !(TCP_RX_DDR_BYPASS)
        ap_uint<32> pkgAddr         = 0;
        pkgAddr(29, WINDOW_BITS)    = rx_app_rsp.session_id(13, 0);
        pkgAddr(WINDOW_BITS - 1, 0) = rx_app_rsp.app_read;
        rx_buffer_read_cmd.write(DataMoverCmd(pkgAddr, rasi_rx_app_read_length));
#else
        rx_buffer_read_cmd.write(1);
#endif
        // Update app read pointer
        rx_app_to_rx_sar_req.write(
            RxSarAppReqRsp(rx_app_rsp.session_id, rx_app_rsp.app_read + rasi_rx_app_read_length));
        rasi_fsm_state = 0;
      }
      break;
  }
}
