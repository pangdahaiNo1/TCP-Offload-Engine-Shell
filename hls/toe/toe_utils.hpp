#ifndef _TOE_UTILS_HPP
#define _TOE_UTILS_HPP

#include "tcp_conn.hpp"
#include "toe_config.hpp"
#include "utils/axi_utils.hpp"

void                 RemoveIpHeader(const NetAXIS cur_word,
                                    const NetAXIS prev_word,

                                    ap_uint<4> ip_header_length,
                                    NetAXIS &  send_word) {
#pragma HLS          INLINE
#pragma HLS PIPELINE II = 1
  send_word.dest = prev_word.dest;
  // send_word.id   = prev_word.id;
  // send_word.user = prev_word.user;

  switch (ip_header_length) {
    case 5:
      send_word.data(447, 0)   = prev_word.data(511, 64);
      send_word.keep(55, 0)    = prev_word.keep(63, 8);
      send_word.data(511, 448) = cur_word.data(63, 0);
      send_word.keep(63, 56)   = cur_word.keep(7, 0);
      break;
    case 6:
      send_word.data(415, 0)   = prev_word.data(511, 96);
      send_word.keep(51, 0)    = prev_word.keep(63, 12);
      send_word.data(511, 416) = cur_word.data(95, 0);
      send_word.keep(63, 52)   = cur_word.keep(11, 0);
      break;
    case 7:
      send_word.data(383, 0)   = prev_word.data(511, 128);
      send_word.keep(47, 0)    = prev_word.keep(63, 16);
      send_word.data(511, 384) = cur_word.data(127, 0);
      send_word.keep(63, 48)   = cur_word.keep(15, 0);
      break;
    case 8:
      send_word.data(351, 0)   = prev_word.data(511, 160);
      send_word.keep(43, 0)    = prev_word.keep(63, 20);
      send_word.data(511, 352) = cur_word.data(159, 0);
      send_word.keep(63, 44)   = cur_word.keep(19, 0);
      break;
    case 9:
      send_word.data(319, 0)   = prev_word.data(511, 192);
      send_word.keep(39, 0)    = prev_word.keep(63, 36);
      send_word.data(511, 320) = cur_word.data(191, 0);
      send_word.keep(63, 40)   = cur_word.keep(23, 0);
      break;
    case 10:
      send_word.data(287, 0)   = prev_word.data(511, 224);
      send_word.keep(35, 0)    = prev_word.keep(63, 28);
      send_word.data(511, 288) = cur_word.data(223, 0);
      send_word.keep(63, 36)   = cur_word.keep(27, 0);
      break;
    case 11:
      send_word.data(255, 0)   = prev_word.data(511, 256);
      send_word.keep(31, 0)    = prev_word.keep(63, 32);
      send_word.data(511, 256) = cur_word.data(255, 0);
      send_word.keep(63, 32)   = cur_word.keep(31, 0);
      break;
    case 12:
      send_word.data(223, 0)   = prev_word.data(511, 288);
      send_word.keep(27, 0)    = prev_word.keep(63, 36);
      send_word.data(511, 224) = cur_word.data(287, 0);
      send_word.keep(63, 28)   = cur_word.keep(35, 0);
      break;
    case 13:
      send_word.data(191, 0)   = prev_word.data(511, 320);
      send_word.keep(23, 0)    = prev_word.keep(63, 40);
      send_word.data(511, 192) = cur_word.data(319, 0);
      send_word.keep(63, 24)   = cur_word.keep(39, 0);
      break;
    case 14:
      send_word.data(159, 0)   = prev_word.data(511, 352);
      send_word.keep(19, 0)    = prev_word.keep(63, 44);
      send_word.data(511, 160) = cur_word.data(351, 0);
      send_word.keep(63, 20)   = cur_word.keep(43, 0);
      break;
    case 15:
      send_word.data(127, 0)   = prev_word.data(511, 384);
      send_word.keep(15, 0)    = prev_word.keep(63, 48);
      send_word.data(511, 128) = cur_word.data(383, 0);
      send_word.keep(63, 16)   = cur_word.keep(47, 0);
      break;
    default:
      cout << "Error the offset is not valid" << endl;
      break;
  }
}

#endif