#ifndef _RX_ENGINE_
#define _RX_ENGINE_

#include "ipv4/ipv4.hpp"
#include "toe/tcp_header.hpp"
#include "toe/toe_config.hpp"
#include "toe/toe_intf.hpp"
using hls::stream;

struct RxEngFsmMetaData {
  TcpSessionID  session_id;
  IpAddr        src_ip;
  TcpPortNumber dst_port;
  TcpHeaderMeta header;
  // lookup from port table,
  // it will be sent to the target `role`
  NetAXISDest role_id;
  RxEngFsmMetaData() {}
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
    sstream << header.to_string();
    sstream << "Role ID: " << this->role_id << endl;
    return sstream.str();
  }
#endif
};

void RxEngTcpPseudoHeaderInsert(stream<NetAXIS> &ip_packet,
                                stream<NetAXIS> &tcp_pseudo_packet_for_checksum,
                                stream<NetAXIS> &tcp_pseudo_packet_for_rx_eng);

void RxEngParseTcpHeader(stream<NetAXIS> &            tcp_pseudo_packet,
                         stream<TcpPseudoHeaderMeta> &tcp_meta_data,
                         stream<NetAXIS> &            tcp_payload);

void RxEngParseTcpHeaderOptions(stream<TcpPseudoHeaderMeta> &tcp_meta_data_in,
                                stream<TcpPseudoHeaderMeta> &tcp_meta_data_out);

void rx_engine(
    // ip packet
    stream<NetAXIS> &ip_packet,
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
    stream<OpenSessionStatus> &rx_eng_to_tx_app_notification,
    // to app connection notify, when net app passive open a conection
    stream<NewClientNotification> &rx_eng_to_tx_app_new_client_notification,
    // to app data notify
    stream<AppNotification> &rx_eng_to_rx_app_notification,

    // to event engine
    stream<EventWithTuple> &rx_eng_to_event_eng_set_event,
    // tcp payload to rx app
    stream<NetAXIS> &rx_eng_to_rx_app_data

);

#endif  //_RX_ENGINE_