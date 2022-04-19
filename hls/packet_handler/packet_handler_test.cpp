#include "packet_handler.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

using namespace hls;
using namespace std;

int ComparePacpPacketsWithGolden(stream<NetAXIS> &dut_packets, stream<NetAXIS> &golden_packets) {
  int wordCount     = 0;
  int packets       = 0;
  int error_packets = 0;
  while (!dut_packets.empty()) {
    NetAXIS curr_word;
    NetAXIS golden_word;
    dut_packets.read(curr_word);
    golden_packets.read(golden_word);
    int  error              = 0;
    bool error_packets_flag = false;

    for (int m = 0; m < 64; m++) {
      if (curr_word.data((m * 8) + 7, m * 8) != golden_word.data((m * 8) + 7, m * 8)) {
        error++;
      }
    }
    if (error != 0) {
      error_packets_flag = true;
      cout << "dutPacket : \t\t" << hex << curr_word.data << "\tkeep: " << curr_word.keep
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
    } else
      wordCount++;
  }

  cout << "Compared packets " << dec << packets << endl;
  return error_packets;
  ;
}

void AxiStreamSplitter(stream<NetAXIS> &output_ethernet_packets,
                       stream<NetAXIS> &arp_packets,
                       stream<NetAXIS> &icmp_packets,
                       stream<NetAXIS> &udp_packets,
                       stream<NetAXIS> &tcp_packets) {
  int packets_cnt      = 0;
  int arp_packets_cnt  = 0;
  int icmp_packets_cnt = 0;
  int udp_packets_cnt  = 0;
  int tcp_packets_cnt  = 0;
  while (!output_ethernet_packets.empty()) {
    NetAXIS curr_word;
    output_ethernet_packets.read(curr_word);
    NetAXIS curr_word_without_dest;
    curr_word_without_dest.data = curr_word.data;
    curr_word_without_dest.keep = curr_word.keep;
    curr_word_without_dest.last = curr_word.last;

    switch (curr_word.dest) {
      case 0:
        arp_packets.write(curr_word_without_dest);
        arp_packets_cnt++;
        break;
      case 1:
        icmp_packets.write(curr_word_without_dest);
        icmp_packets_cnt++;
        break;
      case 2:
        tcp_packets.write(curr_word_without_dest);
        tcp_packets_cnt++;
        break;
      case 3:
        udp_packets.write(curr_word_without_dest);
        udp_packets_cnt++;
        break;
    }
    if (curr_word.last == 1) {
      packets_cnt++;
    }
  }
  printf("Splited packets %d \nARP packets %d \n ICMP packets %d \nTCP packets "
         "%d\n UDP packets %d\n",
         packets_cnt,
         arp_packets_cnt,
         icmp_packets_cnt,
         tcp_packets_cnt,
         udp_packets_cnt);
  return;
}

int main(int argc, char **argv) {
  stream<NetAXIS> input_ethernet_packets("input_ethernet_packets");
  stream<NetAXIS> input_golden_arp_packets("input_golden_arp_packets");
  stream<NetAXIS> input_golden_icmp_packets("input_golden_icmp_packets");
  stream<NetAXIS> input_golden_tcp_packets("input_golden_tcp_packets");
  stream<NetAXIS> input_golden_udp_packets("input_golden_udp_packets");

  char *input_file;
  char *golden_input_file_arp;
  char *golden_input_file_icmp;
  char *golden_input_file_tcp;
  char *golden_input_file_udp;
  char *output_file_prefix;

  int count = 0;

  if (argc < 7) {
    cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>*5 +  <OUTPUT_PCAP_FILE> "
         << endl;
    ;
    return -1;
  }

  input_file                     = argv[1];
  golden_input_file_arp          = argv[2];
  golden_input_file_icmp         = argv[3];
  golden_input_file_tcp          = argv[4];
  golden_input_file_udp          = argv[5];
  output_file_prefix             = argv[6];
  char *output_file_arp_packets  = NULL;
  char *output_file_icmp_packets = NULL;
  char *output_file_tcp_packets  = NULL;
  char *output_file_udp_packets  = NULL;
  asprintf(&output_file_arp_packets, "%s%s", output_file_prefix, "_arp.pcap");
  asprintf(&output_file_icmp_packets, "%s%s", output_file_prefix, "_icmp.pcap");
  asprintf(&output_file_tcp_packets, "%s%s", output_file_prefix, "_tcp.pcap");
  asprintf(&output_file_udp_packets, "%s%s", output_file_prefix, "_udp.pcap");

  cout << "Read Ethernet Packets" << endl;
  PcapToStream(input_file, false, input_ethernet_packets);
  cout << "Read ARP Packets" << endl;
  PcapToStream(golden_input_file_arp, false, input_golden_arp_packets);
  cout << "Read ICMP Packets" << endl;
  PcapToStream(golden_input_file_icmp, true, input_golden_icmp_packets);
  cout << "Read TCP Packets" << endl;
  PcapToStream(golden_input_file_tcp, true, input_golden_tcp_packets);
  cout << "Read UDP Packets" << endl;
  PcapToStream(golden_input_file_udp, true, input_golden_udp_packets);

  stream<NetAXIS> dut_output_ethernet_packets("dut_output_ethernet_packets");
  stream<NetAXIS> dut_output_arp_packets("dut_output_arp_packets");
  stream<NetAXIS> dut_output_icmp_packets("dut_output_icmp_packets");
  stream<NetAXIS> dut_output_udp_packets("dut_output_udp_packets");
  stream<NetAXIS> dut_output_tcp_packets("dut_output_tcp_packets");

  for (int i = 0; i < 10000; i++) {
    PacketHandler(input_ethernet_packets, dut_output_ethernet_packets);
  }

  AxiStreamSplitter(dut_output_ethernet_packets,
                    dut_output_arp_packets,
                    dut_output_icmp_packets,
                    dut_output_udp_packets,
                    dut_output_tcp_packets);
  int error_packets = 0;
  error_packets += ComparePacpPacketsWithGolden(dut_output_arp_packets, input_golden_arp_packets);
  error_packets += ComparePacpPacketsWithGolden(dut_output_icmp_packets, input_golden_icmp_packets);
  error_packets += ComparePacpPacketsWithGolden(dut_output_udp_packets, input_golden_udp_packets);
  error_packets += ComparePacpPacketsWithGolden(dut_output_tcp_packets, input_golden_tcp_packets);

  cout << "Error packets " << dec << error_packets << endl;
  return error_packets;
}
