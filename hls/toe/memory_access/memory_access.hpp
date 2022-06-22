#ifndef _MEMORY_ACCESS_HPP_
#define _MEMORY_ACCESS_HPP_

#include "toe/toe_conn.hpp"
#include "utils/axi_utils.hpp"
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

/**
 * The template parameter is used to generate two different instances,
 * in rx_app_intf @p is_rx = 1,
 * in tx_engine @p is_rx = 0
 */
template <int is_rx>
void ReadDataSendCmd(
    // inner read mem cmd, from rx app intf or tx engine
    stream<MemBufferRWCmd> &            to_mover_read_mem_cmd_in,
    stream<DataMoverCmd> &              mover_read_mem_cmd_out,
    stream<MemBufferRWCmdDoubleAccess> &mem_buffer_double_access) {
#pragma HLS pipeline II = 1
#pragma HLS INLINE   off

#pragma HLS INTERFACE axis port = mover_read_mem_cmd_out
#pragma HLS aggregate variable = mover_read_mem_cmd_out compact = bit

  static bool                read_cmd_is_breakdown = false;
  static MemBufferRWCmd      inner_cmd;
  static ap_uint<16>         first_cmd_bbt = 0;
  DataMoverCmd               data_mver_cmd;
  MemBufferRWCmdDoubleAccess double_access_cmd;
  // use template parameter to determine variable LOG_MODULE
  ToeModule LOG_MODULE = is_rx == 1 ? RX_APP_IF : TX_ENGINE;

  if (read_cmd_is_breakdown == false) {
    if (!to_mover_read_mem_cmd_in.empty()) {
      to_mover_read_mem_cmd_in.read(inner_cmd);
      data_mver_cmd = DataMoverCmd(inner_cmd.addr, inner_cmd.length);
      // Check for overflow
      if (inner_cmd.next_addr.bit(WINDOW_BITS)) {
        // Compute remaining space in the buffer before overflow
        first_cmd_bbt = BUFFER_SIZE - inner_cmd.addr(WINDOW_BITS - 1, 0);
        data_mver_cmd = DataMoverCmd(inner_cmd.addr, first_cmd_bbt);

        read_cmd_is_breakdown           = true;
        double_access_cmd.double_access = true;
        // Offset of MSB byte valid in the last beat of the first trans
        double_access_cmd.byte_offset = first_cmd_bbt(5, 0);
      }
      mover_read_mem_cmd_out.write(data_mver_cmd);
      mem_buffer_double_access.write(double_access_cmd);
      logger.Info(LOG_MODULE, "ReadMem Req", inner_cmd.to_string());
      logger.Info(LOG_MODULE, DATA_MVER, "ReadMem Cmd0", data_mver_cmd.to_string());
      logger.Info(LOG_MODULE, "ReadMem Double? ", double_access_cmd.to_string());
    }
  } else if (read_cmd_is_breakdown == true) {
    // Clear the least significant bits of the address
    inner_cmd.addr.range(WINDOW_BITS - 1, 0) = 0;
    data_mver_cmd = DataMoverCmd(inner_cmd.addr, inner_cmd.length - first_cmd_bbt);
    mover_read_mem_cmd_out.write(data_mver_cmd);
    logger.Info(LOG_MODULE, DATA_MVER, "ReadMem Cmd1", data_mver_cmd.to_string());
    read_cmd_is_breakdown = false;
  }
}

/**
 * The template parameter is used to generate two different instances,
 * in rx_app_intf @p is_rx = 1,
 * in tx_engine @p is_rx = 0
 */
template <int is_rx>
void ReadDataFromMem(
    // datamover read data
    stream<NetAXIS> &                   mover_read_mem_data_in,
    stream<MemBufferRWCmdDoubleAccess> &mem_buffer_double_access,
    stream<NetAXISWord> &               to_rx_or_tx_read_data) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

#pragma HLS INTERFACE axis port = mover_read_mem_data_in

  enum TxEngReadDataFsmState {
    READ_ACCESS,
    NO_BREAKDOWN,
    BREAKDOWN_BLOCK_0,
    BREAKDOWN_ALIGNED,
    FIRST_MERGE,
    BREAKDOWN_BLOCK_1,
    EXTRA_DATA
  };

  const TxEngReadDataFsmState  INITIAL_STATE = READ_ACCESS;
  static TxEngReadDataFsmState fsm_state     = INITIAL_STATE;

  static ap_uint<6> offset_block0;
  static ap_uint<6> offset_block1;

  MemBufferRWCmdDoubleAccess mem_double_access;
  static NetAXISWord         prev_beat_word;
  // NetAXIS                    read_mem_data;
  NetAXISWord cur_beat_word;
  NetAXISWord send_beat_word;

  bool is_ddr_bypass;
  bool to_rx_or_tx_word_need_to_align = true;
  // use template parameter to determine variable LOG_MODULE
  ToeModule LOG_MODULE = is_rx == 1 ? RX_APP_IF : TX_ENGINE;

  switch (fsm_state) {
    case READ_ACCESS:
      if (!mem_buffer_double_access.empty()) {
        mem_buffer_double_access.read(mem_double_access);
        offset_block0 = mem_double_access.byte_offset;
        offset_block1 = 64 - mem_double_access.byte_offset;
        logger.Info(LOG_MODULE, "Offset0", offset_block0.to_string(16));
        logger.Info(LOG_MODULE, "Offset1", offset_block1.to_string(16));

        if (mem_double_access.double_access) {
          fsm_state = BREAKDOWN_BLOCK_0;
        } else {
          fsm_state = NO_BREAKDOWN;
        }
      }
      break;
    case NO_BREAKDOWN:
    case BREAKDOWN_ALIGNED:
      if (!mover_read_mem_data_in.empty() && !to_rx_or_tx_read_data.full()) {
        cur_beat_word = mover_read_mem_data_in.read();
        to_rx_or_tx_read_data.write(cur_beat_word);
        logger.Info(MOCK_MEMY, DATA_MVER, "ReadMemData ALIGNED", cur_beat_word.to_string());
        logger.Info(LOG_MODULE, "PayloadData ALIGNED", cur_beat_word.to_string());

        if (cur_beat_word.last) {
          fsm_state = INITIAL_STATE;
        }
      }
      break;
    case BREAKDOWN_BLOCK_0:
      if (!mover_read_mem_data_in.empty() && !to_rx_or_tx_read_data.full()) {
        cur_beat_word = mover_read_mem_data_in.read();
        logger.Info(MOCK_MEMY, DATA_MVER, "ReadMemData BREAK0", cur_beat_word.to_string());

        send_beat_word.data = cur_beat_word.data;
        send_beat_word.keep = cur_beat_word.keep;
        send_beat_word.last = 0;
        if (cur_beat_word.last) {
          if (offset_block0 == 0) {
            to_rx_or_tx_word_need_to_align = false;
            fsm_state                      = BREAKDOWN_ALIGNED;
          } else {  // This mean that the last not has all byte valid
            to_rx_or_tx_word_need_to_align = true;
            fsm_state                      = FIRST_MERGE;
          }
        }
        if (!to_rx_or_tx_word_need_to_align) {
          to_rx_or_tx_read_data.write(send_beat_word);
          logger.Info(LOG_MODULE, "PayloadData BREAK0", send_beat_word.to_string());
        }

        prev_beat_word = cur_beat_word;
      }
      break;
    case FIRST_MERGE:
      if (!mover_read_mem_data_in.empty() && !to_rx_or_tx_read_data.full()) {
        cur_beat_word = mover_read_mem_data_in.read();
        logger.Info(MOCK_MEMY, DATA_MVER, "ReadMemData MERGE", cur_beat_word.to_string());

        MergeTwoWordsHead(cur_beat_word, prev_beat_word, offset_block0, send_beat_word);
        send_beat_word.last = 0;

        if (cur_beat_word.last) {
          if (cur_beat_word.keep.bit(offset_block1)) {
            fsm_state = EXTRA_DATA;
          } else {
            send_beat_word.last = 1;
            fsm_state           = INITIAL_STATE;
          }
        } else {
          fsm_state = BREAKDOWN_BLOCK_1;
        }
        prev_beat_word = cur_beat_word;

        to_rx_or_tx_read_data.write(send_beat_word);
        logger.Info(DATA_MVER, LOG_MODULE, "PayloadData MERGE", send_beat_word.to_string());
      }
      break;
    case BREAKDOWN_BLOCK_1:
      if (!mover_read_mem_data_in.empty() && !to_rx_or_tx_read_data.full()) {
        cur_beat_word = mover_read_mem_data_in.read();
        logger.Info(DATA_MVER, LOG_MODULE, "PayloadData BREAK1", send_beat_word.to_string());

        ConcatTwoWords(cur_beat_word, prev_beat_word, offset_block1, send_beat_word);

        send_beat_word.last = 0;
        if (cur_beat_word.last) {
          if (cur_beat_word.keep.bit(offset_block1)) {
            fsm_state = EXTRA_DATA;
          } else {
            send_beat_word.last = 1;
            fsm_state           = INITIAL_STATE;
          }
        }

        prev_beat_word = cur_beat_word;
        to_rx_or_tx_read_data.write(send_beat_word);
        logger.Info(DATA_MVER, LOG_MODULE, "PayloadData BREAK1", send_beat_word.to_string());
      }
      break;
    case EXTRA_DATA:
      if (!to_rx_or_tx_read_data.full()) {
        ConcatTwoWords(NetAXISWord(), prev_beat_word, offset_block1, send_beat_word);
        send_beat_word.last = 1;

        to_rx_or_tx_read_data.write(send_beat_word);
        logger.Info(DATA_MVER, LOG_MODULE, "PayloadData EXTRA", send_beat_word.to_string());

        fsm_state = INITIAL_STATE;
      }
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
 * @param      mover_write_mem_data_out  Data to memory. If the buffer overflows, the second part
 * of the data has to be realigned
 * @param      mover_write_mem_cmd_out   Command to the data mover. It takes into account buffer
 * overflow. Two write commands when buffer overflows
 *
 *
 * The template parameter is used to generate two different instances,
 * in rx_engine   @p is_rx = 1,
 * in tx_app_intf @p is_rx = 0
 */
template <int is_rx>
void WriteDataToMem(
    // inner write mem cmd, from rx engine or tx app intf
    stream<MemBufferRWCmd> &to_mover_write_mem_cmd_in,
    stream<NetAXISWord> &   from_rx_or_tx_write_data,
    stream<DataMoverCmd> &  mover_write_mem_cmd_out,
    stream<NetAXIS> &       mover_write_mem_data_out,
    // if mem access break down, write true to this fifo
    stream<ap_uint<1> > &mem_buffer_double_access_flag) {
#pragma HLS pipeline II = 1
#pragma HLS INLINE   off

#pragma HLS aggregate variable = to_mover_write_mem_cmd_in compact = bit
#pragma HLS aggregate variable = mover_write_mem_cmd_out compact = bit
// out data + cmd
#pragma HLS INTERFACE axis port = mover_write_mem_data_out
#pragma HLS INTERFACE axis port = mover_write_mem_cmd_out

  enum WriteDataFsmState {
    WAIT_CMD,
    FWD_NO_BREAKDOWN,
    FWD_BREAKDOWN_0,
    FWD_BREAKDOWN_1,
    FWD_EXTRA
  };
  static WriteDataFsmState fsm_state = WAIT_CMD;

  // if breakdown to two cmd, the bytes to transfer in the first command
  static ap_uint<23> first_cmd_bbt;
  // if breakdown, the last word in first cmd could not equal to 64B
  // if not equal to 64B, record the offset for concating to the new words
  static ap_uint<6> byte_offset;
  // last word in the first cmd
  static ap_uint<64> first_cmd_last_beat_keep;
  // first word want to transfer several beats
  static ap_uint<10> first_cmd_trans_beats;
  static ap_uint<10> first_cmd_trans_beats_cnt = 1;

  static MemBufferRWCmd inner_cmd;
  static DataMoverCmd   data_mver_cmd;

  bool               rx_or_tx_write_break_down = false;
  NetAXISWord        cur_beat_word;
  NetAXISWord        send_beat_word;
  static NetAXISWord prev_beat_word;

  ToeModule LOG_MODULE = is_rx == 1 ? RX_ENGINE : TX_APP_IF;

  switch (fsm_state) {
    case WAIT_CMD:
      if (!to_mover_write_mem_cmd_in.empty()) {
        inner_cmd = to_mover_write_mem_cmd_in.read();
        logger.Info(LOG_MODULE, "WriteMem Req", inner_cmd.to_string());
        // Compute the address of the last byte to write
        data_mver_cmd = DataMoverCmd(inner_cmd.addr, inner_cmd.length);

        // The remaining buffer space is not enough. need to breakdown cmd
        if (inner_cmd.next_addr.bit(WINDOW_BITS)) {
          // Compute how much bytes are needed in the first transaction
          data_mver_cmd.bbt = BUFFER_SIZE - inner_cmd.addr(WINDOW_BITS - 1, 0);
          // Determines the position of the MSB in the last word
          // equals to bbt % 64
          byte_offset   = data_mver_cmd.bbt.range(5, 0);
          first_cmd_bbt = data_mver_cmd.bbt;
          // Get the keep of the last transaction of  the first memory offset;
          first_cmd_last_beat_keep = DataLengthToAxisKeep(byte_offset);
          // Determines how many transaction are in the first memory access
          if (byte_offset != 0) {
            first_cmd_trans_beats = data_mver_cmd.bbt.range(WINDOW_BITS - 1, 6) + 1;
          } else {
            first_cmd_trans_beats = data_mver_cmd.bbt.range(WINDOW_BITS - 1, 6);
          }
          rx_or_tx_write_break_down = true;
          first_cmd_trans_beats_cnt = 1;
          fsm_state                 = FWD_BREAKDOWN_0;
        } else {
          rx_or_tx_write_break_down = false;
          fsm_state                 = FWD_NO_BREAKDOWN;
        }
        // set the breakdown flag
        mem_buffer_double_access_flag.write(rx_or_tx_write_break_down);
        // issue the first command
        mover_write_mem_cmd_out.write(data_mver_cmd);
        logger.Info(LOG_MODULE, DATA_MVER, "WriteMem Cmd1", data_mver_cmd.to_string());
      }
      break;
    case FWD_NO_BREAKDOWN:
      if (!from_rx_or_tx_write_data.empty()) {
        from_rx_or_tx_write_data.read(cur_beat_word);

        if (cur_beat_word.last) {
          fsm_state = WAIT_CMD;
        }
        mover_write_mem_data_out.write(cur_beat_word.to_net_axis());
        logger.Info(LOG_MODULE, DATA_MVER, "Transfer Data ALIGNED", cur_beat_word.to_string());
        logger.Info(DATA_MVER, MOCK_MEMY, "WriteMem Data ALIGNED", cur_beat_word.to_string());
      }
      break;
    case FWD_BREAKDOWN_0:
      // the first cmd write state
      if (!from_rx_or_tx_write_data.empty()) {
        from_rx_or_tx_write_data.read(cur_beat_word);
        logger.Info(LOG_MODULE, DATA_MVER, "Transfer Data BREAK0", cur_beat_word.to_string());
        send_beat_word.last = 0;
        if (first_cmd_trans_beats_cnt == first_cmd_trans_beats) {
          send_beat_word.data = cur_beat_word.data;
          // last beat in first trans length = byte_offset
          send_beat_word.keep = first_cmd_last_beat_keep;
          send_beat_word.last = 1;
          // the session buffer start address
          data_mver_cmd.saddr(31, WINDOW_BITS)    = inner_cmd.addr(31, WINDOW_BITS);
          data_mver_cmd.saddr(WINDOW_BITS - 1, 0) = 0;

          // Recompute the bytes to transfer in the second memory access
          data_mver_cmd.bbt = inner_cmd.length - first_cmd_bbt;
          // Issue the second command

          mover_write_mem_cmd_out.write(data_mver_cmd);
          logger.Info(LOG_MODULE, DATA_MVER, "WriteMem Cmd2", data_mver_cmd.to_string());
          if (cur_beat_word.last) {
            fsm_state = FWD_EXTRA;
          } else {
            fsm_state = FWD_BREAKDOWN_1;
          }
        } else {
          send_beat_word = cur_beat_word;
        }
        first_cmd_trans_beats_cnt++;
        prev_beat_word = cur_beat_word;
        mover_write_mem_data_out.write(send_beat_word.to_net_axis());
        logger.Info(DATA_MVER, MOCK_MEMY, "WriteMem Data BREAK0", send_beat_word.to_string());
      }
      break;
    case FWD_BREAKDOWN_1:
      // the second cmd write state
      if (!from_rx_or_tx_write_data.empty()) {
        from_rx_or_tx_write_data.read(cur_beat_word);
        logger.Info(LOG_MODULE, DATA_MVER, "Transfer Data BREAK1", cur_beat_word.to_string());

        ConcatTwoWords(cur_beat_word, prev_beat_word, byte_offset, send_beat_word);

        send_beat_word.last = 0;
        if (cur_beat_word.last) {
          if (cur_beat_word.keep.bit(byte_offset) && byte_offset != 0) {
            fsm_state = FWD_EXTRA;
          } else {
            fsm_state           = WAIT_CMD;
            send_beat_word.last = 1;
          }
        }
        prev_beat_word = cur_beat_word;
        mover_write_mem_data_out.write(send_beat_word.to_net_axis());
        logger.Info(DATA_MVER, MOCK_MEMY, "WriteMem Data BREAK1", send_beat_word.to_string());
      }
      break;
    case FWD_EXTRA:
      // The EXTRA part of the memory write has less than 64 bytes
      ConcatTwoWords(NetAXISWord(), prev_beat_word, byte_offset, send_beat_word);
      send_beat_word.last = 1;
      mover_write_mem_data_out.write(send_beat_word.to_net_axis());
      logger.Info(DATA_MVER, MOCK_MEMY, "WriteMem Data EXTRA", send_beat_word.to_string());

      fsm_state = WAIT_CMD;
      break;
  }
}
// rx=1, tx=0
template <int is_rx>
void        GetSessionMemAddr(const TcpSessionID &    id,
                              const TcpSessionBuffer &app_rw,
                              ap_uint<32> &           session_mem_addr) {
#pragma HLS INLINE
  session_mem_addr[31] = (is_rx == 1) ? 0 : (!TCP_RX_DDR_BYPASS);
  // only use the least significant bits in session id
  session_mem_addr(30, WINDOW_BITS)    = id((30 - WINDOW_BITS), 0);
  session_mem_addr(WINDOW_BITS - 1, 0) = app_rw(WINDOW_BITS - 1, 0);
}
#endif