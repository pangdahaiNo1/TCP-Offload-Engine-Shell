#include "memory_access.hpp"

/**
 * Concat two word into one word
 *
 *   512                              k                               0
 *   ------------------------------------------------------------------
 *   |           cur_word(k,0)       |      prev_word(511,k)          |
 *   ------------------------------------------------------------------
 *   Note: k = byte_offset * 8
 *
 */
void ConcatTwoWords(NetAXIS cur_word, NetAXIS prev_word, ap_uint<6> byte_offset, NetAXIS &send_word

) {
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
 * This module reads a command and determines whether two memory access are
 * needed or not. In base of that issues one or two memory write command(s), and
 * the data is aligned properly, the realignment if is necessary is done in the
 * second access.
 *
 * @param      tx_app_to_mem_data_in   Packet payload if any
 * @param      tx_app_to_mem_cmd_in    Internal command to write data into the memory. It does
 * not take into account buffer overflow
 * @param      mover_mem_cmd_out   Command to the data mover. It takes into account buffer
 * overflow. Two write commands when buffer overflows
 * @param      mover_mem_data_out  Data to memory. If the buffer overflows, the second part
 * of the data has to be realigned
 */
void                 TxAppWriteDataToMem(stream<NetAXIS> &     tx_app_to_mem_data_in,
                                         stream<DataMoverCmd> &tx_app_to_mem_cmd_in,
                                         stream<NetAXIS> &     mover_mem_data_out,
                                         stream<DataMoverCmd> &mover_mem_cmd_out) {
#pragma HLS pipeline II = 1
#pragma HLS INLINE   off

  enum TxAppWriteFsmState {
    WAIT_CMD,
    FWD_NO_BREAKDOWN,
    FWD_BREAKDOWN_0,
    FWD_BREAKDOWN_1,
    FWD_EXTRA
  };
  static TxAppWriteFsmState fsm_state = WAIT_CMD;

  // if breakdown to two cmd, the bytes to transfer in the first command
  static ap_uint<23> first_cmd_bbt;
  // if breakdown, the last word in first cmd could not equal to 64B
  // if not equal to 64B, record the offset for concating to the new words
  static ap_uint<6> byte_offset;
  // last word in the first cmd
  static ap_uint<64> first_cmd_last_word_keep;
  // first word send word total and counter
  static ap_uint<10> first_cmd_send_words;
  static ap_uint<10> first_cmd_send_cnt = 1;

  static DataMoverCmd to_mem_cmd;
  static DataMoverCmd send_cmd;

  ap_uint<WINDOW_BITS + 1> buffer_overflow_addr;

  NetAXIS        cur_word;
  NetAXIS        send_word;
  static NetAXIS prev_word;

  switch (fsm_state) {
    case WAIT_CMD:
      if (!tx_app_to_mem_cmd_in.empty()) {
        tx_app_to_mem_cmd_in.read(to_mem_cmd);
        // Compute the address of the last byte to write
        buffer_overflow_addr = to_mem_cmd.saddr(WINDOW_BITS - 1, 0) + to_mem_cmd.bbt;
        // The remaining buffer space is not enough. need to breakdown cmd
        if (buffer_overflow_addr.bit(WINDOW_BITS)) {
          // Compute how much bytes are needed in the first transaction
          send_cmd.bbt   = BUFFER_SIZE - to_mem_cmd.saddr(WINDOW_BITS - 1, 0);
          send_cmd.saddr = to_mem_cmd.saddr;
          // Determines the position of the MSB in the last word
          // equals to bbt % 64
          byte_offset   = send_cmd.bbt.range(5, 0);
          first_cmd_bbt = send_cmd.bbt;
          // Get the keep of the last transaction of  the first memory offset;
          first_cmd_last_word_keep = DataLengthToAxisKeep(byte_offset);
          // Determines how many transaction are in the first memory access
          if (byte_offset != 0) {
            first_cmd_send_words = send_cmd.bbt.range(WINDOW_BITS - 1, 6) + 1;
          } else {
            first_cmd_send_words = send_cmd.bbt.range(WINDOW_BITS - 1, 6);
          }
          first_cmd_send_cnt = 1;
          fsm_state          = FWD_BREAKDOWN_0;
        } else {
          send_cmd  = to_mem_cmd;
          fsm_state = FWD_NO_BREAKDOWN;
        }
        // issue the first command
        mover_mem_cmd_out.write(send_cmd);
      }
      break;
    case FWD_NO_BREAKDOWN:
      if (!tx_app_to_mem_data_in.empty()) {
        tx_app_to_mem_data_in.read(cur_word);
        send_word.data = cur_word.data;
        send_word.keep = cur_word.keep;
        send_word.last = cur_word.last;

        if (cur_word.last) {
          fsm_state = WAIT_CMD;
        }
        mover_mem_data_out.write(send_word);
      }
      break;
    case FWD_BREAKDOWN_0:
      // the first cmd write state
      if (!tx_app_to_mem_data_in.empty()) {
        tx_app_to_mem_data_in.read(cur_word);
        send_word.last = 0;
        if (first_cmd_send_cnt == first_cmd_send_words) {
          send_word.data = cur_word.data;
          send_word.keep = first_cmd_last_word_keep;  // last word length = byte_offset
          send_word.last = 1;
          // the session buffer start address
          send_cmd.saddr(31, WINDOW_BITS)    = to_mem_cmd.saddr(31, WINDOW_BITS);
          send_cmd.saddr(WINDOW_BITS - 1, 0) = 0;

          // Recompute the bytes to transfer in the second memory access
          send_cmd.bbt = to_mem_cmd.bbt - first_cmd_bbt;
          // Issue the second command
          mover_mem_cmd_out.write(send_cmd);
          if (cur_word.last) {
            fsm_state = FWD_EXTRA;
          } else {
            fsm_state = FWD_BREAKDOWN_1;
          }
        } else {
          send_word = cur_word;
        }
        first_cmd_send_cnt++;
        prev_word = cur_word;
        mover_mem_data_out.write(send_word);
      }
      break;
    case FWD_BREAKDOWN_1:
      // the second cmd write state
      if (!tx_app_to_mem_data_in.empty()) {
        tx_app_to_mem_data_in.read(cur_word);
        ConcatTwoWords(cur_word, prev_word, byte_offset, send_word);

        send_word.last = 0;
        if (cur_word.last) {
          if (cur_word.keep.bit(byte_offset) && byte_offset != 0) {
            fsm_state = FWD_EXTRA;
          } else {
            fsm_state      = WAIT_CMD;
            send_word.last = 1;
          }
        }
        prev_word = cur_word;
        mover_mem_data_out.write(send_word);
      }
      break;
    case FWD_EXTRA:
      // The EXTRA part of the memory write has less than 64 bytes
      ConcatTwoWords(NetAXIS(), prev_word, byte_offset, send_word);
      send_word.last = 1;
      mover_mem_data_out.write(send_word);
      fsm_state = WAIT_CMD;
      break;
  }
}
