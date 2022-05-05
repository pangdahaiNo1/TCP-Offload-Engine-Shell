#include "arp_server.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace std;
ArpTableEntry arpTable[256];

void initTable(void) {
  for (unsigned int m = 0; m < 255; m++) {
    arpTable[m] = ArpTableEntry(0, 0, 0);
  }
}

int main(int argc, char **argv) {
  // file
  char *outputPcap = (char *)"outputARP.pcap";
  char *input_file;
  char *golden_output;
  if (argc < 3) {
    cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE> <GOLDEN_PCAP_FILE> "
         << endl;
    return -1;
  }
  input_file    = argv[1];
  golden_output = argv[2];

  // Stream
  stream<NetAXIS>      arp_data_in("arp_data_in");
  stream<NetAXIS>      arp_data_out("arp_data_out");
  stream<NetAXIS>      arp_data_out_golden("arp_data_out_golden");
  stream<NetAXIS>      arp_data_out_to_save("arp_data_out_to_save");
  stream<ap_uint<32> > macIpEncode_req("macIpEncode_req");
  stream<ArpTableRsp>  macIpEncode_rsp("macIpEncode_rsp");
  // config
  ap_uint<1>  arp_scan        = 0;
  ap_uint<48> my_mac_addr     = SwapByte<48>(0x000A35029DE5);
  ap_uint<32> my_ip_addr      = SwapByte<32>(0xC0A80005);
  ap_uint<32> gateway_ip_addr = SwapByte<32>(0xC0A80001);
  ap_uint<32> subnet_mask     = SwapByte<32>(0xFFFFFF00);
  // read ARP request
  PcapToStream(input_file, false, arp_data_in);
  // read golden ARP response
  PcapToStream(golden_output, false, arp_data_out_golden);
  // Clear ARP table
  initTable();

  for (unsigned int i = 0; i < 5000; i++) {
    arp_server(arp_data_in,
               macIpEncode_req,
               arp_data_out,
               macIpEncode_rsp,
               arpTable,
               arp_scan,
               my_mac_addr,
               my_ip_addr,
               gateway_ip_addr,
               subnet_mask);
  }

  unsigned int outpkt = 0;
  while (!arp_data_out.empty()) {
    NetAXIS currWord;
    arp_data_out.read(currWord);
    arp_data_out_to_save.write(currWord);
    if (currWord.last)
      outpkt++;
  }

  std::cout << "There were a total of " << outpkt << " output ARP packets" << std::endl;
  /*Compare ARP out*/
  ComparePacpPacketsWithGolden(arp_data_out_to_save, arp_data_out_golden, true);

  return 0;
}