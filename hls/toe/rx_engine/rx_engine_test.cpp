#include "rx_engine.hpp"
#include "toe/mock/mock_toe.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

int main(int argc, char **argv) {
  stream<NetAXIS> input_golden_tcp_packets("input_golden_tcp_packets");
  char *          golden_input_file_tcp = argv[1];

  cout << "Read TCP Packets from " << golden_input_file_tcp << endl;
  PcapToStream(golden_input_file_tcp, true, input_golden_tcp_packets);
  Ipv4Header ipv4_header;

  stream<NetAXIS>             tcp_pseudo_packet("tcp_pseudo_packet");
  stream<NetAXIS>             checksum_psedo_packet("checksum_psedo_packet");
  stream<TcpPseudoHeaderMeta> tcp_meta1("tcp_meta1");
  stream<TcpPseudoHeaderMeta> tcp_meta2("tcp_meta2");
  stream<NetAXIS>             tcp_payload("tcp_payload");
  for (int i = 0; i < 100; i++) {
    RxEngTcpPseudoHeaderInsert(input_golden_tcp_packets, checksum_psedo_packet, tcp_pseudo_packet);
  }
  int seg_count = 0;
  while (!checksum_psedo_packet.empty()) {
    stream<NetAXIS>      tmp_tcp_packet("tmp_tcp_packet");
    stream<SubChecksum>  tmp_subchecksum("tmp_subchecksum");
    stream<ap_uint<16> > tmp_checksum("tmp_checksum");
    ComputeSubChecksum(checksum_psedo_packet, tmp_subchecksum);
    // if (!tmp_subchecksum.empty()) {
    CheckChecksum(tmp_subchecksum, tmp_checksum);
    tmp_checksum.read();
    //}
    RxEngParseTcpHeader(tmp_tcp_packet, tcp_meta1, tcp_payload);
    while (!tcp_meta1.empty()) {
      RxEngParseTcpHeaderOptions(tcp_meta1, tcp_meta2);
      while (!tcp_meta2.empty()) {
        TcpPseudoHeaderMeta tmp;
        tcp_meta2.read(tmp);
        cout << hex << tmp.to_string() << seg_count << endl;
        seg_count++;
      }
      cout << tcp_meta1.read().to_string() << seg_count << endl;
    }
  }

  SaveNetAXISToFile(tcp_pseudo_packet, "tcp_pseudo_packet.dat");
  return 0;
}
