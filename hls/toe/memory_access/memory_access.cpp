#include "memory_access.hpp"
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger    logger;
void                 TxEngReadDataSendCmd(stream<MemBufferRWCmd> &            tx_eng_to_mem_cmd_in,
                                          stream<DataMoverCmd> &              mover_read_mem_cmd_out,
                                          stream<MemBufferRWCmdDoubleAccess> &mem_buffer_double_access) {
#pragma HLS pipeline II = 1
#pragma HLS INLINE   off

#pragma HLS INTERFACE axis port = mover_read_mem_cmd_out
#pragma HLS aggregate variable = mover_read_mem_cmd_out compact = bit

  static bool                read_cmd_is_breakdown = false;
  static MemBufferRWCmd      buf_rw_cmd;
  static ap_uint<16>         first_cmd_bbt = 0;
  DataMoverCmd               to_mem_read_cmd;
  MemBufferRWCmdDoubleAccess double_access_cmd;

  if (read_cmd_is_breakdown == false) {
    if (!tx_eng_to_mem_cmd_in.empty()) {
      tx_eng_to_mem_cmd_in.read(buf_rw_cmd);
      logger.Info(TX_ENGINE, TX_ENGINE, "ReadMem Req", buf_rw_cmd.to_string());
      to_mem_read_cmd = DataMoverCmd(buf_rw_cmd.addr, buf_rw_cmd.length);
      // Check for overflow
      if (buf_rw_cmd.next_addr.bit(WINDOW_BITS)) {
        // Compute remaining space in the buffer before overflow
        first_cmd_bbt   = BUFFER_SIZE - buf_rw_cmd.addr;
        to_mem_read_cmd = DataMoverCmd(buf_rw_cmd.addr, first_cmd_bbt);

        read_cmd_is_breakdown           = true;
        double_access_cmd.double_access = true;
        // Offset of MSB byte valid in the last beat of the first trans
        double_access_cmd.byte_offset = first_cmd_bbt(5, 0);
      }
      mover_read_mem_cmd_out.write(to_mem_read_cmd);
      mem_buffer_double_access.write(double_access_cmd);
      logger.Info(TX_ENGINE, DATA_MVER, "ReadMem Cmd0", to_mem_read_cmd.to_string());
      logger.Info(TX_ENGINE, TX_ENGINE, "ReadMem Double? ", double_access_cmd.to_string());
    }
  } else if (read_cmd_is_breakdown == true) {
    // Clear the least significant bits of the address
    buf_rw_cmd.addr.range(WINDOW_BITS - 1, 0) = 0;
    to_mem_read_cmd = DataMoverCmd(buf_rw_cmd.addr, buf_rw_cmd.length - first_cmd_bbt);
    mover_read_mem_cmd_out.write(to_mem_read_cmd);
    logger.Info(TX_ENGINE, DATA_MVER, "ReadMem Cmd1", to_mem_read_cmd.to_string());
    read_cmd_is_breakdown = false;
  }
}

/** @ingroup tx_engine
 *  See app_MemDataRead_aligner description
 */
void TxEngReadDataFromMem(stream<NetAXIS> &                   mover_read_mem_data_in,
                          stream<MemBufferRWCmdDoubleAccess> &mem_buffer_double_access,
#if (TCP_NODELAY)
                          stream<bool> &   tx_eng_is_ddr_bypass,
                          stream<NetAXIS> &tx_eng_read_data_in,
#endif
                          stream<NetAXISWord> &to_tx_eng_read_data) {
#pragma HLS INLINE   off
#pragma HLS pipeline II = 1

#pragma HLS INTERFACE axis port = mover_read_mem_data_in
#if (TCP_NODELAY)
#pragma HLS INTERFACE axis port = tx_eng_read_data_in
#endif
  enum TxEngReadDataFsmState {
    READ_ACCESS,
    NO_BREAKDOWN,
    BREAKDOWN_BLOCK_0,
    BREAKDOWN_ALIGNED,
    FIRST_MERGE,
    BREAKDOWN_BLOCK_1,
    EXTRA_DATA
#if (TCP_NODELAY)
    ,
    NO_USE_DDR,
    READ_BYPASS
#endif
  };

#if (TCP_NODELAY)
  const TxEngReadDataFsmState INITIAL_STATE = READ_BYPASS;
#else
  const TxEngReadDataFsmState INITIAL_STATE = READ_ACCESS;
#endif
  static TxEngReadDataFsmState fsm_state = INITIAL_STATE;

  static ap_uint<6> offset_block0;
  static ap_uint<6> offset_block1;

  MemBufferRWCmdDoubleAccess mem_double_access;
  static NetAXISWord         prev_beat_word;
  // NetAXIS                    read_mem_data;
  NetAXISWord cur_beat_word;
  NetAXISWord send_beat_word;

  bool is_ddr_bypass;
  bool to_tx_eng_word_need_to_align = true;

  switch (fsm_state) {
#if (TCP_NODELAY)
    case READ_BYPASS:
      if (!tx_eng_is_ddr_bypass.empty()) {
        tx_eng_is_ddr_bypass.read(is_ddr_bypass);
        if (is_ddr_bypass) {
          fsm_state = NO_USE_DDR;
        } else {
          fsm_state = READ_ACCESS;
        }
      }
      break;
    case NO_USE_DDR:
      if (!tx_eng_read_data_in.empty() && !to_tx_eng_read_data.full()) {
        tx_eng_read_data_in.read(cur_beat_word);
        to_tx_eng_read_data.write(cur_beat_word);
        if (cur_beat_word.last) {
          fsm_state = READ_BYPASS;
        }
      }
      break;
#endif
    case READ_ACCESS:
      if (!mem_buffer_double_access.empty()) {
        mem_buffer_double_access.read(mem_double_access);
        offset_block0 = mem_double_access.byte_offset;
        offset_block1 = 64 - mem_double_access.byte_offset;
        logger.Info(TX_ENGINE, TX_ENGINE, "Offset0", offset_block0.to_string(16));
        logger.Info(TX_ENGINE, TX_ENGINE, "Offset1", offset_block1.to_string(16));

        if (mem_double_access.double_access) {
          fsm_state = BREAKDOWN_BLOCK_0;
        } else {
          fsm_state = NO_BREAKDOWN;
        }
      }
      break;
    case NO_BREAKDOWN:
    case BREAKDOWN_ALIGNED:
      if (!mover_read_mem_data_in.empty() && !to_tx_eng_read_data.full()) {
        cur_beat_word = mover_read_mem_data_in.read();
        to_tx_eng_read_data.write(cur_beat_word);
        logger.Info(MOCK_MEMY, DATA_MVER, "ReadMemData ALIGNED", cur_beat_word.to_string());
        logger.Info(DATA_MVER, TX_ENGINE, "PayloadData ALIGNED", cur_beat_word.to_string());

        if (cur_beat_word.last) {
          fsm_state = INITIAL_STATE;
        }
      }
      break;
    case BREAKDOWN_BLOCK_0:
      if (!mover_read_mem_data_in.empty() && !to_tx_eng_read_data.full()) {
        cur_beat_word = mover_read_mem_data_in.read();
        logger.Info(MOCK_MEMY, DATA_MVER, "ReadMemData BREAK0", cur_beat_word.to_string());

        send_beat_word.data = cur_beat_word.data;
        send_beat_word.keep = cur_beat_word.keep;
        send_beat_word.last = 0;
        if (cur_beat_word.last) {
          if (offset_block0 == 0) {
            to_tx_eng_word_need_to_align = false;
            fsm_state                    = BREAKDOWN_ALIGNED;
          } else {  // This mean that the last not has all byte valid
            to_tx_eng_word_need_to_align = true;
            fsm_state                    = FIRST_MERGE;
          }
        }
        if (!to_tx_eng_word_need_to_align) {
          to_tx_eng_read_data.write(send_beat_word);
          logger.Info(DATA_MVER, TX_ENGINE, "PayloadData BREAK0", send_beat_word.to_string());
        }

        prev_beat_word = cur_beat_word;
      }
      break;
    case FIRST_MERGE:
      if (!mover_read_mem_data_in.empty() && !to_tx_eng_read_data.full()) {
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

        to_tx_eng_read_data.write(send_beat_word);
        logger.Info(DATA_MVER, TX_ENGINE, "PayloadData MERGE", send_beat_word.to_string());
      }
      break;
    case BREAKDOWN_BLOCK_1:
      if (!mover_read_mem_data_in.empty() && !to_tx_eng_read_data.full()) {
        cur_beat_word = mover_read_mem_data_in.read();
        logger.Info(DATA_MVER, TX_ENGINE, "PayloadData BREAK1", send_beat_word.to_string());

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
        to_tx_eng_read_data.write(send_beat_word);
        logger.Info(DATA_MVER, TX_ENGINE, "PayloadData BREAK1", send_beat_word.to_string());
      }
      break;
    case EXTRA_DATA:
      if (!to_tx_eng_read_data.full()) {
        ConcatTwoWords(NetAXISWord(), prev_beat_word, offset_block1, send_beat_word);
        send_beat_word.last = 1;

        to_tx_eng_read_data.write(send_beat_word);
        logger.Info(DATA_MVER, TX_ENGINE, "PayloadData EXTRA", send_beat_word.to_string());

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
 * @param      mover_write_mem_cmd_out   Command to the data mover. It takes into account buffer
 * overflow. Two write commands when buffer overflows
 * @param      mover_write_mem_data_out  Data to memory. If the buffer overflows, the second part
 * of the data has to be realigned
 */
// TODO: use MemBufferRWCMD struct zelin 220511
void                 TxAppWriteDataToMem(stream<NetAXISWord> & tx_app_to_mem_data_in,
                                         stream<DataMoverCmd> &tx_app_to_mem_cmd_in,
                                         stream<NetAXIS> &     mover_write_mem_data_out,
                                         stream<DataMoverCmd> &mover_write_mem_cmd_out) {
#pragma HLS pipeline II = 1
#pragma HLS INLINE   off
// in data + cmd
#pragma HLS aggregate variable = tx_app_to_mem_data_in compact = bit
#pragma HLS aggregate variable = tx_app_to_mem_cmd_in compact = bit
// out data + cmd
#pragma HLS INTERFACE axis port = mover_write_mem_data_out
#pragma HLS INTERFACE axis port = mover_write_mem_cmd_out
#pragma HLS aggregate variable = mover_write_mem_cmd_out compact = bit

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
  static ap_uint<64> first_cmd_last_beat_keep;
  // first word want to transfer several beats
  static ap_uint<10> first_cmd_trans_beats;
  static ap_uint<10> first_cmd_trans_beats_cnt = 1;

  static DataMoverCmd tx_app_cmd;
  static DataMoverCmd data_mver_cmd;

  ap_uint<WINDOW_BITS + 1> buffer_overflow_addr;

  NetAXISWord        cur_beat_word;
  NetAXISWord        send_beat_word;
  static NetAXISWord prev_beat_word;

  switch (fsm_state) {
    case WAIT_CMD:
      if (!tx_app_to_mem_cmd_in.empty()) {
        tx_app_to_mem_cmd_in.read(tx_app_cmd);
        logger.Info(TX_APP_IF, TX_APP_IF, "WriteMem Req", tx_app_cmd.to_string());
        // Compute the address of the last byte to write
        buffer_overflow_addr = tx_app_cmd.saddr(WINDOW_BITS - 1, 0) + tx_app_cmd.bbt;
        // The remaining buffer space is not enough. need to breakdown cmd
        if (buffer_overflow_addr.bit(WINDOW_BITS)) {
          // Compute how much bytes are needed in the first transaction
          data_mver_cmd.bbt   = BUFFER_SIZE - tx_app_cmd.saddr(WINDOW_BITS - 1, 0);
          data_mver_cmd.saddr = tx_app_cmd.saddr;
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
          first_cmd_trans_beats_cnt = 1;
          fsm_state                 = FWD_BREAKDOWN_0;
        } else {
          data_mver_cmd = tx_app_cmd;
          fsm_state     = FWD_NO_BREAKDOWN;
        }
        // issue the first command
        mover_write_mem_cmd_out.write(data_mver_cmd);
        logger.Info(TX_APP_IF, DATA_MVER, "WriteMem Cmd1", data_mver_cmd.to_string());
      }
      break;
    case FWD_NO_BREAKDOWN:
      if (!tx_app_to_mem_data_in.empty()) {
        tx_app_to_mem_data_in.read(cur_beat_word);

        if (cur_beat_word.last) {
          fsm_state = WAIT_CMD;
        }
        mover_write_mem_data_out.write(cur_beat_word.to_net_axis());
        logger.Info(TX_APP_IF, DATA_MVER, "Transfer Data ALIGNED", cur_beat_word.to_string());
        logger.Info(DATA_MVER, MOCK_MEMY, "WriteMem Data ALIGNED", cur_beat_word.to_string());
      }
      break;
    case FWD_BREAKDOWN_0:
      // the first cmd write state
      if (!tx_app_to_mem_data_in.empty()) {
        tx_app_to_mem_data_in.read(cur_beat_word);
        logger.Info(TX_APP_IF, DATA_MVER, "Transfer Data BREAK0", cur_beat_word.to_string());
        send_beat_word.last = 0;
        if (first_cmd_trans_beats_cnt == first_cmd_trans_beats) {
          send_beat_word.data = cur_beat_word.data;
          // last beat in first trans length = byte_offset
          send_beat_word.keep = first_cmd_last_beat_keep;
          send_beat_word.last = 1;
          // the session buffer start address
          data_mver_cmd.saddr(31, WINDOW_BITS)    = tx_app_cmd.saddr(31, WINDOW_BITS);
          data_mver_cmd.saddr(WINDOW_BITS - 1, 0) = 0;

          // Recompute the bytes to transfer in the second memory access
          data_mver_cmd.bbt = tx_app_cmd.bbt - first_cmd_bbt;
          // Issue the second command

          mover_write_mem_cmd_out.write(data_mver_cmd);
          logger.Info(TX_APP_IF, DATA_MVER, "WriteMem Cmd2", data_mver_cmd.to_string());
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
      if (!tx_app_to_mem_data_in.empty()) {
        tx_app_to_mem_data_in.read(cur_beat_word);
        logger.Info(TX_APP_IF, DATA_MVER, "Transfer Data BREAK1", cur_beat_word.to_string());

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

void        GetSessionMemAddr(const TcpSessionID &    id,
                              const TcpSessionBuffer &app_rw,
                              // true = rx, false = tx
                              bool         is_rx_or_tx,
                              ap_uint<32> &session_mem_addr) {
#pragma HLS INLINE
  session_mem_addr[31] = is_rx_or_tx ? 0 : (!TCP_RX_DDR_BYPASS);
  // only use the least significant bits in session id
  session_mem_addr(30, WINDOW_BITS)    = id((30 - WINDOW_BITS), 0);
  session_mem_addr(WINDOW_BITS - 1, 0) = app_rw(WINDOW_BITS - 1, 0);
}