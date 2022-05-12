
#include "memory_access.hpp"
#include "toe/mock/mock_toe.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace hls;

void EmptyTxWriteDataFifos(std::ofstream &       out_stream,
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

void EmptyTxReadCmdFifos(std::ofstream &                     out_stream,
                         stream<MemBufferRWCmdDoubleAccess> &mem_buffer_double_access,
                         stream<DataMoverCmd> &              mover_mem_cmd_out,
                         int                                 sim_cycle) {
  DataMoverCmd               cmd_out;
  MemBufferRWCmdDoubleAccess double_access;

  while (!mover_mem_cmd_out.empty()) {
    mover_mem_cmd_out.read(cmd_out);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx eng read mem cmd: \t";
    out_stream << cmd_out.to_string() << "\n";
  }
  while (!mem_buffer_double_access.empty()) {
    mem_buffer_double_access.read(double_access);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx eng read mem double access check \t";
    out_stream << double_access.to_string() << "\n";
  }
}

void EmptyTxReadDataFifos(std::ofstream &  out_stream,
                          stream<NetAXIS> &to_tx_eng_read_data,
                          int              sim_cycle) {
  NetAXIS data_out;

  while (!to_tx_eng_read_data.empty()) {
    to_tx_eng_read_data.read(data_out);
    out_stream << "Cycle " << std::dec << sim_cycle << ": Tx Eng Read \t";
    out_stream << dec << KeepToLength(data_out.keep) << " Bytes word from mem\n";
  }
}

void TestTxWriteToMem(stream<NetAXIS> &input_tcp_packets) {
  std::ofstream outputFile;

  outputFile.open("./out_write_data.dat");
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
        cur_cmd = DataMoverCmd(0x12350000, cur_word_length);
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
        cur_cmd = DataMoverCmd(0x123FFFF1, cur_word_length);
        tx_app_to_mem_cmd_in.write(cur_cmd);
        break;
      default:
        break;
    }
    TxAppWriteDataToMem(
        tx_app_to_mem_data_in, tx_app_to_mem_cmd_in, mover_mem_data_out, mover_mem_cmd_out);
    EmptyTxWriteDataFifos(outputFile, mover_mem_data_out, mover_mem_cmd_out, sim_cycle);
    sim_cycle++;
  }
}

void TestTxEngReadCmd() {
  std::ofstream outputFile;

  outputFile.open("./out_read_cmd.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }

  stream<MemBufferRWCmd>             tx_eng_to_mem_cmd_in;
  stream<DataMoverCmd>               mover_read_mem_cmd_out;
  stream<MemBufferRWCmdDoubleAccess> mem_buffer_double_access;

  int sim_cycle = 0;
  while (sim_cycle < 200) {
    switch (sim_cycle) {
      case 1:
        tx_eng_to_mem_cmd_in.write(MemBufferRWCmd(0x12350000, 0xFF));
        break;
      case 2:
        tx_eng_to_mem_cmd_in.write(MemBufferRWCmd(0x1233FFFF, 0xFF));

        break;
      case 3:
        tx_eng_to_mem_cmd_in.write(MemBufferRWCmd(0x1237FFFE, 0x10));
        break;
      case 4:
        tx_eng_to_mem_cmd_in.write(MemBufferRWCmd(0x1234FFF1, 0x100));
        break;
      default:
        break;
    }
    TxEngReadDataSendCmd(tx_eng_to_mem_cmd_in, mover_read_mem_cmd_out, mem_buffer_double_access);
    EmptyTxReadCmdFifos(outputFile, mem_buffer_double_access, mover_read_mem_cmd_out, sim_cycle);
    sim_cycle++;
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *input_tcp_pcap_file = argv[1];
  cout << "Read TCP Packets from " << input_tcp_pcap_file << endl;
  stream<NetAXIS> input_tcp_packets("input_tcp_packets");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_packets);

  // open output file
  TestTxWriteToMem(input_tcp_packets);
  TestTxEngReadCmd();

  return 0;
}
