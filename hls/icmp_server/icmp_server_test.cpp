#include "icmp_server.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace hls;
using namespace std;

int main(int argc, char **argv) {
  stream<NetAXIS> ipRxData("ipRxData");
  stream<NetAXIS> ipTxData("ipTxData");
  stream<NetAXIS> goldenData("goldenData");
  ap_uint<32>     myIpAddress = 0x0500a8c0;

  char *input_file;
  char *golden_input;

  if (argc < 3) {
    cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE> <GOLDEN_PCAP_FILE> "
         << endl;
    ;
    return -1;
  }

  input_file                   = argv[1];
  golden_input                 = argv[2];
  string icmp_in_axis_file     = "icmp_input.dat";
  string icmp_out_axis_file    = "icmp_output.dat";
  string icmp_golden_axis_file = "icmp_golden.dat";

  PcapToStream(input_file, true, ipRxData);
  SaveNetAXISToFile(ipRxData, icmp_in_axis_file);

  PcapToStream(input_file, true, ipRxData);

  for (int m = 0; m < 50000; m++) {
    icmp_server(ipRxData, myIpAddress, ipTxData);
  }

  PcapToStream(golden_input, true, goldenData);

  int error_packets = 0;
  error_packets     = ComparePacpPacketsWithGolden(ipTxData, goldenData, true);
  cout << "Error packets " << dec << error_packets << endl;

  return 0;
}
