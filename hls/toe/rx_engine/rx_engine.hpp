#ifndef _RX_ENGINE_
#define _RX_ENGINE_

#include "ipv4/ipv4.hpp"
#include "toe/tcp_header.hpp"

using hls::stream;

void RxEngTcpPseudoHeaderInsert(stream<NetAXIS> &ip_packet,
                                stream<NetAXIS> &tcp_pseudo_packet_for_checksum,
                                stream<NetAXIS> &tcp_pseudo_packet_for_rx_eng);

void RxEngParseTcpHeader(stream<NetAXIS> &            tcp_pseudo_packet,
                         stream<TcpPseudoHeaderMeta> &tcp_meta_data,
                         stream<NetAXIS> &            tcp_payload);
#endif  //_RX_ENGINE_