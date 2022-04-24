#include "rx_app_intf.hpp"

using namespace hls;

/** @ingroup rx_app_intf
 *  This application interface is used to open passive connections
 *  @param[in]		appListeningIn
 *  @param[in]		appStopListeningIn
 *  @param[in]		rxAppPorTableListenIn
 *  @param[in]		rxAppPortTableCloseIn
 *  @param[out]		appListeningOut
 *  @param[out]		rxAppPorTableListenOut
 */
// lock step for the multi role listening request
void                 rx_app_intf(stream<NetAXISListenPortReq> &rx_app_to_rx_intf_listen_port_req,
                                 stream<NetAXISListenPortRsp> &rx_intf_to_rx_app_listen_port_rsp,
                                 stream<NetAXISListenPortReq> &rx_intf_to_ptable_listen_port_req,
                                 stream<NetAXISListenPortRsp> &ptable_to_rx_intf_listen_port_rsp) {
#pragma HLS PIPELINE II = 1

  static bool listen_port_lock = false;

  if (!rx_app_to_rx_intf_listen_port_req.empty() && !listen_port_lock) {
    rx_intf_to_ptable_listen_port_req.write(rx_app_to_rx_intf_listen_port_req.read());
    listen_port_lock = true;
  } else if (!ptable_to_rx_intf_listen_port_rsp.empty() && listen_port_lock) {
    rx_intf_to_rx_app_listen_port_rsp.write(ptable_to_rx_intf_listen_port_rsp.read());
    listen_port_lock = false;
  }
}
