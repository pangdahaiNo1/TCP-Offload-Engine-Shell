#include "icmp_server.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace hls;
using namespace std;

int main(int argc, char **argv) {
  stream<NetAXIS> ip_rx_data("ip_rx_data");
  stream<NetAXIS> ip_tx_data("ip_tx_data");
  stream<NetAXIS> ip_tx_data_golden("ip_tx_data_golden");
  ap_uint<32>     my_ip_addr = 0x0500a8c0;

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

  PcapToStream(input_file, true, ip_rx_data);
  SaveNetAXISToFile(ip_rx_data, icmp_in_axis_file);

  PcapToStream(input_file, true, ip_rx_data);
  for (int m = 0; m < 500; m++) {
    icmp_server(ip_rx_data, my_ip_addr, ip_tx_data);
  }

  PcapToStream(golden_input, true, ip_tx_data_golden);

  int error_packets = 0;
  // it not contains the ipv4 checksum
  error_packets = ComparePacpPacketsWithGolden(ip_tx_data, ip_tx_data_golden, true);
  cout << "Error packets " << dec << error_packets << endl;

  return 0;
}
