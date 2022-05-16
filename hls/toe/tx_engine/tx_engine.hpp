

#ifndef _TX_ENGINE_HPP_
#define _TX_ENGINE_HPP_

#include "ipv4/ipv4.hpp"
#include "toe/memory_access/memory_access.hpp"
#include "toe/tcp_header.hpp"
#include "toe/toe_config.hpp"
#include "toe/toe_intf.hpp"

struct TxEngFsmMetaData {
  ap_uint<32> seq_number;
  ap_uint<32> ack_number;
  ap_uint<16> win_size;
  ap_uint<4>  win_scale;
  ap_uint<16> length;
  ap_uint<1>  ack;
  ap_uint<1>  rst;
  ap_uint<1>  syn;
  ap_uint<1>  fin;
  // lookup from port table,
  // it will be sent to the target `role`
  NetAXISDest role_id;
  TxEngFsmMetaData() {}
  TxEngFsmMetaData(ap_uint<1> ack, ap_uint<1> rst, ap_uint<1> syn, ap_uint<1> fin)
      : seq_number(0)
      , ack_number(0)
      , win_size(0)
      , win_scale(0)
      , length(0)
      , ack(ack)
      , rst(rst)
      , syn(syn)
      , fin(fin) {}
  TxEngFsmMetaData(ap_uint<32> seq_number,
                   ap_uint<32> ack_number,
                   ap_uint<1>  ack,
                   ap_uint<1>  rst,
                   ap_uint<1>  syn,
                   ap_uint<1>  fin)
      : seq_number(seq_number)
      , ack_number(ack_number)
      , win_size(0)
      , length(0)
      , ack(ack)
      , rst(rst)
      , syn(syn)
      , fin(fin) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Tx FSM State: \n" << std::hex;
    sstream << "Flag: " << (this->ack == 1 ? "A/" : "") << (this->rst == 1 ? "R/" : "")
            << (this->syn == 1 ? "S/" : "") << (this->fin == 1 ? "F" : "") << endl;
    sstream << "Seqence Number: " << this->seq_number << endl;
    sstream << "Acknowlegnement Number: " << this->ack_number << endl;
    sstream << "Window Size: " << this->win_size << endl;
    sstream << "Window Scale: " << this->win_scale << endl;
    sstream << "Role ID: " << this->role_id << endl;
    return sstream.str();
  }
#endif
};

/** @ingroup tx_engine
 *
 */
struct IpAddrPair {
  // big endian
  IpAddr src_ip_addr;
  IpAddr dst_ip_addr;
  IpAddrPair() {}
  IpAddrPair(IpAddr src_ip, IpAddr dst_ip) : src_ip_addr(src_ip), dst_ip_addr(dst_ip) {}
};

void TxEngTcpFsm(
    // from ack delay to tx engine req
    stream<EventWithTuple> &ack_delay_to_tx_eng_event,
    stream<ap_uint<1> > &   tx_eng_read_count_fifo,
    // rx sar read req
    stream<TcpSessionID> &  tx_eng_to_rx_sar_lup_req,
    stream<RxSarLookupRsp> &rx_sar_to_tx_eng_lup_rsp,
    // tx sar r/w req
    stream<TxEngToTxSarReq> &tx_eng_to_tx_sar_req,
    stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
    // timer
    stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
    stream<TcpSessionID> &          tx_eng_to_timer_set_ptimer,
    // to session lookup req
    stream<ap_uint<16> > &tx_eng_to_slookup_rev_table_req,
    // inner connect
    // to construct ipv4 header
    stream<ap_uint<16> > &tx_tcp_payload_length,
    // to construct the tcp pseudo full header
    stream<TxEngFsmMetaData> &tx_eng_fsm_meta_data,
    // to read mem
    stream<MemBufferRWCmd> &tx_eng_to_mem_cmd_in,
#if (TCP_NODELAY)
    stream<bool> &tx_eng_is_ddr_bypass,
#endif
    // fourtuple source is from tx enigne  FSM or slookup, true = slookup, false = FSM
    stream<bool> &tx_four_tuple_source,
    // four tuple from tx enigne FSM
    stream<FourTuple> &tx_eng_fsm_four_tuple);

void TxEngConstructPseudoPacket(stream<NetAXIS> &tx_tcp_pseduo_header,
                                stream<bool> &   tx_tcp_packet_contains_payload,
                                stream<NetAXIS> &tx_tcp_packet_payload,
                                stream<NetAXIS> &tx_tcp_pseduo_packet_for_tx_eng,
                                stream<NetAXIS> &tx_tcp_pseduo_packet_for_checksum);

void TxEngRemovePseudoHeader(stream<NetAXIS> &tx_tcp_pseduo_packet_for_tx_eng,
                             stream<NetAXIS> &tx_tcp_packet);

void TxEngConstructIpv4Packet(stream<NetAXIS> &     tx_ipv4_header,
                              stream<ap_uint<16> > &tx_tcp_checksum,
                              stream<NetAXIS> &     tx_tcp_packet,
                              stream<NetAXIS> &     tx_ip_pkt_out);

void tx_engine(
    // from ack delay to tx engine req
    stream<EventWithTuple> &ack_delay_to_tx_eng_event,
    stream<ap_uint<1> > &   tx_eng_read_count_fifo,
    // rx sar
    stream<TcpSessionID> &  tx_eng_to_rx_sar_lup_req,
    stream<RxSarLookupRsp> &rx_sar_to_tx_eng_lup_rsp,
    // tx sar
    stream<TxEngToTxSarReq> &tx_eng_to_tx_sar_req,
    stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
    // timer
    stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
    stream<TcpSessionID> &          tx_eng_to_timer_set_ptimer,
    // to session lookup req/rsp
    stream<ap_uint<16> > &          tx_eng_to_slookup_rev_table_req,
    stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
    // to datamover cmd
    stream<DataMoverCmd> &mover_read_mem_cmd_out,
    // read data from data mem
    stream<NetAXIS> &mover_read_mem_data_in,
#if (TCP_NODELAY)
    stream<NetAXIS> &tx_app_to_tx_eng_data,
#endif
    // to outer
    stream<NetAXIS> &tx_ip_pkt_out

);


#endif  // TX_ENGINE_HPP