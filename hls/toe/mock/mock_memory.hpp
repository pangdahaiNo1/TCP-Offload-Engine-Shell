#ifndef _MOCK_MEMORY_HPP_
#define _MOCK_MEMORY_HPP_
#include "toe/mock/mock_logger.hpp"
#include "toe/toe_conn.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <vector>
using hls::stream;
using std::queue;
using std::to_string;
using std::unordered_map;
using std::vector;

// little endian memory
class MockMem {
public:
  // key = 40bit addr, val = mem val
  unordered_map<uint64_t, ap_uint<8> > _mock_memory;
  // write cmd fifo
  queue<DataMoverCmd> _s2mm_cmd_fifo;

  MockMem() {
    _mock_memory   = unordered_map<uint64_t, ap_uint<8> >();
    _s2mm_cmd_fifo = queue<DataMoverCmd>();
  }
  void MockMemIntf(MockLogger &          logger,
                   stream<DataMoverCmd> &mover_read_mem_cmd,
                   stream<NetAXIS> &     mover_read_mem_data,

                   stream<DataMoverCmd> &   mover_write_mem_cmd,
                   stream<NetAXIS> &        mover_write_mem_data,
                   stream<DataMoverStatus> &mover_write_mem_status) {
    DataMoverCmd    cur_cmd;
    NetAXISWord     cur_word;
    DataMoverStatus cur_sts;
    // S2MM-Write data to mem
    if (!mover_write_mem_cmd.empty()) {
      cur_cmd = mover_write_mem_cmd.read();

      logger.Info(TOE_TOP, DATA_MVER, "WriteMem Cmd", cur_cmd.to_string());
      _s2mm_cmd_fifo.push(cur_cmd);
    }
    if (!mover_write_mem_data.empty()) {
      cur_word = mover_write_mem_data.read();
      S2MMWriteToMem(cur_word.to_net_axis());
      cur_sts.okay = 1;
      mover_write_mem_status.write(cur_sts);
      logger.Info(TOE_TOP, DATA_MVER, "WriteMem data", cur_word.to_string());
    }
    // MM2S - read data from mem
    if (!mover_read_mem_cmd.empty()) {
      cur_cmd = mover_read_mem_cmd.read();
      MM2SReadFromMem(cur_cmd, mover_read_mem_data);
      logger.Info(TOE_TOP, DATA_MVER, "ReadMem Cmd", cur_cmd.to_string());
    }
  }
  void PushS2MMCmd(const DataMoverCmd &cmd) { _s2mm_cmd_fifo.push(cmd); }

  // assume the head cmd.bbt is larger than one_beat data length
  // ignore the one_beat last signal
  void S2MMWriteToMem(const NetAXIS &one_beat) {
    assert(!_s2mm_cmd_fifo.empty());
    DataMoverCmd   front_cmd        = _s2mm_cmd_fifo.front();
    const uint16_t s2mm_data_length = KeepToLength(one_beat.keep);
    // cout << s2mm_data_length << " " << one_beat.keep.to_string(16) << endl;
    uint64_t cur_addr        = front_cmd.saddr.to_uint64();
    uint64_t remaining_bytes = front_cmd.bbt;
    for (int i = 0; i < s2mm_data_length; i++) {
      _mock_memory[cur_addr] = one_beat.data((i + 1) * 8 - 1, i * 8);
      cur_addr += 1;
      remaining_bytes -= 1;
    }
    assert(remaining_bytes >= 0);
    if (remaining_bytes == 0) {
      _s2mm_cmd_fifo.pop();
    } else {
      _s2mm_cmd_fifo.front().bbt   = remaining_bytes;
      _s2mm_cmd_fifo.front().saddr = cur_addr;
    }
    return;
  }

  void VectorToAxiStream(const vector<ap_uint<8> > &read_vals, stream<NetAXIS> &mem_data) {
    int     bytes_to_trans = read_vals.size();
    NetAXIS cur_beat       = NewNetAXIS(0, 0, 0, 0);

    for (int i = 0; i < bytes_to_trans; i++) {
      uint16_t cur_beat_byte                                          = i % 64;
      cur_beat.data((cur_beat_byte + 1) * 8 - 1, (cur_beat_byte * 8)) = read_vals[i];
      cur_beat.keep.set_bit(cur_beat_byte, 1);
      if (i == (bytes_to_trans - 1)) {
        cur_beat.last = 1;
      }
      if (cur_beat_byte == 63 || i == (bytes_to_trans - 1)) {
        mem_data.write(cur_beat);
        cur_beat = NewNetAXIS(0, 0, 0, 0);
      }
    }
    return;
  }

  void MM2SReadFromMem(const DataMoverCmd &cmd, stream<NetAXIS> &mem_data) {
    uint64_t            bytes_to_trans = cmd.bbt;
    uint64_t            cur_addr       = cmd.saddr;
    vector<ap_uint<8> > read_vals      = vector<ap_uint<8> >();
    for (int i = 0; i < bytes_to_trans; i++) {
      auto iter = _mock_memory.find(cur_addr);
      if (iter == _mock_memory.end()) {
        std::cerr << "read a invalid mem" << hex << cur_addr << std::endl;
        exit(-1);
      } else {
        read_vals.push_back(iter->second);
      }
      cur_addr += 1;
    }
    VectorToAxiStream(read_vals, mem_data);
    return;
  }
  /**
   * save mem to file, the addr is sorted
   *
   * <40bit addr> <val>
   *
   */
  void DumpMockMemoryToFile(string file_name) {
    // sort addr
    std::vector<uint64_t> keys;

    keys.reserve(_mock_memory.size());
    for (auto &it : _mock_memory) {
      keys.push_back(it.first);
    }
    std::sort(keys.begin(), keys.end());
    // write to sting stream
    std::stringstream sstream;
    uint64_t          prev_addr      = 0;
    uint64_t          one_line_bytes = 8;
    for (int i = 0; i < keys.size(); i++) {
      prev_addr = keys[i];
      sstream << "0x" << std::uppercase << std::setfill('0') << std::setw(10) << std::hex << keys[i]
              << " " << std::uppercase << std::setfill('0') << std::setw(2) << std::hex
              << _mock_memory[keys[i]].to_ushort() << endl;
    }
    std::ofstream output_stream;
    output_stream.open(file_name);
    output_stream << sstream.str();
    output_stream.close();
    return;
  }

  int LoadMockMemoryFromFile(string file_name) {
    std::ifstream memfile(file_name);
    if (!memfile.is_open()) {
      std::cout << "Error: could not open tcp input file." << std::endl;
      return -1;
    }
    string cur_addr_str, cur_byte_str;
    while (memfile >> cur_addr_str >> cur_byte_str) {
      ap_uint<64> cur_addr               = ap_uint<64>(cur_addr_str.c_str(), 16);
      ap_uint<8>  cur_byte               = ap_uint<8>(cur_addr_str.c_str(), 16);
      _mock_memory[cur_addr.to_uint64()] = cur_byte;
    }
    memfile.close();
    return 0;
  }
};

#endif