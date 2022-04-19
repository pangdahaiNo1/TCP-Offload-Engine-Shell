#ifndef _PCAP2STREAM_H_
#define _PCAP2STREAM_H_

#include "utils/axi_utils.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

using hls::stream;

void PcapToStream(char *file2load,                    // pcapfilename
                  bool  remove_file_ethernet_header,  // 0: No ethernet in the packet, 1:
                                                      // ethernet include
                  stream<NetAXIS> &output_data        // output data
);

void PcapToStreamStep(char *file2load,               // pcapfilename
                      bool  remove_ethernet_header,  // 0: No ethernet in the packet, 1: ethernet
                                                     // include
                      bool &           end_of_data,
                      stream<NetAXIS> &output_data  // output data
);

unsigned KeepToLength(ap_uint<NET_TDATA_WIDTH / 8> keep);

int StreamToPcap(char *file2save,                      // pcapfilename
                 bool  insert_stream_ethernet_header,  // 0: No ethernet in the packet, 1:
                                                       // ethernet include
                 bool using_microseconds_precision,    // 1: microseconds precision 0:
                                                       // nanoseconds precision
                 stream<NetAXIS> &input_data,          // output data
                 bool             close_file);

#endif