
#include "memory_access.hpp"
#include "toe/mock/mock_toe.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace hls;

void EmptyFifos(std::ofstream &       out_stream,
                stream<NetAXIS> &     mover_mem_data_out,
                stream<DataMoverCmd> &mover_mem_cmd_out,
                int                   sim_cycle) {
  DataMoverCmd cmd_out;
  NetAXIS      data_out;

  while (!mover_mem_cmd_out.empty()) {
    mover_mem_cmd_out.read(cmd_out);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App write cmd: \t";
    out_stream << cmd_out.to_string() << "\n";
  }
  while (!mover_mem_data_out.empty()) {
    mover_mem_data_out.read(data_out);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx App write \t";
    out_stream << dec << KeepToLength(data_out.keep) << " Bytes word to mem\n";
  }
}

int main(int argc, char **argv) {
  char *input_tcp_pcap_file = argv[1];
  cout << "Read TCP Packets from " << input_tcp_pcap_file << endl;
  stream<NetAXIS> input_tcp_packets("input_tcp_packets");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_packets);
  // open output file
  std::ofstream outputFile;

  outputFile.open("./out.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }

  stream<NetAXIS>      tx_app_to_mem_data_in;
  stream<DataMoverCmd> tx_app_to_mem_cmd_in;
  stream<NetAXIS>      mover_mem_data_out;
  stream<DataMoverCmd> mover_mem_cmd_out;

  NetAXIS cur_word{};
  // ip packet length, because it remove the eth header
  TcpSessionBuffer cur_word_length = 0;
  DataMoverCmd     cur_cmd         = DataMoverCmd{};

  int sim_cycle = 0;
  while (sim_cycle < 200) {
    switch (sim_cycle) {
      case 1:
        do {
          input_tcp_packets.read(cur_word);
          tx_app_to_mem_data_in.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
        } while (cur_word.last != 1);
        cur_cmd = DataMoverCmd(0x12345678, cur_word_length);
        tx_app_to_mem_cmd_in.write(cur_cmd);
        break;
      case 2:
        cur_word_length = 0;
        do {
          input_tcp_packets.read(cur_word);
          tx_app_to_mem_data_in.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
        } while (cur_word.last != 1);
        cur_cmd = DataMoverCmd(0x12345678, cur_word_length);
        tx_app_to_mem_cmd_in.write(cur_cmd);
        break;
      case 3:
        cur_word_length = 0;
        do {
          input_tcp_packets.read(cur_word);
          tx_app_to_mem_data_in.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
        } while (cur_word.last != 1);
        cur_cmd = DataMoverCmd(0x12345678, cur_word_length);
        tx_app_to_mem_cmd_in.write(cur_cmd);
        break;
      case 4:
        cur_word_length = 0;
        do {
          input_tcp_packets.read(cur_word);
          tx_app_to_mem_data_in.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
        } while (cur_word.last != 1);
        cur_cmd = DataMoverCmd(0x123FFF01, cur_word_length);
        tx_app_to_mem_cmd_in.write(cur_cmd);
        break;
      default:
        break;
    }
    TxAppWriteDataToMem(
        tx_app_to_mem_data_in, tx_app_to_mem_cmd_in, mover_mem_data_out, mover_mem_cmd_out);
    EmptyFifos(outputFile, mover_mem_data_out, mover_mem_cmd_out, sim_cycle);
    sim_cycle++;
  }

  return 0;
}
