#ifndef _TX_ENGINE_HPP_
#define _TX_ENGINE_HPP_

#include "toe/memory_access/memory_access.hpp"
#include "toe/toe_config.hpp"
#include "toe/toe_conn.hpp"

struct TxEngFsmMetaData {
  ap_uint<32> seq_number;
  ap_uint<32> ack_number;
  ap_uint<16> win_size;
  ap_uint<4>  win_scale;
  // length = tcp payload length + tcp header options length
  ap_uint<16> length;
  ap_uint<1>  ack;
  ap_uint<1>  rst;
  ap_uint<1>  syn;
  ap_uint<1>  fin;
  // lookup from port table,
  // it will be sent to the target `role`
  NetAXISDest role_id;
  TxEngFsmMetaData()
      : seq_number(0)
      , ack_number(0)
      , win_size(0)
      , win_scale(0)
      , length(0)
      , ack(0)
      , rst(0)
      , syn(0)
      , fin(0)
      , role_id(0) {}
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
    sstream << "Flag: " << (ack == 1 ? "A/" : "") << (rst == 1 ? "R/" : "")
            << (syn == 1 ? "S/" : "") << (fin == 1 ? "F" : "") << "\n";
    sstream << "Seqence Number: " << seq_number.to_string(16) << "\n";
    sstream << "Acknowlegnement Number: " << ack_number.to_string(16) << "\n";
    sstream << "Window Size: " << win_size.to_string(16) << "\n";
    sstream << "Window Scale: " << win_scale << "\n";
    sstream << "Payload+Options Length: " << length.to_string(16) << "\n";
    sstream << "Role ID: " << role_id;
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

INLINE ap_uint<32> RandomVal() {
#ifndef __SYNTHESIS__
  return 0xFF73CE98;  // just for simulation
#else
  // init value
  static ap_uint<32> ramdom_val = 0x562301af;
  ramdom_val                    = (ramdom_val * 8) xor ramdom_val;
  return ramdom_val;
#endif
}

// in big endian
struct IpAddrPair {
  // big endian
  IpAddr src_ip_addr;
  IpAddr dst_ip_addr;
  IpAddrPair() : src_ip_addr(0), dst_ip_addr(0) {}
  IpAddrPair(IpAddr src_ip, IpAddr dst_ip) : src_ip_addr(src_ip), dst_ip_addr(dst_ip) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "SrcIP: " << SwapByte(src_ip_addr).to_string(16) << "\t"
            << "DstIP: " << SwapByte(dst_ip_addr).to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};

void TxEngTcpFsm(
    // from ack delay to tx engine req
    stream<EventWithTuple> &ack_delay_to_tx_eng_event,
    stream<ap_uint<1> >    &tx_eng_read_count_fifo,
    // rx sar read req
    stream<TcpSessionID>   &tx_eng_to_rx_sar_lup_req,
    stream<RxSarLookupRsp> &rx_sar_to_tx_eng_lup_rsp,
    // tx sar r/w req
    stream<TxEngToTxSarReq> &tx_eng_to_tx_sar_req,
    stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
    // timer
    stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
    stream<TcpSessionID>           &tx_eng_to_timer_set_ptimer,
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

void TxEngFourTupleHandler(stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
                           stream<bool>                   &tx_four_tuple_source,
                           stream<FourTuple>              &tx_eng_fsm_four_tuple,
                           stream<FourTuple>              &tx_four_tuple_for_tcp_header,
                           stream<IpAddrPair>             &tx_ip_pair_for_ip_header);

void TxEngPseudoFullHeaderConstruct(stream<TxEngFsmMetaData> &tx_eng_fsm_meta_data,
                                    stream<FourTuple>        &tx_four_tuple_for_tcp_header,
                                    stream<bool>             &tx_tcp_packet_contains_payload,
                                    stream<NetAXISWord>      &tx_tcp_pseudo_full_header_out);

void TxEngConstructPseudoPacket(stream<NetAXISWord> &tx_tcp_pseudo_full_header,
                                stream<bool>        &tx_tcp_packet_contains_payload,
                                stream<NetAXISWord> &tx_tcp_packet_payload,
                                stream<NetAXISWord> &tx_tcp_pseudo_packet_for_tx_eng,
                                stream<NetAXISWord> &tx_tcp_pseudo_packet_for_checksum);

void TxEngRemovePseudoHeader(stream<NetAXISWord> &tx_tcp_pseduo_packet_for_tx_eng,
                             stream<NetAXISWord> &tx_tcp_packet);

void TxEngConstructIpv4Header(stream<ap_uint<16> > &tx_tcp_payload_length,
                              stream<IpAddrPair>   &tx_tcp_ip_pair,
                              stream<NetAXISWord>  &tx_ipv4_header);

void TxEngConstructIpv4Packet(stream<NetAXISWord>  &tx_ipv4_header,
                              stream<ap_uint<16> > &tx_tcp_checksum,
                              stream<NetAXISWord>  &tx_tcp_packet,
                              stream<NetAXIS>      &tx_ip_pkt_out);

void tx_engine(
    // from ack delay to tx engine req
    stream<EventWithTuple> &ack_delay_to_tx_eng_event,
    stream<ap_uint<1> >    &tx_eng_read_count_fifo,
    // rx sar
    stream<TcpSessionID>   &tx_eng_to_rx_sar_lup_req,
    stream<RxSarLookupRsp> &rx_sar_to_tx_eng_lup_rsp,
    // tx sar
    stream<TxEngToTxSarReq> &tx_eng_to_tx_sar_req,
    stream<TxSarToTxEngRsp> &tx_sar_to_tx_eng_rsp,
    // timer
    stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
    stream<TcpSessionID>           &tx_eng_to_timer_set_ptimer,
    // to session lookup req/rsp
    stream<ap_uint<16> >           &tx_eng_to_slookup_rev_table_req,
    stream<ReverseTableToTxEngRsp> &slookup_rev_table_to_tx_eng_rsp,
    // to datamover cmd
    stream<DataMoverCmd> &tx_eng_to_mover_read_cmd,
    // read data from mem
    stream<NetAXIS> &mover_to_tx_eng_read_data,
#if (TCP_NODELAY)
    stream<NetAXIS> &tx_app_to_tx_eng_data,
#endif
    // to outer
    stream<NetAXIS> &tx_ip_pkt_out);
#endif  // TX_ENGINE_HPP