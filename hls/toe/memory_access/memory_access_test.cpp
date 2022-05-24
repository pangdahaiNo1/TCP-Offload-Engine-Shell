
#include "memory_access.hpp"
#include "toe/mock/mock_memory.hpp"
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

  stream<NetAXISWord>  tx_app_to_mem_data_in;
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

void TestMockMem() {
  MockMem mem = MockMem();

  for (int i = 0; i < 128; i++) {
    uint64_t cur_addr          = 0x4812345600 + i;
    mem._mock_memory[cur_addr] = i;
  }
  stream<NetAXIS> read_mem_data("mem_data");
  // read from mem
  mem.MM2SReadFromMem(DataMoverCmd(0x12345600, 128), read_mem_data);
  // write to mem, new addr
  mem.PushS2MMCmd(DataMoverCmd(0x12345A00, 128));
  mem.S2MMWriteToMem(read_mem_data.read());
  mem.S2MMWriteToMem(read_mem_data.read());
  // save current mem
  std::ofstream outputFile;
  outputFile.open("./test_mem_dump_mem.dat");
  outputFile << mem.DumpMockMemory();
  outputFile.close();

  // clear mem for testing load
  mem._mock_memory.clear();
  mem.LoadMockMemoryFromFile("./test_mem_dump_mem.dat");

  outputFile.open("./test_mem_dump_mem_after_load.dat");
  outputFile << mem.DumpMockMemory();
  outputFile.close();

  // cout << mem.DumpMockMemory() << endl;
}

void TestTxMem(stream<NetAXIS> &input_golden_packets, stream<NetAXISWord> &read_mem_packets) {
  // mock mem
  MockMem mock_mem = MockMem();

  // tx app
  stream<NetAXISWord>  tx_app_to_mem_data_in("tx_app_to_mem_data_in");
  stream<DataMoverCmd> tx_app_to_mem_cmd_in("tx_app_to_mem_cmd_in");
  stream<NetAXIS>      mover_mem_data_out("mover_mem_data_out");
  stream<DataMoverCmd> mover_mem_cmd_out("mover_mem_cmd_out");
  int                  sim_cycle = 0;
  // fifo
  stream<MemBufferRWCmd> mem_cmd_fifo("mem_cmd_fifo");

  NetAXIS      cur_word{};
  TcpSessionBuffer cur_word_length = 0;
  ap_uint<32>      saddr           = 0;
  while (sim_cycle < 200) {
    switch (sim_cycle % 20) {
      case 1:
        // tx app write data to mem
        cur_word_length = 0;
        while (!input_golden_packets.empty()) {
          input_golden_packets.read(cur_word);
          tx_app_to_mem_data_in.write(cur_word);
          cur_word_length += KeepToLength(cur_word.keep);
          if (cur_word.last == 1) {
            break;
          }
        }
        if (cur_word_length != 0) {
          DataMoverCmd cur_cmd = DataMoverCmd(saddr, cur_word_length);
          tx_app_to_mem_cmd_in.write(cur_cmd);
          mem_cmd_fifo.write(MemBufferRWCmd(saddr, cur_word_length));
          saddr += cur_word_length;
        }
        break;
      default:
        break;
    }
    TxAppWriteDataToMem(
        tx_app_to_mem_data_in, tx_app_to_mem_cmd_in, mover_mem_data_out, mover_mem_cmd_out);
    while (!mover_mem_cmd_out.empty()) {
      mock_mem.PushS2MMCmd(mover_mem_cmd_out.read());
    }
    while (!mover_mem_data_out.empty()) {
      mock_mem.S2MMWriteToMem(mover_mem_data_out.read());
    }
    sim_cycle++;
  }
  // save current mem
  std::ofstream outputFile;
  outputFile.open("./dump_mem.dat");
  outputFile << mock_mem.DumpMockMemory();

  // tx engine read data from mem
  stream<MemBufferRWCmd>             tx_eng_to_mem_cmd_in("tx_eng_to_mem_cmd_in");
  stream<DataMoverCmd>               mover_read_mem_cmd_out("mover_read_mem_cmd_out");
  stream<MemBufferRWCmdDoubleAccess> mem_buffer_double_access("mem_buffer_double_access");

  stream<NetAXIS> mover_read_mem_data_in("mover_read_mem_data_in");
  stream<NetAXIS> to_tx_eng_read_data("to_tx_eng_read_data");
  sim_cycle = 0;
  while (sim_cycle < 30) {
    switch (sim_cycle % 5) {
      case 1:
        if (!mem_cmd_fifo.empty()) {
          MemBufferRWCmd cur_cmd = mem_cmd_fifo.read();
          cout << cur_cmd.addr.to_string(16) << " " << cur_cmd.length.to_string(16) << endl;
          tx_eng_to_mem_cmd_in.write(cur_cmd);
        }
        break;

      default:
        break;
    }

    TxEngReadDataSendCmd(tx_eng_to_mem_cmd_in, mover_read_mem_cmd_out, mem_buffer_double_access);
    while (!mover_read_mem_cmd_out.empty()) {
      NetAXIS cur_trans;
      mock_mem.MM2SReadFromMem(mover_read_mem_cmd_out.read(), mover_read_mem_data_in);
    }
    TxEngReadDataFromMem(mover_read_mem_data_in, mem_buffer_double_access, read_mem_packets);
    sim_cycle++;
  }

  // cout << mock_mem.DumpMockMemory() << endl;
  // cout << saddr.to_string(16) << endl;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *input_tcp_pcap_file = argv[1];
  cout << "Read TCP Packets from " << input_tcp_pcap_file << endl;
  stream<NetAXIS> input_tcp_packets("input_tcp_packets");
  stream<NetAXIS> input_tcp_packets2("input_tcp_packets2");
  stream<NetAXIS> input_tcp_packets3("input_tcp_packets3");
  stream<NetAXISWord> read_mem_tcp_packets("read_mem_tcp_packets");
  stream<NetAXIS> read_mem_tcp_packets_for_compare("read_mem_tcp_packets");
  PcapToStream(input_tcp_pcap_file, true, input_tcp_packets);
  // tcp header + ip header + eth header = 74B
  PcapToStream(input_tcp_pcap_file, false, input_tcp_packets2);
  PcapToStream(input_tcp_pcap_file, false, input_tcp_packets3);

  // open output file
  TestTxWriteToMem(input_tcp_packets);
  TestTxEngReadCmd();
  TestMockMem();
  TestTxMem(input_tcp_packets2, read_mem_tcp_packets);
  NetAXIStreamWordToNetAXIStream(read_mem_tcp_packets, read_mem_tcp_packets_for_compare);
  ComparePacpPacketsWithGolden(input_tcp_packets3, read_mem_tcp_packets_for_compare, true);

  return 0;
}
