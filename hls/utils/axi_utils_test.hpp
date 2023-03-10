#ifndef _AXI_UTILS_TEST_HPP_
#define _AXI_UTILS_TEST_HPP_
#include "axi_utils.hpp"

#include <fstream>
#include <hls_stream.h>
#include <iostream>

using hls::stream;
using std::setw;
using std::string;

int LoadNetAXISFromFile(stream<NetAXIS> &net_axis_stream, const string &file_name) {
  std::ifstream axis_infile(file_name);
  if (!axis_infile.is_open()) {
    std::cout << "Error: could not open tcp input file." << std::endl;
    return -1;
  }
  NetAXIS one_data;
  string  one_data_data, one_data_id, one_data_user, one_data_dest, one_data_keep, one_data_last;
  while (axis_infile >> one_data_data >> one_data_dest >> one_data_keep >> one_data_last) {
    one_data.data = NetAXISData(one_data_data.c_str(), 16);
    // one_data.id   = NetAXISId(one_data_id.c_str(), 16);
    // one_data.user = NetAXISUser(one_data_user.c_str(), 16);
    one_data.dest = NetAXISDest(one_data_dest.c_str(), 16);
    one_data.keep = NetAXISKeep(one_data_keep.c_str(), 16);
    one_data.last = ap_uint<1>(one_data_last.c_str(), 16);

    net_axis_stream.write(one_data);
  }
  axis_infile.close();
  return 0;
}

void SaveNetAXISToFile(stream<NetAXIS> &net_axis_stream, const string &file_name) {
  std::ofstream axis_outfile(file_name);
  while (!net_axis_stream.empty()) {
    NetAXIS one_data;
    net_axis_stream.read(one_data);
    axis_outfile << one_data.data.to_string(16);
    axis_outfile << " ";
    // axis_outfile << one_data.id.to_string(16);
    // axis_outfile << " ";
    // axis_outfile << one_data.user.to_string(16);
    // axis_outfile << " ";
    axis_outfile << one_data.dest.to_string(16);
    axis_outfile << " ";
    axis_outfile << one_data.keep.to_string(16);
    axis_outfile << " ";
    axis_outfile << one_data.last.to_string(16);
    axis_outfile << " ";
    axis_outfile << endl;
  }
  axis_outfile.close();
}

int ComparePacpPacketsWithGolden(stream<NetAXIS> &dut_packets,
                                 stream<NetAXIS> &golden_packets,
                                 bool             debug_all_bytes) {
  int  wordCount          = 0;
  int  packets            = 0;
  int  error_packets      = 0;
  bool error_packets_flag = false;
  while (!dut_packets.empty()) {
    NetAXIS curr_word;
    NetAXIS golden_word;
    dut_packets.read(curr_word);
    golden_packets.read(golden_word);
    int error = 0;

    for (int m = 0; m < 64; m++) {
      if (curr_word.data((m * 8) + 7, m * 8) != golden_word.data((m * 8) + 7, m * 8)) {
        if (debug_all_bytes) {
          cout << "Packet [" << dec << setw(4) << packets << "] Word [" << setw(3) << wordCount
               << " ]Byte [" << setw(2) << dec << m << "] does not match generated ";
          cout << hex << curr_word.data((m * 8) + 7, m * 8) << " expected "
               << golden_word.data((m * 8) + 7, m * 8) << endl;
        }
        error++;
      }
    }
    if (error != 0) {
      error_packets_flag = true;
      cout << "dutPacket : \t" << hex << curr_word.data << "\tkeep: " << curr_word.keep
           << "\tlast: " << dec << curr_word.last << endl;
      cout << "goldenPacket : \t" << hex << golden_word.data << "\tkeep: " << golden_word.keep
           << "\tlast: " << dec << golden_word.last << endl;
    }
    // return -1;
    if (curr_word.keep != golden_word.keep) {
      cout << "keep does not match generated " << hex << setw(18) << curr_word.keep << " expected "
           << setw(18) << golden_word.keep << endl;
      error_packets_flag = true;
    }
    if (curr_word.last != golden_word.last) {
      cout << "Error last does not match " << endl;
      error_packets_flag = true;
    }

    if (curr_word.last) {
      packets++;
      wordCount = 0;

      if (error_packets_flag) {
        error_packets++;
      }
      error_packets_flag = false;
    } else {
      wordCount++;
    }
  }

  cout << "Compared packets " << dec << packets << endl;
  return error_packets;
  ;
}

void NetAXIStreamToNetAXIStreamWord(stream<NetAXIS>     &net_axis_stream_in,
                                    stream<NetAXISWord> &net_axis_word_stream_out) {
  stream<NetAXISWord> ret_stream;
  while (!net_axis_stream_in.empty()) {
    net_axis_word_stream_out.write(net_axis_stream_in.read());
  }
}

void NetAXIStreamWordToNetAXIStream(stream<NetAXISWord> &net_axis_word_stream_in,
                                    stream<NetAXIS>     &net_axis_stream_out) {
  stream<NetAXISWord> ret_stream;
  while (!net_axis_word_stream_in.empty()) {
    net_axis_stream_out.write(net_axis_word_stream_in.read().to_net_axis());
  }
}

// a ugly util func, need to be refactored
void GenRandStream(uint64_t bytes_cnt, stream<NetAXIS> &rand_stream) {
  uint64_t last_beat_byte = bytes_cnt % NET_TDATA_BYTES;
  uint64_t beat_cnt       = bytes_cnt >> NET_TDATA_BYTES_LOG2;
  uint64_t cnt            = 0;
  NetAXIS  cur_beat;
  while (cnt < beat_cnt) {
    for (int i = 0; i < 64; i++) {
      cur_beat.data((i + 1) * 8 - 1, i * 8) = 0xA0 + i;
    }
    cur_beat.keep = -1;
    cur_beat.last = 0;

    cnt++;
    if (cnt == beat_cnt && last_beat_byte == 0) {
      cur_beat.last = 1;
      rand_stream.write(cur_beat);
      return;
    }
    rand_stream.write(cur_beat);
  }

  cur_beat.data = 0;

  for (int i = 0; i < last_beat_byte; i++) {
    cur_beat.data((i + 1) * 8 - 1, i * 8) = 0xA0 + i;
  }
  cur_beat.keep = (1UL << last_beat_byte) - 1;
  cur_beat.last = 1;
  rand_stream.write(cur_beat);
}

#endif