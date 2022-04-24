#include "toe/toe_intf.hpp"

using namespace hls;

/** @defgroup rx_app_intf RX Application Interface
 *  @ingroup app_if
 *
 */
void rx_app_intf(stream<NetAXISListenPortReq> &rx_app_to_rx_intf_listen_port_req,
                 stream<NetAXISListenPortRsp> &rx_intf_to_rx_app_listen_port_rsp,
                 stream<NetAXISListenPortReq> &rx_intf_to_ptable_listen_port_req,
                 stream<NetAXISListenPortRsp> &ptable_to_rx_intf_listen_port_rsp);