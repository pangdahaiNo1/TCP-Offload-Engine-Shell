#include "axi_utils.hpp"

void                 ComputeSubChecksum(stream<NetAXIS> &    pkt_in,
                                        stream<NetAXIS> &    pkt_out,
                                        stream<SubChecksum> &sub_checksum) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off
  static SubChecksum tcp_checksums;
  if (!pkt_in.empty()) {
    NetAXIS cur_word = pkt_in.read();
    pkt_out.write(cur_word);
    for (int i = 0; i < NET_TDATA_WIDTH / 16; i++) {
#pragma HLS UNROLL
      ap_uint<16> temp;
      if (cur_word.keep(i * 2 + 1, i * 2) == 0x3) {
        temp(7, 0)  = cur_word.data(i * 16 + 15, i * 16 + 8);
        temp(15, 8) = cur_word.data(i * 16 + 7, i * 16);
        tcp_checksums.sum[i] += temp;
        tcp_checksums.sum[i] = (tcp_checksums.sum[i] + (tcp_checksums.sum[i] >> 16)) & 0xFFFF;
      } else if (cur_word.keep[i * 2] == 0x1) {
        temp(7, 0)  = 0;
        temp(15, 8) = cur_word.data(i * 16 + 7, i * 16);
        tcp_checksums.sum[i] += temp;
        tcp_checksums.sum[i] = (tcp_checksums.sum[i] + (tcp_checksums.sum[i] >> 16)) & 0xFFFF;
      }
    }
    if (cur_word.last == 1) {
      sub_checksum.write(tcp_checksums);
      for (int i = 0; i < NET_TDATA_WIDTH / 16; i++) {
#pragma HLS UNROLL
        tcp_checksums.sum[i] = 0;
      }
    }
  }
}

void                 CheckChecksum(stream<SubChecksum> &sub_checksum, stream<bool> &valid_pkt_out) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  if (!sub_checksum.empty()) {
    SubChecksum subsums = sub_checksum.read();
    // cout << hex << subsums.to_string() << endl;

    for (int i = 0; i < 16; i++) {
#pragma HLS unroll
      subsums.sum[i] += subsums.sum[i + 16];
      subsums.sum[i] = (subsums.sum[i] + (subsums.sum[i] >> 16)) & 0xFFFF;
    }

    for (int i = 0; i < 8; i++) {
#pragma HLS unroll
      subsums.sum[i] += subsums.sum[i + 8];
      subsums.sum[i] = (subsums.sum[i] + (subsums.sum[i] >> 16)) & 0xFFFF;
    }

    for (int i = 0; i < 4; i++) {
#pragma HLS unroll
      subsums.sum[i] += subsums.sum[i + 4];
      subsums.sum[i] = (subsums.sum[i] + (subsums.sum[i] >> 16)) & 0xFFFF;
    }

    // N >= 4 -> 64bit data
    subsums.sum[0] += subsums.sum[2];
    subsums.sum[1] += subsums.sum[3];
    subsums.sum[0] = (subsums.sum[0] + (subsums.sum[0] >> 16)) & 0xFFFF;
    subsums.sum[1] = (subsums.sum[1] + (subsums.sum[1] >> 16)) & 0xFFFF;
    subsums.sum[0] += subsums.sum[1];
    subsums.sum[0] = (subsums.sum[0] + (subsums.sum[0] >> 16)) & 0xFFFF;
    subsums.sum[0] = ~subsums.sum[0];
    std::cout << "checked checksum: " << std::hex << (uint16_t)subsums.sum[0](15, 0) << std::endl;
    valid_pkt_out.write(subsums.sum[0](15, 0) == 0);
  }
}
