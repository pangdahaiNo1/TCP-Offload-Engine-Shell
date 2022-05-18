#include "toe/mock/mock_toe.hpp"
#include "toe/rx_engine/rx_engine.hpp"
#include "toe/tx_engine/tx_engine.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

void TestPseduoPktoIpv4Pkt(stream<NetAXIS> &golden_tcp_pseudo_pkt_in,
                           stream<NetAXIS> &golden_ip_header_in) {
  stream<TcpPseudoFullHeader> tcp_pseudo_pkts_header;
  stream<NetAXIS>             tcp_pseudo_pkts_fifo;
  stream<NetAXIS>             tcp_pseudo_pkts_for_checksum_fifo;
  while (!golden_tcp_pseudo_pkt_in.empty()) {
    TcpPseudoFullHeader pkts;
    NetAXIS             cur_word = golden_tcp_pseudo_pkt_in.read();
    tcp_pseudo_pkts_fifo.write(cur_word);
    tcp_pseudo_pkts_for_checksum_fifo.write(cur_word);
    pkts.FromWord(cur_word.data(TCP_HEADER_WITH_PSEUDO_WIDTH - 1, 0));
    cout << std::hex << pkts.to_string() << endl;
    ;
    while (!cur_word.last) {
      cur_word = golden_tcp_pseudo_pkt_in.read();
      tcp_pseudo_pkts_fifo.write(cur_word);
      tcp_pseudo_pkts_for_checksum_fifo.write(cur_word);
    }
  }
  int sim_cycle = 0;
  // tx engine test bench
  stream<NetAXIS>      tcp_packet_fifo;
  stream<SubChecksum>  tcp_packet_subchecksum_fifo;
  stream<ap_uint<16> > tcp_packet_checksum_fifo;
  stream<NetAXIS>      tcp_ip_packet_fifo;
  while (sim_cycle < 50) {
    // the tcp pseudo packet input contains the valid checksum, then calculate checksum here will
    // make all tcp checksum = 0, if not zero, the testbench are failed
    ComputeSubChecksum(tcp_pseudo_pkts_for_checksum_fifo, tcp_packet_subchecksum_fifo);
    CheckChecksum(tcp_packet_subchecksum_fifo, tcp_packet_checksum_fifo);
    TxEngRemovePseudoHeader(tcp_pseudo_pkts_fifo, tcp_packet_fifo);
    TxEngConstructIpv4Packet(
        golden_ip_header_in, tcp_packet_checksum_fifo, tcp_packet_fifo, tcp_ip_packet_fifo);
    sim_cycle++;
  }
  StreamToPcap("tx_ip_tcp.pcap", true, true, tcp_ip_packet_fifo, true);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *input_tcp_pcap_file = argv[1];
  cout << "Read TCP Packets from " << input_tcp_pcap_file << endl;
  stream<NetAXIS> input_tcp_ip_pkt("input_tcp_ip_pkt");
  stream<NetAXIS> input_tcp_ip_pkt_for_ipheader("input_tcp_ip_pkt_for_ipheader");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_ip_pkt);
  PcapToStream(input_tcp_pcap_file, true, input_tcp_ip_pkt_for_ipheader);

  int sim_cycle = 0;
  // construct ip header
  stream<NetAXIS> golden_ip_header_in;
  while (!input_tcp_ip_pkt_for_ipheader.empty()) {
    NetAXIS cur_word = input_tcp_ip_pkt_for_ipheader.read();
    NetAXIS ip_word(cur_word.data(IPV4_HEADER_WIDTH - 1, 0), 0, 0xFFFFF, 1);
    golden_ip_header_in.write(ip_word);
    while (!cur_word.last) {
      cur_word = input_tcp_ip_pkt_for_ipheader.read();
    }
  }

  stream<NetAXIS> tcp_pseudo_pkt1;
  stream<NetAXIS> tcp_pseudo_pkt2;
  // construct pseudo packet
  while (sim_cycle < 50) {
    RxEngTcpPseudoHeaderInsert(input_tcp_ip_pkt, tcp_pseudo_pkt1, tcp_pseudo_pkt2);
    sim_cycle++;
  }
  SaveNetAXISToFile(tcp_pseudo_pkt1, "tcp_pseudo_pkt1.dat");
  TestPseduoPktoIpv4Pkt(tcp_pseudo_pkt2, golden_ip_header_in);
  return 0;
}
