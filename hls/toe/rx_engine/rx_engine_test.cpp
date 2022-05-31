#include "rx_engine.hpp"
#include "toe/mock/mock_toe.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

void EmptyTcpPayloadFifos(std::ofstream &      out_stream,
                          stream<NetAXISWord> &tcp_payload_fifo,
                          int                  sim_cycle) {
  NetAXISWord tcp_payload;
  while (!tcp_payload_fifo.empty()) {
    tcp_payload_fifo.read(tcp_payload);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Rx Eng recvd payload\t";
    out_stream << dec << KeepToLength(tcp_payload.keep) << " Bytes word to Net APP "
               << tcp_payload.dest << "\n";
  }
}

void EmptyTcpRxPseudoHeaderFifos(std::ofstream &              out_stream,
                                 stream<TcpPseudoHeaderMeta> &tcp_pseudo_header_meta_parsed,
                                 stream<ap_uint<16> > &       tcp_pseudo_packet_checksum,
                                 int                          sim_cycle) {
  TcpPseudoHeaderMeta meta_header;

  ap_uint<16> tcp_checksum;
  while (!tcp_pseudo_header_meta_parsed.empty()) {
    tcp_pseudo_header_meta_parsed.read(meta_header);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Rx Eng reced seg\n";
    out_stream << meta_header.to_string() << "\n";
  }
  while (!tcp_pseudo_packet_checksum.empty()) {
    tcp_pseudo_packet_checksum.read(tcp_checksum);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Rx Eng reced seg checksum is\t";
    out_stream << tcp_checksum.to_string(16) << "\n";
  }
}

void TestTcpPseudoHeaderParser(stream<NetAXISWord> &input_tcp_packet) {
  // open output file
  std::ofstream outputFile;
  outputFile.open("./out_tcp_pseudo_header.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }
  // some fifos
  stream<NetAXISWord>  tcp_pseudo_packet_for_checksum_fifo("tcp_pseudo_packet_for_checksum_fifo");
  stream<NetAXISWord>  tcp_pseudo_packet_for_rx_eng_fifo("tcp_pseudo_packet_for_rx_eng_fifo");
  stream<SubChecksum>  tcp_pseudo_packet_subchecksum_fifo("tcp_pseudo_packet_subchecksum_fifo");
  stream<ap_uint<16> > tcp_pseudo_packet_checksum_fifo("tcp_pseudo_packet_checksum_fifo");
  stream<TcpPseudoHeaderMeta> tcp_pseudo_header_meta_fifo("tcp_pseudo_header_meta_fifo");
  stream<TcpPseudoHeaderMeta> tcp_pseudo_header_meta_parsed_fifo(
      "tcp_pseudo_header_meta_parsed_fifo");
  stream<NetAXISWord> tcp_payload_fifo("tcp_payload_fifo");
  stream<NetAXIS>     tcp_payload_fifo_for_save("tcp_payload_fifo2");
  stream<NetAXISWord> tcp_payload_fifo2("tcp_payload_fifo2");

  // simulation
  int sim_cycle = 0;
  while (sim_cycle < 200) {
    RxEngTcpPseudoHeaderInsert(
        input_tcp_packet, tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_for_rx_eng_fifo);
    ComputeSubChecksum(tcp_pseudo_packet_for_checksum_fifo, tcp_pseudo_packet_subchecksum_fifo);
    CheckChecksum(tcp_pseudo_packet_subchecksum_fifo, tcp_pseudo_packet_checksum_fifo);
    RxEngParseTcpHeader(
        tcp_pseudo_packet_for_rx_eng_fifo, tcp_pseudo_header_meta_fifo, tcp_payload_fifo);
    RxEngParseTcpHeaderOptions(tcp_pseudo_header_meta_fifo, tcp_pseudo_header_meta_parsed_fifo);
    if (!tcp_payload_fifo.empty()) {
      NetAXISWord cur_word = tcp_payload_fifo.read();
      tcp_payload_fifo_for_save.write(cur_word.to_net_axis());
      tcp_payload_fifo2.write(cur_word);
    }
    EmptyTcpRxPseudoHeaderFifos(
        outputFile, tcp_pseudo_header_meta_parsed_fifo, tcp_pseudo_packet_checksum_fifo, sim_cycle);
    EmptyTcpPayloadFifos(outputFile, tcp_payload_fifo2, sim_cycle);
    sim_cycle++;
  }
  SaveNetAXISToFile(tcp_payload_fifo_for_save, "tcp_payload.dat");
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *              golden_input_file_tcp = argv[1];
  stream<NetAXIS>     input_golden_tcp_packets_read_in("input_golden_tcp_packets_read_in");
  stream<NetAXISWord> input_golden_tcp_packets("input_golden_tcp_packets");
  PcapToStream(golden_input_file_tcp, true, input_golden_tcp_packets_read_in);
  NetAXIStreamToNetAXIStreamWord(input_golden_tcp_packets_read_in, input_golden_tcp_packets);

  TestTcpPseudoHeaderParser(input_golden_tcp_packets);
  // NetAXIStreamWordToNetAXIStream(tcp_pseudo_packet, tcp_pseudo_packet_for_save);
  // SaveNetAXISToFile(tcp_pseudo_packet_for_save, "tcp_pseudo_packet.dat");
  return 0;
}
