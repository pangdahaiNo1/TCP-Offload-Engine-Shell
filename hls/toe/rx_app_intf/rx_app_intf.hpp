#include "toe/toe_conn.hpp"

using namespace hls;

/** @defgroup rx_app_intf RX Application Interface
 *  @ingroup app_if
 *
 */
void RxAppPortHandler(stream<NetAXISListenPortReq> &net_app_to_rx_app_listen_port_req,
                      stream<NetAXISListenPortRsp> &rx_app_to_net_app_listen_port_rsp,
                      stream<ListenPortReq> &       rx_app_to_ptable_listen_port_req,
                      stream<ListenPortRsp> &       ptable_to_rx_app_listen_port_rsp);

void RxAppDataHandler(stream<NetAXISAppReadReq> &net_app_to_rx_app_recv_data_req,
                      stream<NetAXISAppReadRsp> &rx_app_to_net_app_recv_data_rsp,
                      stream<RxSarAppReqRsp> &   rx_app_to_rx_sar_req,
                      stream<RxSarAppReqRsp> &   rx_sar_to_rx_app_rsp,
#if !(TCP_RX_DDR_BYPASS)
                      // inner read mem cmd
                      stream<MemBufferRWCmd> &rx_app_to_mem_read_cmd,
                      // read mem payload
                      stream<NetAXISWord> &mover_to_rx_app_read_data,
#else
                      // rx engine data to net app
                      stream<NetAXISWord> &rx_eng_to_rx_app_data,
#endif
                      stream<NetAXIS> &net_app_recv_data);

void NetAppNotificationTdestHandler(stream<AppNotificationNoTDEST> &app_notification_no_tdest,
                                    stream<NetAXISAppNotification> &net_app_notification,
                                    stream<TcpSessionID> &          slookup_tdest_lookup_req,
                                    stream<NetAXISDest> &           slookup_tdest_lookup_rsp);

void rx_app_intf(
    // net role - port handler
    stream<NetAXISListenPortReq> &net_app_to_rx_app_listen_port_req,
    stream<NetAXISListenPortRsp> &rx_app_to_net_app_listen_port_rsp,
    // rxapp - port table
    stream<ListenPortReq> &rx_app_to_ptable_listen_port_req,
    stream<ListenPortRsp> &ptable_to_rx_app_listen_port_rsp,
    // not role - data handler
    stream<NetAXISAppReadReq> &net_app_to_rx_app_recv_data_req,
    stream<NetAXISAppReadRsp> &rx_app_to_net_app_recv_data_rsp,
    // rx sar req/rsp
    stream<RxSarAppReqRsp> &rx_app_to_rx_sar_req,
    stream<RxSarAppReqRsp> &rx_sar_to_rx_app_rsp,
#if !(TCP_RX_DDR_BYPASS)
    // data from mem to net app
    stream<DataMoverCmd> &rx_app_to_mover_read_cmd,
    stream<NetAXIS> &     mover_to_rx_app_read_data,
#else
    // data from rx engine to net app
    stream<NetAXISWord> &rx_eng_to_rx_app_data,
#endif
    stream<NetAXIS> &net_app_recv_data,
    // net role app - notification
    // Rx engine to Rx app
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification,
    // Timer to Rx app
    stream<AppNotificationNoTDEST> &rtimer_to_rx_app_notification,
    // lookup for tdest, req/rsp connect to slookup controller
    stream<TcpSessionID> &rx_app_to_slookup_tdest_lookup_req,
    stream<NetAXISDest> & slookup_to_rx_app_tdest_lookup_rsp,
    // appnotifacation to net app with TDEST
    stream<NetAXISAppNotification> &net_app_notification);