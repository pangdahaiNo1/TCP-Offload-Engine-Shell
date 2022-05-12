#include "axi_utils.hpp"

void ComputeSubChecksum(stream<NetAXIS> &pkt_in, stream<SubChecksum> &sub_checksum) {
#pragma HLS PIPELINE II = 1
#pragma HLS          INLINE
  static SubChecksum tcp_checksums;
  if (!pkt_in.empty()) {
    NetAXIS cur_word = pkt_in.read();
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

void CheckChecksum(stream<SubChecksum> &sub_checksum, stream<ap_uint<16> > &final_checksum) {
#pragma HLS PIPELINE II = 1
#pragma HLS          INLINE

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
    final_checksum.write(subsums.sum[0](15, 0));
  }
}

// In this context length==0 is not valid, then
// length=0 is consider as 64 which is every keep bit to '1'
ap_uint<64> DataLengthToAxisKeep(ap_uint<6> length) {
#pragma HLS INLINE
  const ap_uint<64> keep_table[64] = {
      0xFFFFFFFFFFFFFFFF, 0x0000000000000001, 0x0000000000000003, 0x0000000000000007,
      0x000000000000000F, 0x000000000000001F, 0x000000000000003F, 0x000000000000007F,
      0x00000000000000FF, 0x00000000000001FF, 0x00000000000003FF, 0x00000000000007FF,
      0x0000000000000FFF, 0x0000000000001FFF, 0x0000000000003FFF, 0x0000000000007FFF,
      0x000000000000FFFF, 0x000000000001FFFF, 0x000000000003FFFF, 0x000000000007FFFF,
      0x00000000000FFFFF, 0x00000000001FFFFF, 0x00000000003FFFFF, 0x00000000007FFFFF,
      0x0000000000FFFFFF, 0x0000000001FFFFFF, 0x0000000003FFFFFF, 0x0000000007FFFFFF,
      0x000000000FFFFFFF, 0x000000001FFFFFFF, 0x000000003FFFFFFF, 0x000000007FFFFFFF,
      0x00000000FFFFFFFF, 0x00000001FFFFFFFF, 0x00000003FFFFFFFF, 0x00000007FFFFFFFF,
      0x0000000FFFFFFFFF, 0x0000001FFFFFFFFF, 0x0000003FFFFFFFFF, 0x0000007FFFFFFFFF,
      0x000000FFFFFFFFFF, 0x000001FFFFFFFFFF, 0x000003FFFFFFFFFF, 0x000007FFFFFFFFFF,
      0x00000FFFFFFFFFFF, 0x00001FFFFFFFFFFF, 0x00003FFFFFFFFFFF, 0x00007FFFFFFFFFFF,
      0x0000FFFFFFFFFFFF, 0x0001FFFFFFFFFFFF, 0x0003FFFFFFFFFFFF, 0x0007FFFFFFFFFFFF,
      0x000FFFFFFFFFFFFF, 0x001FFFFFFFFFFFFF, 0x003FFFFFFFFFFFFF, 0x007FFFFFFFFFFFFF,
      0x00FFFFFFFFFFFFFF, 0x01FFFFFFFFFFFFFF, 0x03FFFFFFFFFFFFFF, 0x07FFFFFFFFFFFFFF,
      0x0FFFFFFFFFFFFFFF, 0x1FFFFFFFFFFFFFFF, 0x3FFFFFFFFFFFFFFF, 0x7FFFFFFFFFFFFFFF};
#pragma HLS RESOURCE variable = keep_table core = ROM_1P_LUTRAM latency = 1

  return keep_table[length];
}

/**
 * Concat two word into one word, prev_word tail + cur_word head = send word
 *
 *   512                              k                               0
 *   ------------------------------------------------------------------
 *   |           cur_word(k-1,0)     |      prev_word(511,k)          |
 *   ------------------------------------------------------------------
 *   Note: k = byte_offset * 8
 *
 */
void        ConcatTwoWords(const NetAXIS &   cur_word,
                           const NetAXIS &   prev_word,
                           const ap_uint<6> &byte_offset,
                           NetAXIS &         send_word) {
#pragma HLS INLINE
  switch (byte_offset) {
    case 0:
      send_word.data = cur_word.data;
      send_word.keep = cur_word.keep;
      break;
    case 1:
      send_word.data = (cur_word.data(7, 0), prev_word.data(511, 8));
      send_word.keep = (cur_word.keep(0, 0), prev_word.keep(63, 1));
      break;
    case 2:
      send_word.data = (cur_word.data(15, 0), prev_word.data(511, 16));
      send_word.keep = (cur_word.keep(1, 0), prev_word.keep(63, 2));
      break;
    case 3:
      send_word.data = (cur_word.data(23, 0), prev_word.data(511, 24));
      send_word.keep = (cur_word.keep(2, 0), prev_word.keep(63, 3));
      break;
    case 4:
      send_word.data = (cur_word.data(31, 0), prev_word.data(511, 32));
      send_word.keep = (cur_word.keep(3, 0), prev_word.keep(63, 4));
      break;
    case 5:
      send_word.data = (cur_word.data(39, 0), prev_word.data(511, 40));
      send_word.keep = (cur_word.keep(4, 0), prev_word.keep(63, 5));
      break;
    case 6:
      send_word.data = (cur_word.data(47, 0), prev_word.data(511, 48));
      send_word.keep = (cur_word.keep(5, 0), prev_word.keep(63, 6));
      break;
    case 7:
      send_word.data = (cur_word.data(55, 0), prev_word.data(511, 56));
      send_word.keep = (cur_word.keep(6, 0), prev_word.keep(63, 7));
      break;
    case 8:
      send_word.data = (cur_word.data(63, 0), prev_word.data(511, 64));
      send_word.keep = (cur_word.keep(7, 0), prev_word.keep(63, 8));
      break;
    case 9:
      send_word.data = (cur_word.data(71, 0), prev_word.data(511, 72));
      send_word.keep = (cur_word.keep(8, 0), prev_word.keep(63, 9));
      break;
    case 10:
      send_word.data = (cur_word.data(79, 0), prev_word.data(511, 80));
      send_word.keep = (cur_word.keep(9, 0), prev_word.keep(63, 10));
      break;
    case 11:
      send_word.data = (cur_word.data(87, 0), prev_word.data(511, 88));
      send_word.keep = (cur_word.keep(10, 0), prev_word.keep(63, 11));
      break;
    case 12:
      send_word.data = (cur_word.data(95, 0), prev_word.data(511, 96));
      send_word.keep = (cur_word.keep(11, 0), prev_word.keep(63, 12));
      break;
    case 13:
      send_word.data = (cur_word.data(103, 0), prev_word.data(511, 104));
      send_word.keep = (cur_word.keep(12, 0), prev_word.keep(63, 13));
      break;
    case 14:
      send_word.data = (cur_word.data(111, 0), prev_word.data(511, 112));
      send_word.keep = (cur_word.keep(13, 0), prev_word.keep(63, 14));
      break;
    case 15:
      send_word.data = (cur_word.data(119, 0), prev_word.data(511, 120));
      send_word.keep = (cur_word.keep(14, 0), prev_word.keep(63, 15));
      break;
    case 16:
      send_word.data = (cur_word.data(127, 0), prev_word.data(511, 128));
      send_word.keep = (cur_word.keep(15, 0), prev_word.keep(63, 16));
      break;
    case 17:
      send_word.data = (cur_word.data(135, 0), prev_word.data(511, 136));
      send_word.keep = (cur_word.keep(16, 0), prev_word.keep(63, 17));
      break;
    case 18:
      send_word.data = (cur_word.data(143, 0), prev_word.data(511, 144));
      send_word.keep = (cur_word.keep(17, 0), prev_word.keep(63, 18));
      break;
    case 19:
      send_word.data = (cur_word.data(151, 0), prev_word.data(511, 152));
      send_word.keep = (cur_word.keep(18, 0), prev_word.keep(63, 19));
      break;
    case 20:
      send_word.data = (cur_word.data(159, 0), prev_word.data(511, 160));
      send_word.keep = (cur_word.keep(19, 0), prev_word.keep(63, 20));
      break;
    case 21:
      send_word.data = (cur_word.data(167, 0), prev_word.data(511, 168));
      send_word.keep = (cur_word.keep(20, 0), prev_word.keep(63, 21));
      break;
    case 22:
      send_word.data = (cur_word.data(175, 0), prev_word.data(511, 176));
      send_word.keep = (cur_word.keep(21, 0), prev_word.keep(63, 22));
      break;
    case 23:
      send_word.data = (cur_word.data(183, 0), prev_word.data(511, 184));
      send_word.keep = (cur_word.keep(22, 0), prev_word.keep(63, 23));
      break;
    case 24:
      send_word.data = (cur_word.data(191, 0), prev_word.data(511, 192));
      send_word.keep = (cur_word.keep(23, 0), prev_word.keep(63, 24));
      break;
    case 25:
      send_word.data = (cur_word.data(199, 0), prev_word.data(511, 200));
      send_word.keep = (cur_word.keep(24, 0), prev_word.keep(63, 25));
      break;
    case 26:
      send_word.data = (cur_word.data(207, 0), prev_word.data(511, 208));
      send_word.keep = (cur_word.keep(25, 0), prev_word.keep(63, 26));
      break;
    case 27:
      send_word.data = (cur_word.data(215, 0), prev_word.data(511, 216));
      send_word.keep = (cur_word.keep(26, 0), prev_word.keep(63, 27));
      break;
    case 28:
      send_word.data = (cur_word.data(223, 0), prev_word.data(511, 224));
      send_word.keep = (cur_word.keep(27, 0), prev_word.keep(63, 28));
      break;
    case 29:
      send_word.data = (cur_word.data(231, 0), prev_word.data(511, 232));
      send_word.keep = (cur_word.keep(28, 0), prev_word.keep(63, 29));
      break;
    case 30:
      send_word.data = (cur_word.data(239, 0), prev_word.data(511, 240));
      send_word.keep = (cur_word.keep(29, 0), prev_word.keep(63, 30));
      break;
    case 31:
      send_word.data = (cur_word.data(247, 0), prev_word.data(511, 248));
      send_word.keep = (cur_word.keep(30, 0), prev_word.keep(63, 31));
      break;
    case 32:
      send_word.data = (cur_word.data(255, 0), prev_word.data(511, 256));
      send_word.keep = (cur_word.keep(31, 0), prev_word.keep(63, 32));
      break;
    case 33:
      send_word.data = (cur_word.data(263, 0), prev_word.data(511, 264));
      send_word.keep = (cur_word.keep(32, 0), prev_word.keep(63, 33));
      break;
    case 34:
      send_word.data = (cur_word.data(271, 0), prev_word.data(511, 272));
      send_word.keep = (cur_word.keep(33, 0), prev_word.keep(63, 34));
      break;
    case 35:
      send_word.data = (cur_word.data(279, 0), prev_word.data(511, 280));
      send_word.keep = (cur_word.keep(34, 0), prev_word.keep(63, 35));
      break;
    case 36:
      send_word.data = (cur_word.data(287, 0), prev_word.data(511, 288));
      send_word.keep = (cur_word.keep(35, 0), prev_word.keep(63, 36));
      break;
    case 37:
      send_word.data = (cur_word.data(295, 0), prev_word.data(511, 296));
      send_word.keep = (cur_word.keep(36, 0), prev_word.keep(63, 37));
      break;
    case 38:
      send_word.data = (cur_word.data(303, 0), prev_word.data(511, 304));
      send_word.keep = (cur_word.keep(37, 0), prev_word.keep(63, 38));
      break;
    case 39:
      send_word.data = (cur_word.data(311, 0), prev_word.data(511, 312));
      send_word.keep = (cur_word.keep(38, 0), prev_word.keep(63, 39));
      break;
    case 40:
      send_word.data = (cur_word.data(319, 0), prev_word.data(511, 320));
      send_word.keep = (cur_word.keep(39, 0), prev_word.keep(63, 40));
      break;
    case 41:
      send_word.data = (cur_word.data(327, 0), prev_word.data(511, 328));
      send_word.keep = (cur_word.keep(40, 0), prev_word.keep(63, 41));
      break;
    case 42:
      send_word.data = (cur_word.data(335, 0), prev_word.data(511, 336));
      send_word.keep = (cur_word.keep(41, 0), prev_word.keep(63, 42));
      break;
    case 43:
      send_word.data = (cur_word.data(343, 0), prev_word.data(511, 344));
      send_word.keep = (cur_word.keep(42, 0), prev_word.keep(63, 43));
      break;
    case 44:
      send_word.data = (cur_word.data(351, 0), prev_word.data(511, 352));
      send_word.keep = (cur_word.keep(43, 0), prev_word.keep(63, 44));
      break;
    case 45:
      send_word.data = (cur_word.data(359, 0), prev_word.data(511, 360));
      send_word.keep = (cur_word.keep(44, 0), prev_word.keep(63, 45));
      break;
    case 46:
      send_word.data = (cur_word.data(367, 0), prev_word.data(511, 368));
      send_word.keep = (cur_word.keep(45, 0), prev_word.keep(63, 46));
      break;
    case 47:
      send_word.data = (cur_word.data(375, 0), prev_word.data(511, 376));
      send_word.keep = (cur_word.keep(46, 0), prev_word.keep(63, 47));
      break;
    case 48:
      send_word.data = (cur_word.data(383, 0), prev_word.data(511, 384));
      send_word.keep = (cur_word.keep(47, 0), prev_word.keep(63, 48));
      break;
    case 49:
      send_word.data = (cur_word.data(391, 0), prev_word.data(511, 392));
      send_word.keep = (cur_word.keep(48, 0), prev_word.keep(63, 49));
      break;
    case 50:
      send_word.data = (cur_word.data(399, 0), prev_word.data(511, 400));
      send_word.keep = (cur_word.keep(49, 0), prev_word.keep(63, 50));
      break;
    case 51:
      send_word.data = (cur_word.data(407, 0), prev_word.data(511, 408));
      send_word.keep = (cur_word.keep(50, 0), prev_word.keep(63, 51));
      break;
    case 52:
      send_word.data = (cur_word.data(415, 0), prev_word.data(511, 416));
      send_word.keep = (cur_word.keep(51, 0), prev_word.keep(63, 52));
      break;
    case 53:
      send_word.data = (cur_word.data(423, 0), prev_word.data(511, 424));
      send_word.keep = (cur_word.keep(52, 0), prev_word.keep(63, 53));
      break;
    case 54:
      send_word.data = (cur_word.data(431, 0), prev_word.data(511, 432));
      send_word.keep = (cur_word.keep(53, 0), prev_word.keep(63, 54));
      break;
    case 55:
      send_word.data = (cur_word.data(439, 0), prev_word.data(511, 440));
      send_word.keep = (cur_word.keep(54, 0), prev_word.keep(63, 55));
      break;
    case 56:
      send_word.data = (cur_word.data(447, 0), prev_word.data(511, 448));
      send_word.keep = (cur_word.keep(55, 0), prev_word.keep(63, 56));
      break;
    case 57:
      send_word.data = (cur_word.data(455, 0), prev_word.data(511, 456));
      send_word.keep = (cur_word.keep(56, 0), prev_word.keep(63, 57));
      break;
    case 58:
      send_word.data = (cur_word.data(463, 0), prev_word.data(511, 464));
      send_word.keep = (cur_word.keep(57, 0), prev_word.keep(63, 58));
      break;
    case 59:
      send_word.data = (cur_word.data(471, 0), prev_word.data(511, 472));
      send_word.keep = (cur_word.keep(58, 0), prev_word.keep(63, 59));
      break;
    case 60:
      send_word.data = (cur_word.data(479, 0), prev_word.data(511, 480));
      send_word.keep = (cur_word.keep(59, 0), prev_word.keep(63, 60));
      break;
    case 61:
      send_word.data = (cur_word.data(487, 0), prev_word.data(511, 488));
      send_word.keep = (cur_word.keep(60, 0), prev_word.keep(63, 61));
      break;
    case 62:
      send_word.data = (cur_word.data(495, 0), prev_word.data(511, 496));
      send_word.keep = (cur_word.keep(61, 0), prev_word.keep(63, 62));
      break;
    case 63:
      send_word.data = (cur_word.data(503, 0), prev_word.data(511, 504));
      send_word.keep = (cur_word.keep(62, 0), prev_word.keep(63, 63));
      break;
  }
}

/**
 * Merge two words head part, prev_word head + cur_word head = send word
 *
 *   512                              k                               0
 *   ------------------------------------------------------------------
 *   |           cur_word(511-k,0)     |      prev_word(k,0)          |
 *   ------------------------------------------------------------------
 *   Note: k = byte_offset * 8
 *
 */
void MergeTwoWordsHead(const NetAXIS &cur_word,
                       const NetAXIS &prev_word,
                       ap_uint<6>     byte_offset,
                       NetAXIS &      send_word) {
  switch (byte_offset) {
    case 0:
      send_word.data = cur_word.data;
      send_word.keep = cur_word.keep;
      break;
    case 1:
      send_word.data = (cur_word.data(503, 0), prev_word.data(7, 0));
      send_word.keep = (cur_word.keep(62, 0), prev_word.keep(0, 0));
      break;
    case 2:
      send_word.data = (cur_word.data(495, 0), prev_word.data(15, 0));
      send_word.keep = (cur_word.keep(61, 0), prev_word.keep(1, 0));
      break;
    case 3:
      send_word.data = (cur_word.data(487, 0), prev_word.data(23, 0));
      send_word.keep = (cur_word.keep(60, 0), prev_word.keep(2, 0));
      break;
    case 4:
      send_word.data = (cur_word.data(479, 0), prev_word.data(31, 0));
      send_word.keep = (cur_word.keep(59, 0), prev_word.keep(3, 0));
      break;
    case 5:
      send_word.data = (cur_word.data(471, 0), prev_word.data(39, 0));
      send_word.keep = (cur_word.keep(58, 0), prev_word.keep(4, 0));
      break;
    case 6:
      send_word.data = (cur_word.data(463, 0), prev_word.data(47, 0));
      send_word.keep = (cur_word.keep(57, 0), prev_word.keep(5, 0));
      break;
    case 7:
      send_word.data = (cur_word.data(455, 0), prev_word.data(55, 0));
      send_word.keep = (cur_word.keep(56, 0), prev_word.keep(6, 0));
      break;
    case 8:
      send_word.data = (cur_word.data(447, 0), prev_word.data(63, 0));
      send_word.keep = (cur_word.keep(55, 0), prev_word.keep(7, 0));
      break;
    case 9:
      send_word.data = (cur_word.data(439, 0), prev_word.data(71, 0));
      send_word.keep = (cur_word.keep(54, 0), prev_word.keep(8, 0));
      break;
    case 10:
      send_word.data = (cur_word.data(431, 0), prev_word.data(79, 0));
      send_word.keep = (cur_word.keep(53, 0), prev_word.keep(9, 0));
      break;
    case 11:
      send_word.data = (cur_word.data(423, 0), prev_word.data(87, 0));
      send_word.keep = (cur_word.keep(52, 0), prev_word.keep(10, 0));
      break;
    case 12:
      send_word.data = (cur_word.data(415, 0), prev_word.data(95, 0));
      send_word.keep = (cur_word.keep(51, 0), prev_word.keep(11, 0));
      break;
    case 13:
      send_word.data = (cur_word.data(407, 0), prev_word.data(103, 0));
      send_word.keep = (cur_word.keep(50, 0), prev_word.keep(12, 0));
      break;
    case 14:
      send_word.data = (cur_word.data(399, 0), prev_word.data(111, 0));
      send_word.keep = (cur_word.keep(49, 0), prev_word.keep(13, 0));
      break;
    case 15:
      send_word.data = (cur_word.data(391, 0), prev_word.data(119, 0));
      send_word.keep = (cur_word.keep(48, 0), prev_word.keep(14, 0));
      break;
    case 16:
      send_word.data = (cur_word.data(383, 0), prev_word.data(127, 0));
      send_word.keep = (cur_word.keep(47, 0), prev_word.keep(15, 0));
      break;
    case 17:
      send_word.data = (cur_word.data(375, 0), prev_word.data(135, 0));
      send_word.keep = (cur_word.keep(46, 0), prev_word.keep(16, 0));
      break;
    case 18:
      send_word.data = (cur_word.data(367, 0), prev_word.data(143, 0));
      send_word.keep = (cur_word.keep(45, 0), prev_word.keep(17, 0));
      break;
    case 19:
      send_word.data = (cur_word.data(359, 0), prev_word.data(151, 0));
      send_word.keep = (cur_word.keep(44, 0), prev_word.keep(18, 0));
      break;
    case 20:
      send_word.data = (cur_word.data(351, 0), prev_word.data(159, 0));
      send_word.keep = (cur_word.keep(43, 0), prev_word.keep(19, 0));
      break;
    case 21:
      send_word.data = (cur_word.data(343, 0), prev_word.data(167, 0));
      send_word.keep = (cur_word.keep(42, 0), prev_word.keep(20, 0));
      break;
    case 22:
      send_word.data = (cur_word.data(335, 0), prev_word.data(175, 0));
      send_word.keep = (cur_word.keep(41, 0), prev_word.keep(21, 0));
      break;
    case 23:
      send_word.data = (cur_word.data(327, 0), prev_word.data(183, 0));
      send_word.keep = (cur_word.keep(40, 0), prev_word.keep(22, 0));
      break;
    case 24:
      send_word.data = (cur_word.data(319, 0), prev_word.data(191, 0));
      send_word.keep = (cur_word.keep(39, 0), prev_word.keep(23, 0));
      break;
    case 25:
      send_word.data = (cur_word.data(311, 0), prev_word.data(199, 0));
      send_word.keep = (cur_word.keep(38, 0), prev_word.keep(24, 0));
      break;
    case 26:
      send_word.data = (cur_word.data(303, 0), prev_word.data(207, 0));
      send_word.keep = (cur_word.keep(37, 0), prev_word.keep(25, 0));
      break;
    case 27:
      send_word.data = (cur_word.data(295, 0), prev_word.data(215, 0));
      send_word.keep = (cur_word.keep(36, 0), prev_word.keep(26, 0));
      break;
    case 28:
      send_word.data = (cur_word.data(287, 0), prev_word.data(223, 0));
      send_word.keep = (cur_word.keep(35, 0), prev_word.keep(27, 0));
      break;
    case 29:
      send_word.data = (cur_word.data(279, 0), prev_word.data(231, 0));
      send_word.keep = (cur_word.keep(34, 0), prev_word.keep(28, 0));
      break;
    case 30:
      send_word.data = (cur_word.data(271, 0), prev_word.data(239, 0));
      send_word.keep = (cur_word.keep(33, 0), prev_word.keep(29, 0));
      break;
    case 31:
      send_word.data = (cur_word.data(263, 0), prev_word.data(247, 0));
      send_word.keep = (cur_word.keep(32, 0), prev_word.keep(30, 0));
      break;
    case 32:
      send_word.data = (cur_word.data(255, 0), prev_word.data(255, 0));
      send_word.keep = (cur_word.keep(31, 0), prev_word.keep(31, 0));
      break;
    case 33:
      send_word.data = (cur_word.data(247, 0), prev_word.data(263, 0));
      send_word.keep = (cur_word.keep(30, 0), prev_word.keep(32, 0));
      break;
    case 34:
      send_word.data = (cur_word.data(239, 0), prev_word.data(271, 0));
      send_word.keep = (cur_word.keep(29, 0), prev_word.keep(33, 0));
      break;
    case 35:
      send_word.data = (cur_word.data(231, 0), prev_word.data(279, 0));
      send_word.keep = (cur_word.keep(28, 0), prev_word.keep(34, 0));
      break;
    case 36:
      send_word.data = (cur_word.data(223, 0), prev_word.data(287, 0));
      send_word.keep = (cur_word.keep(27, 0), prev_word.keep(35, 0));
      break;
    case 37:
      send_word.data = (cur_word.data(215, 0), prev_word.data(295, 0));
      send_word.keep = (cur_word.keep(26, 0), prev_word.keep(36, 0));
      break;
    case 38:
      send_word.data = (cur_word.data(207, 0), prev_word.data(303, 0));
      send_word.keep = (cur_word.keep(25, 0), prev_word.keep(37, 0));
      break;
    case 39:
      send_word.data = (cur_word.data(199, 0), prev_word.data(311, 0));
      send_word.keep = (cur_word.keep(24, 0), prev_word.keep(38, 0));
      break;
    case 40:
      send_word.data = (cur_word.data(191, 0), prev_word.data(319, 0));
      send_word.keep = (cur_word.keep(23, 0), prev_word.keep(39, 0));
      break;
    case 41:
      send_word.data = (cur_word.data(183, 0), prev_word.data(327, 0));
      send_word.keep = (cur_word.keep(22, 0), prev_word.keep(40, 0));
      break;
    case 42:
      send_word.data = (cur_word.data(175, 0), prev_word.data(335, 0));
      send_word.keep = (cur_word.keep(21, 0), prev_word.keep(41, 0));
      break;
    case 43:
      send_word.data = (cur_word.data(167, 0), prev_word.data(343, 0));
      send_word.keep = (cur_word.keep(20, 0), prev_word.keep(42, 0));
      break;
    case 44:
      send_word.data = (cur_word.data(159, 0), prev_word.data(351, 0));
      send_word.keep = (cur_word.keep(19, 0), prev_word.keep(43, 0));
      break;
    case 45:
      send_word.data = (cur_word.data(151, 0), prev_word.data(359, 0));
      send_word.keep = (cur_word.keep(18, 0), prev_word.keep(44, 0));
      break;
    case 46:
      send_word.data = (cur_word.data(143, 0), prev_word.data(367, 0));
      send_word.keep = (cur_word.keep(17, 0), prev_word.keep(45, 0));
      break;
    case 47:
      send_word.data = (cur_word.data(135, 0), prev_word.data(375, 0));
      send_word.keep = (cur_word.keep(16, 0), prev_word.keep(46, 0));
      break;
    case 48:
      send_word.data = (cur_word.data(127, 0), prev_word.data(383, 0));
      send_word.keep = (cur_word.keep(15, 0), prev_word.keep(47, 0));
      break;
    case 49:
      send_word.data = (cur_word.data(119, 0), prev_word.data(391, 0));
      send_word.keep = (cur_word.keep(14, 0), prev_word.keep(48, 0));
      break;
    case 50:
      send_word.data = (cur_word.data(111, 0), prev_word.data(399, 0));
      send_word.keep = (cur_word.keep(13, 0), prev_word.keep(49, 0));
      break;
    case 51:
      send_word.data = (cur_word.data(103, 0), prev_word.data(407, 0));
      send_word.keep = (cur_word.keep(12, 0), prev_word.keep(50, 0));
      break;
    case 52:
      send_word.data = (cur_word.data(95, 0), prev_word.data(415, 0));
      send_word.keep = (cur_word.keep(11, 0), prev_word.keep(51, 0));
      break;
    case 53:
      send_word.data = (cur_word.data(87, 0), prev_word.data(423, 0));
      send_word.keep = (cur_word.keep(10, 0), prev_word.keep(52, 0));
      break;
    case 54:
      send_word.data = (cur_word.data(79, 0), prev_word.data(431, 0));
      send_word.keep = (cur_word.keep(9, 0), prev_word.keep(53, 0));
      break;
    case 55:
      send_word.data = (cur_word.data(71, 0), prev_word.data(439, 0));
      send_word.keep = (cur_word.keep(8, 0), prev_word.keep(54, 0));
      break;
    case 56:
      send_word.data = (cur_word.data(63, 0), prev_word.data(447, 0));
      send_word.keep = (cur_word.keep(7, 0), prev_word.keep(55, 0));
      break;
    case 57:
      send_word.data = (cur_word.data(55, 0), prev_word.data(455, 0));
      send_word.keep = (cur_word.keep(6, 0), prev_word.keep(56, 0));
      break;
    case 58:
      send_word.data = (cur_word.data(47, 0), prev_word.data(463, 0));
      send_word.keep = (cur_word.keep(5, 0), prev_word.keep(57, 0));
      break;
    case 59:
      send_word.data = (cur_word.data(39, 0), prev_word.data(471, 0));
      send_word.keep = (cur_word.keep(4, 0), prev_word.keep(58, 0));
      break;
    case 60:
      send_word.data = (cur_word.data(31, 0), prev_word.data(479, 0));
      send_word.keep = (cur_word.keep(3, 0), prev_word.keep(59, 0));
      break;
    case 61:
      send_word.data = (cur_word.data(23, 0), prev_word.data(487, 0));
      send_word.keep = (cur_word.keep(2, 0), prev_word.keep(60, 0));
      break;
    case 62:
      send_word.data = (cur_word.data(15, 0), prev_word.data(495, 0));
      send_word.keep = (cur_word.keep(1, 0), prev_word.keep(61, 0));
      break;
    case 63:
      send_word.data = (cur_word.data(7, 0), prev_word.data(503, 0));
      send_word.keep = (cur_word.keep(0, 0), prev_word.keep(62, 0));
      break;
  }
}

/**
 * Right shift one word, the MSB fills 0
 *
 *   512                              k                               0
 *   ------------------------------------------------------------------
 *   |          zero                 |      cur_word(511,k)           |
 *   ------------------------------------------------------------------
 *   Note: k = shift_byte  * 8
 *
 */
void RightShiftWord(const NetAXIS &cur_word, const ap_uint<6> &shift_byte, NetAXIS &send_word) {
#pragma HLS INLINE

  send_word.data = 0;
  send_word.keep = 0;
  send_word.data = cur_word.data >> (shift_byte << 3);
  send_word.keep = cur_word.keep >> (shift_byte);
}