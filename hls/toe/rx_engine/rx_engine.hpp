#ifndef _RX_ENGINE_
#define _RX_ENGINE_

#include "ipv4/ipv4.hpp"
#include "toe/tcp_header.hpp"
#include "toe/toe_config.hpp"
#include "toe/toe_conn.hpp"
using hls::stream;

struct RxEngFsmMetaData {
  TcpSessionID  session_id;
  IpAddr        src_ip;
  TcpPortNumber dst_port;
  TcpHeaderMeta header;
  // lookup from port table,
  // it will be sent to the target `role`
  NetAXISDest role_id;
  RxEngFsmMetaData() : session_id(0), src_ip(0), dst_port(0), header(), role_id(0) {}
  RxEngFsmMetaData(TcpSessionID  id,
                   IpAddr        src_ip,
                   TcpPortNumber dst_port,
                   TcpHeaderMeta header,
                   NetAXISDest   role)
      : session_id(id), src_ip(src_ip), dst_port(dst_port), header(header), role_id(role) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Rx FSM State: \n" << std::hex;
    sstream << "Session ID: " << this->session_id << "\n";
    sstream << "Source IP: " << this->src_ip << "Dest Port: " << this->dst_port << "\n";
    sstream << header.to_string() << endl;
    sstream << "Role ID: " << this->role_id;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

void RxEngTcpPseudoHeaderInsert(stream<NetAXIS> &    ip_packet,
                                stream<NetAXISWord> &tcp_pseudo_packet_for_checksum,
                                stream<NetAXISWord> &tcp_pseudo_packet_for_rx_eng);

void RxEngParseTcpHeader(stream<NetAXISWord> &        tcp_pseudo_packet,
                         stream<TcpPseudoHeaderMeta> &tcp_meta_data,
                         stream<NetAXISWord> &        tcp_payload);

void RxEngParseTcpHeaderOptions(stream<TcpPseudoHeaderMeta> &tcp_meta_data_in,
                                stream<TcpPseudoHeaderMeta> &tcp_meta_data_out);

void RxEngVerifyChecksumAndPort(stream<ap_uint<16> > &       tcp_checksum_in,
                                stream<TcpPseudoHeaderMeta> &tcp_meta_data_in,
                                stream<bool> &               tcp_payload_dropped_by_checksum,
                                stream<TcpPseudoHeaderMeta> &tcp_meta_data_out,
                                stream<TcpPortNumber> &      rx_eng_to_ptable_check_req);
void RxEngTcpMetaHandler(stream<TcpPseudoHeaderMeta> &tcp_meta_data_in,
                         stream<PtableToRxEngRsp> &   ptable_to_rx_eng_check_rsp,
                         stream<RxEngToSlookupReq> &  rx_eng_to_slookup_req,
                         stream<SessionLookupRsp> &   slookup_to_rx_eng_rsp,
                         stream<RxEngFsmMetaData> &   rx_eng_fsm_meta_data_out,
                         stream<bool> &               tcp_payload_dropped_by_port_or_session,
                         stream<NetAXISDest> &        tcp_payload_tdest,
                         stream<EventWithTuple> &     rx_eng_meta_to_event_eng_set_event);
void RxEngTcpPayloadDropper(stream<NetAXISWord> &tcp_payload_in,
                            stream<bool> &       tcp_payload_dropped_by_checksum,
                            stream<bool> &       tcp_payload_dropped_by_port_or_session,
                            stream<NetAXISDest> &tcp_payload_tdest,
                            stream<bool> &       tcp_payload_dropped_by_rx_fsm,
#if TCP_RX_DDR_BYPASS
                            // send data to rx app intf
                            stream<NetAXISWord> &tcp_payload_out
#else
                            // send data to datamover
                            stream<NetAXIS> &tcp_payload_out
#endif
);
void RxEngTcpFsm(
    // read in rx eng meta data
    stream<RxEngFsmMetaData> &rx_eng_fsm_meta_data_in,
    // Rx SAR R/W
    stream<RxEngToRxSarReq> &rx_eng_to_rx_sar_req,
    stream<RxSarTableEntry> &rx_sar_to_rx_eng_rsp,
    // TX SAR R/W
    stream<RxEngToTxSarReq> &rx_eng_to_tx_sar_req,
    stream<TxSarToRxEngRsp> &tx_sar_to_rx_eng_rsp,
    // state table
    stream<StateTableReq> &rx_eng_to_sttable_req,
    stream<SessionState> & sttable_to_rx_eng_rsp,
    // update timer state
    stream<TcpSessionID> &          rx_eng_to_timer_set_ctimer,
    stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
    stream<TcpSessionID> &          rx_eng_to_timer_clear_ptimer,
    // set event engine
    stream<Event> &rx_eng_fsm_to_event_eng_set_event,
    // to app connection notify, when net app active open a connection
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // to app connection notify, when net app passive open a conection
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    // to app data notify
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification,
#if !TCP_RX_DDR_BYPASS
    stream<DataMoverCmd> &rx_eng_to_mem_cmd,
#endif
    // to app payload should be dropped?
    stream<bool> &tcp_payload_dropped_by_rx_fsm

);

void rx_engine(
    // ip packet
    stream<NetAXIS> &rx_ip_pkt_in,
    // port table check open and TDEST
    stream<TcpPortNumber> &   rx_eng_to_ptable_check_req,
    stream<PtableToRxEngRsp> &ptable_to_rx_eng_check_rsp,
    // to session lookup R/W
    stream<RxEngToSlookupReq> &rx_eng_to_slookup_req,
    stream<SessionLookupRsp> & slookup_to_rx_eng_rsp,
    // FSM
    // Rx SAR R/W
    stream<RxEngToRxSarReq> &rx_eng_to_rx_sar_req,
    stream<RxSarTableEntry> &rx_sar_to_rx_eng_rsp,
    // TX SAR R/W
    stream<RxEngToTxSarReq> &rx_eng_to_tx_sar_req,
    stream<TxSarToRxEngRsp> &tx_sar_to_rx_eng_rsp,
    // state table
    stream<StateTableReq> &rx_eng_to_sttable_req,
    stream<SessionState> & sttable_to_rx_eng_rsp,
    // update timer state
    stream<TcpSessionID> &          rx_eng_to_timer_set_ctimer,
    stream<RxEngToRetransTimerReq> &rx_eng_to_timer_clear_rtimer,
    stream<TcpSessionID> &          rx_eng_to_timer_clear_ptimer,
    // to app connection notify, when net app active open a connection
    stream<OpenConnRspNoTDEST> &rx_eng_to_tx_app_notification,
    // to app connection notify, when net app passive open a conection
    stream<NewClientNotificationNoTDEST> &rx_eng_to_tx_app_new_client_notification,
    // to app data notify
    stream<AppNotificationNoTDEST> &rx_eng_to_rx_app_notification,

    // to event engine
    stream<EventWithTuple> &rx_eng_to_event_eng_set_event,
#if !TCP_RX_DDR_BYPASS
    // tcp payload to mem
    stream<DataMoverCmd> &   rx_eng_to_mem_write_cmd,
    stream<NetAXIS> &        rx_eng_to_mem_write_data,
    stream<DataMoverStatus> &mem_to_rx_eng_write_status
#else
    // tcp payload to rx app
    stream<NetAXISWord> &rx_eng_to_rx_app_data
#endif
);

#endif  //_RX_ENGINE_