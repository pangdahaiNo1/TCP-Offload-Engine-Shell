#include "toe/mock/mock_toe.hpp"
#include "toe/rx_engine/rx_engine.hpp"
#include "toe/tx_engine/tx_engine.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

void TestTcpTxConstructIpv4Pkt(stream<NetAXISWord> &input_tcp_packet) {
  // open output file
  // std::ofstream outputFile;
  // outputFile.open("./out_tcp_pseudo_header.dat");
  // if (!outputFile) {
  //   std::cout << "Error: could not open test output file." << std::endl;
  // }
  // some fifos
  stream<NetAXISWord> input_tcp_packet_ip_header("input_tcp_packet_ip_header");
  stream<NetAXISWord> tx_tcp_pseudo_packet_for_checksum_fifo(
      "tx_tcp_pseudo_packet_for_checksum_fifo");
  stream<NetAXISWord>  tx_tcp_packet_fifo("tx_tcp_packet_fifo");
  stream<NetAXISWord>  tx_tcp_pseudo_packet_for_tx_eng_fifo("tx_tcp_pseudo_packet_for_tx_eng_fifo");
  stream<SubChecksum>  tcp_pseudo_packet_subchecksum_fifo("tcp_pseudo_packet_subchecksum_fifo");
  stream<ap_uint<16> > tcp_pseudo_packet_checksum_fifo("tcp_pseudo_packet_checksum_fifo");
  stream<NetAXIS>      tx_tcp_ip_packet_fifo("tx_tcp_ip_packet_fifo");

  stream<NetAXISWord> input_tcp_packet_copy;
  // simulation
  // construct ip header from input tcp packet
  int sim_cycle = 0;
  while (sim_cycle < 200) {
    // construct ip header from input tcp packet
    if (!input_tcp_packet.empty()) {
      NetAXISWord cur_word = input_tcp_packet.read();
      input_tcp_packet_copy.write(cur_word);
      NetAXISWord ip_word(cur_word.data(IPV4_HEADER_WIDTH - 1, 0), 0, 0xFFFFF, 1);
      input_tcp_packet_ip_header.write(ip_word);
      while (!cur_word.last) {
        cur_word = input_tcp_packet.read();
        input_tcp_packet_copy.write(cur_word);
      }
    }
    // construct tcp pseudo packet from input tcp packet
    RxEngTcpPseudoHeaderInsert(input_tcp_packet_copy,
                               tx_tcp_pseudo_packet_for_checksum_fifo,
                               tx_tcp_pseudo_packet_for_tx_eng_fifo);

    // the tcp pseudo packet input contains the valid checksum, then calculate checksum here will
    // make all tcp checksum = 0, if not zero, the testbench are failed
    ComputeSubChecksum(tx_tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_subchecksum_fifo);
    CheckChecksum(tcp_pseudo_packet_subchecksum_fifo, tcp_pseudo_packet_checksum_fifo);
    TxEngRemovePseudoHeader(tx_tcp_pseudo_packet_for_tx_eng_fifo, tx_tcp_packet_fifo);
    TxEngConstructIpv4Packet(input_tcp_packet_ip_header,
                             tcp_pseudo_packet_checksum_fifo,
                             tx_tcp_packet_fifo,
                             tx_tcp_ip_packet_fifo);
    sim_cycle++;
  }
  StreamToPcap("tx_ip_tcp.pcap", true, true, tx_tcp_ip_packet_fifo, true);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *input_tcp_pcap_file = argv[1];
  cout << "Read TCP Packets from " << input_tcp_pcap_file << endl;
  stream<NetAXIS>     input_tcp_ip_pkt_read_in("input_tcp_ip_pkt_read_in");
  stream<NetAXISWord> input_tcp_ip_pkt("input_tcp_ip_pkt");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_ip_pkt_read_in);
  NetAXIStreamToNetAXIStreamWord(input_tcp_ip_pkt_read_in, input_tcp_ip_pkt);

  TestTcpTxConstructIpv4Pkt(input_tcp_ip_pkt);
  return 0;
}
