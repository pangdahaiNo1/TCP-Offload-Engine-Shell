
#include "ethernet_header_inserter.hpp"
#include "icmp_server/icmp_server.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  ap_uint<48> intel_nic_mac_addr = SwapByte<48>(0x90E2BA847D6C);
  ap_uint<48> my_mac_addr        = SwapByte<48>(0x000A35029DE5);
  ap_uint<32> my_ip_addr         = SwapByte<32>(0xC0A80005);
  ap_uint<32> gateway_ip_addr    = SwapByte<32>(0xC0A80001);
  ap_uint<32> subnet_mask        = SwapByte<32>(0xFFFFFF00);

  stream<NetAXIS> icmp_rx_data("icmp_rx_data");
  stream<NetAXIS> icmp_tx_data("icmp_tx_data");
  stream<NetAXIS> eth_icmp_tx_data("eth_icmp_tx_data");
  stream<NetAXIS> eth_icmp_tx_golden_data("eth_icmp_tx_golden_data");

  char *icmp_rx_file;
  char *icmp_tx_golden_file;
  if (argc < 3) {
    cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE> <GOLDEN_PCAP_FILE> "
         << endl;
    ;
    return -1;
  }
  icmp_rx_file        = argv[1];
  icmp_tx_golden_file = argv[2];
  PcapToStream(icmp_rx_file, true, icmp_rx_data);
  for (int m = 0; m < 50000; m++) {
    icmp_server(icmp_rx_data, my_ip_addr, icmp_tx_data);
  }
  PcapToStream(icmp_tx_golden_file, false, eth_icmp_tx_golden_data);

  stream<ArpTableRsp>  arp_table_rsp;
  stream<ap_uint<32> > arp_table_req;
  for (int i = 0; i < 500000; i++) {
    ethernet_header_inserter(icmp_tx_data,
                             eth_icmp_tx_data,
                             arp_table_rsp,
                             arp_table_req,
                             my_mac_addr,
                             subnet_mask,
                             gateway_ip_addr);
    while (!arp_table_req.empty()) {
      ap_uint<32> req_ip = arp_table_req.read();
      // cout << std::hex << req_ip << endl;
      arp_table_rsp.write(ArpTableRsp(intel_nic_mac_addr, 1));
    }
  }
  ComparePacpPacketsWithGolden(eth_icmp_tx_data, eth_icmp_tx_golden_data, true);
  //SaveNetAXISToFile(eth_icmp_tx_data, "eth_icmp_tx_data.dat");
  //SaveNetAXISToFile(eth_icmp_tx_golden_data, "eth_icmp_tx_golden_data.dat");
  return 0;
}
