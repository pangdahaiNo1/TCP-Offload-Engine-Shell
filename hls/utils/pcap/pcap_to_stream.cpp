#include "pcap_to_stream.hpp"

#include <stdio.h>
#include <stdlib.h>

using std::cout;
using std::endl;
stream<NetAXIS> input_data("data_read_from_pcap");
stream<NetAXIS> packet_without_ethernet("packet_without_ethernet");
NetAXIS         transaction;

void PcapPacketHandler(unsigned char *     user_data,
                       struct pcap_pkthdr *packet_header,
                       unsigned char *     packet) {
  ap_uint<16>                  bytes_sent = 0, bytes_to_send, last_trans;
  ap_uint<NET_TDATA_WIDTH / 8> aux_keep;
  ap_uint<NET_TDATA_WIDTH>     data_value;
  static ap_uint<16>           pkt = 0;

  for (int h = 0; h < NET_TDATA_WIDTH / 8; h++) {
    transaction.keep[h] = 1;
  }
  transaction.last = 0;

  bytes_to_send = (packet_header->len);
#ifdef DEBUG
#ifdef DEBUG_PCAP
  cout << "Bytes to send " << bytes_to_send << endl;
#endif
#endif
  for (int h = 0; h < bytes_to_send; h += NET_TDATA_WIDTH / 8) {
    bytes_sent += NET_TDATA_WIDTH / 8;
    if (bytes_sent < bytes_to_send) {
      data_value = *((ap_uint<NET_TDATA_WIDTH> *)packet);
      packet += NET_TDATA_WIDTH / 8;
    } else if (bytes_sent < (bytes_to_send + NET_TDATA_WIDTH / 8)) {
      last_trans = NET_TDATA_WIDTH / 8 - bytes_sent + bytes_to_send;

      for (int s = 0; s < NET_TDATA_WIDTH / 8; s++) {
        if (s < last_trans)
          aux_keep[s] = 1;
        else
          aux_keep[s] = 0;
      }
      transaction.last = 1;
      transaction.keep = aux_keep;
      data_value       = 0;
      for (int s = 0; s < last_trans * 8; s += 8) {
        data_value.range(s + 7, s) = *((ap_uint<8> *)packet);
        packet++;
      }
    }
#ifdef CHANGE_ENDIANESS
    for (int s = 0; s < NET_TDATA_WIDTH; s += 8) {
      transaction.data.range(NET_TDATA_WIDTH - 1 - s, NET_TDATA_WIDTH - s - 8) =
          data_value.range(s + 7, s);
    }
#else
    transaction.data = data_value;
#endif
#ifdef DEBUG
#ifdef DEBUG_PCAP
    cout << "Test Data [" << dec << pkt << "] Transaction [" << dec << h / (NET_TDATA_WIDTH / 8)
         << "]";
    cout << "\tComplete Data " << hex << transaction.data << "\t\tkeep " << transaction.keep
         << "\tlast " << transaction.last << endl;
    pkt = pkt + transaction.last;
#endif
#endif

    input_data.write(transaction);
  }
}

void RemoveEthernet(stream<NetAXIS> &data_in, stream<NetAXIS> &data_out) {
  NetAXIS curr_word;
  NetAXIS prev_word;
  NetAXIS send_word;

  static int pkt_count  = 0;
  int        word_count = 0;

  bool first_word = true;

  do {
    data_in.read(curr_word);
    // cout << "ETH In[" << dec << pkt_count << "] " << hex << curr_word.data <<
    // "\tkeep: " << curr_word.keep <<
    // "\tlast: " << dec << curr_word.last << endl;

    if (first_word) {
      first_word = false;
      if (curr_word.last) {
        send_word.data(399, 0) = curr_word.data(511, 112);
        send_word.keep(49, 0)  = curr_word.keep(63, 14);
        send_word.last         = 1;
        data_out.write(send_word);
        // cout << "ETH Short[" << dec << pkt_count << "] " << hex <<
        // send_word.data << "\tkeep: " << send_word.keep << "\tlast: " << dec <<
        // send_word.last << endl;
      }
    } else {
      send_word.data(399, 0)   = prev_word.data(511, 112);
      send_word.keep(49, 0)    = prev_word.keep(63, 14);
      send_word.data(511, 400) = curr_word.data(111, 0);
      send_word.keep(63, 50)   = curr_word.keep(13, 0);
      send_word.last           = 0;
      if (curr_word.last) {
        if (curr_word.keep.bit(14)) {
          prev_word = curr_word;
          data_out.write(send_word);
          // cout << "ETH Extra1[" << dec << pkt_count << "] " << hex <<
          // send_word.data << "\tkeep: " << send_word.keep << "\tlast: " << dec
          // << send_word.last << endl;
          send_word.data         = 0;
          send_word.keep         = 0;
          send_word.last         = 1;
          send_word.data(399, 0) = prev_word.data(511, 112);
          send_word.keep(49, 0)  = prev_word.keep(63, 14);
          data_out.write(send_word);
          // cout << "ETH Extra2[" << dec << pkt_count << "] " << hex <<
          // send_word.data << "\tkeep: " << send_word.keep << "\tlast: " << dec
          // << send_word.last << endl;

        } else {
          send_word.last = 1;
          data_out.write(send_word);
          // cout << "ETH Last[" << dec << pkt_count << "] " << hex <<
          // send_word.data << "\tkeep: " << send_word.keep << "\tlast: " << dec
          // << send_word.last << endl;
        }
      } else {
        data_out.write(send_word);
        // cout << "ETH Normal[" << dec << pkt_count << "] " << hex <<
        // send_word.data << "\tkeep: " << send_word.keep << "\tlast: " << dec <<
        // send_word.last << endl;
      }
    }

    prev_word = curr_word;

  } while (!curr_word.last);
  pkt_count++;
}

int OpenFile(char *file_to_load, bool remove_file_ethernet_header) {
  // cout << "File 2 load " << file_to_load << endl;

  /* Read packets from pcapfile */
  if (PcapOpen(file_to_load, remove_file_ethernet_header)) {
    cout << "Error opening the input file with name: " << file_to_load << endl;
    return -1;
  }
  // else
  //	cout <<"File open correct" << endl << endl;

  if (PcapLoop(0,                 /* Iterate over all the packets */
               PcapPacketHandler, /* Routine invoked */
               NULL) < 0) {       /* Argument passed to the handler function (the buffer) */

    PcapClose();
    cout << "Error opening the input file" << endl;
    return -1;
  }

  PcapClose();
  return 0;
}

void PcapToStream(char *file_to_load,                 // pcapfilename
                  bool  remove_file_ethernet_header,  // 0: No ethernet in the packet, 1:
                                                      // ethernet include
                  stream<NetAXIS> &output_data        // output data
) {
  NetAXIS curr_word;
  cout << "Read TCP Packets from " << file_to_load << endl;

  if (OpenFile(file_to_load, remove_file_ethernet_header) == 0) {
    while (!input_data.empty()) {
      input_data.read(curr_word);
      output_data.write(curr_word);
    }
  }
}

/* It returns one complete packet each time is called
 *
 */

void PcapToStreamStep(char *file_to_load,                 // pcapfilename
                      bool  remove_file_ethernet_header,  // 0: No ethernet in the packet, 1:
                                                          // ethernet include
                      bool &           end_of_data,
                      stream<NetAXIS> &output_data  // output data
) {
  static bool error_opening_file = false;
  static bool file_open          = false;

  stream<NetAXIS> currWord_Stream("data_to_remove_ethernet");

  NetAXIS curr_word;

  if (!file_open) {
    file_open = true;
    if (OpenFile(file_to_load, remove_file_ethernet_header) != 0) {
      error_opening_file = true;
    }
  }

  if (!error_opening_file) {
    if (!input_data.empty()) {
      do {
        input_data.read(curr_word);
        output_data.write(curr_word);
      } while (!curr_word.last);
      end_of_data = false;
    } else {
      end_of_data = true;
    }
  }
}

unsigned KeepToLength(ap_uint<NET_TDATA_WIDTH / 8> keep) {
  unsigned ones_count = 0;

  for (int i = 0; i < NET_TDATA_WIDTH / 8; i++) {
    if (keep.bit(i)) {
      ones_count++;
    } else
      break;
  }

  return ones_count;
}

int StreamToPcap(const char *file2save,                      // pcapfilename
                 bool        insert_stream_ethernet_header,  // 0: No ethernet in the packet, 1:
                                                             // ethernet include
                 bool using_microseconds_precision,          // 1: microseconds precision 0:
                                                             // nanoseconds precision
                 stream<NetAXIS> &input_data,                // output data
                 bool             close_file) {
  bool file_open              = false;
  int  pcap_open_write_return = 0;

  NetAXIS curr_word;
  uint8_t ethernet_header[14] = {
      0x90, 0xE2, 0xBA, 0x84, 0x7D, 0x6C, 0x0, 0x0A, 0x35, 0x02, 0x9D, 0xE5, 0x08, 0x00};
  uint8_t packet[65536] = {0};  // Include the Ethernet header
  int     pointer       = 0;

  if (insert_stream_ethernet_header) {
    memcpy(packet, ethernet_header, sizeof(ethernet_header));
    pointer = 14;
  } else {
    pointer = 0;
  }

  if (!file_open) {
    pcap_open_write_return = PcapOpenWrite(file2save, using_microseconds_precision);
    file_open              = true;
  }

  if (pcap_open_write_return != 0) {
    return -1;
  }

  while (!input_data.empty()) {
    input_data.read(curr_word);
#ifdef DEBUG
#ifdef DEBUG_PCAP
    cout << "Stream to pcap: " << hex << curr_word.data << "\tkeep: " << curr_word.keep
         << "\tlast: " << dec << curr_word.last << endl;
#endif  // DEBUG_PCAP
#endif  // DEBUG
    for (int i = 0; i < 64; i++) {
      if (curr_word.keep.bit(i)) {
        packet[pointer] = curr_word.data(i * 8 + 7, i * 8);
        pointer++;
      }
    }
    if (curr_word.last) {
      // call write function
      PcapWriteData(&packet[0], pointer);
      pointer = (insert_stream_ethernet_header) ? 14 : 0;
    }
  }

  if (close_file) {
    PcapCloseWrite();
    file_open = false;
  }

  return 0;
}
