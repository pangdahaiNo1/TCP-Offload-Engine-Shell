#ifndef _AXI_UTILS_TEST_HPP_
#define _AXI_UTILS_TEST_HPP_
#include "axi_utils.hpp"

#include <fstream>
#include <hls_stream.h>
#include <iostream>

using namespace std;
using hls::stream;

int LoadNetAXISFromFile(stream<NetAXIS> &net_axis_stream, const string &file_name) {
  std::ifstream axis_infile(file_name);
  if (!axis_infile.is_open()) {
    std::cout << "Error: could not open tcp input file." << std::endl;
    return -1;
  }
  NetAXIS one_data;
  string  one_data_data, one_data_id, one_data_user, one_data_dest, one_data_keep, one_data_last;
  while (axis_infile >> one_data_data >> one_data_id >> one_data_user >> one_data_dest >>
         one_data_keep >> one_data_last) {
    one_data.data = NetAXISData(one_data_data.c_str(), 16);
    one_data.id   = NetAXISId(one_data_id.c_str(), 16);
    one_data.user = NetAXISUser(one_data_user.c_str(), 16);
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
    axis_outfile << hex;
    axis_outfile << one_data.data;
    axis_outfile << " ";
    axis_outfile << one_data.id;
    axis_outfile << " ";
    axis_outfile << one_data.user;
    axis_outfile << " ";
    axis_outfile << one_data.dest;
    axis_outfile << " ";
    axis_outfile << one_data.keep;
    axis_outfile << " ";
    axis_outfile << one_data.last;
    axis_outfile << " ";
    axis_outfile << endl;
  }
  axis_outfile.close();
}

#endif