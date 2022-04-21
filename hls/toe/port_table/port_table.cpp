#include "port_table.hpp"

#include "toe/toe_config.hpp"

using namespace hls;

/** @ingroup port_table
 *  rxEng and txApp are accessing this table:
 *  rxEng: read
 *  txApp: read -> write
 *  If read and write operation on same address occur at the same time,
 *  read should get the old value, either way it doesn't matter
 *  @param[in]		rx_app_to_ptable_listen_port_req
 *  @param[in]		ptable_check_listening_req_fifo
 *  @param[out]		ptable_to_rx_app_listen_port_rsp
 *  @param[out]		ptable_check_listening_rsp_fifo
 */
void ListeningPortTable(stream<NetAXISListenPortReq> &rx_app_to_ptable_listen_port_req,
                        stream<ap_uint<15> > &        ptable_check_listening_req_fifo,
                        stream<NetAXISListenPortRsp> &ptable_to_rx_app_listen_port_rsp,
                        stream<bool> &                ptable_check_listening_rsp_fifo) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static PortTableEntry listening_port_table[LISTENING_PORT_CNT];
#pragma HLS RESOURCE variable = listening_port_table core = RAM_T2P_BRAM
#pragma HLS DEPENDENCE variable                           = listening_port_table inter false

  NetAXISListenPortReq curr_req;
  ap_uint<15>          check_port_15;
  NetAXISListenPortRsp listen_rsp;

  if (!rx_app_to_ptable_listen_port_req.empty()) {
    // check range, TODO make sure currPort is not equal in 2 consecutive cycles
    rx_app_to_ptable_listen_port_req.read(curr_req);

    listen_rsp.data.wrong_port_number = (curr_req.data.bit(15));
    listen_rsp.data.port_number       = curr_req.data;

    PortTableEntry cur_entry = listening_port_table[curr_req.data(14, 0)];
    if ((cur_entry.is_open == false) && !listen_rsp.data.wrong_port_number) {
      listening_port_table[curr_req.data(14, 0)].is_open = true;
      listening_port_table[curr_req.data(14, 0)].role_id = curr_req.dest;

      listen_rsp.data.already_open      = false;
      listen_rsp.data.open_successfully = true;
    } else {
      listen_rsp.data.already_open      = true;
      listen_rsp.data.open_successfully = false;
    }
    listen_rsp.dest = curr_req.dest;
    ptable_to_rx_app_listen_port_rsp.write(listen_rsp);
  } else if (!ptable_check_listening_req_fifo.empty()) {
    ptable_check_listening_req_fifo.read(check_port_15);
    ptable_check_listening_rsp_fifo.write(listening_port_table[check_port_15].is_open);
  }
}
/** @ingroup port_table
 *  Assumption: We are never going to run out of free ports, since 10K session
 * <<< 32K ports
 * rxEng: read
 * txApp: pt_cursor: read -> write
 * sLookup: write If a free port is found it is written into @ptable_to_tx_app_port_rsp and cached
 * until @ref tx_app_stream_if reads it out
 *  @param[in]		slup_to_ptable_realease_port_req
 *  @param[in]		ptable_check_used_req_fifo
 *  @param[out]		ptable_check_used_rsp_fifo
 *  @param[out]		ptable_to_tx_app_port_rsp
 */
void                 FreePortTable(stream<TcpPortNumber> &slup_to_ptable_realease_port_req,
                                   stream<ap_uint<15> > & ptable_check_used_req_fifo,
                                   stream<bool> &         ptable_check_used_rsp_fifo,
                                   stream<TcpPortNumber> &ptable_to_tx_app_port_rsp) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static bool free_port_table[FREE_PORT_CNT];
#pragma HLS RESOURCE variable = free_port_table core = RAM_T2P_BRAM
#pragma HLS DEPENDENCE variable                      = free_port_table inter false

  // Free Ports Cache
  // static stream<TcpPortNumber > pt_freePortsFifo("pt_freePortsFifo");
  //#pragma HLS STREAM variable=pt_freePortsFifo depth=8

  static ap_uint<15> pt_cursor = 0;
#pragma HLS RESET variable = pt_cursor

  TcpPortNumber slup_port_req;
  TcpPortNumber cur_free_port;
  ap_uint<15>   port_check_used;

  if (!slup_to_ptable_realease_port_req.empty()) {
    slup_to_ptable_realease_port_req.read(slup_port_req);
    if (slup_port_req.bit(15)) {
      free_port_table[slup_port_req(14, 0)] = false;  // shift
    }
  } else if (!ptable_check_used_req_fifo.empty()) {
    ptable_check_used_req_fifo.read(port_check_used);
    ptable_check_used_rsp_fifo.write(free_port_table[port_check_used]);
  } else {
    if (!free_port_table[pt_cursor] &&
        !ptable_to_tx_app_port_rsp.full()) {  // This is not perfect, but yeah
      cur_free_port(14, 0)       = pt_cursor;
      cur_free_port[15]          = 1;
      free_port_table[pt_cursor] = true;
      ptable_to_tx_app_port_rsp.write(cur_free_port);
    }
  }
  pt_cursor++;
}

void                 CheckInMultiplexer(stream<TcpPortNumber> &rx_eng_to_ptable_req,
                                        stream<ap_uint<15> > & ptable_check_listening_req_fifo,
                                        stream<ap_uint<15> > & ptable_check_used_req_fifo,
                                        stream<bool> &         ptable_check_dst_fifo_out) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static const bool dst_is_listening_table = true;
  static const bool dst_is_free_table      = false;
  static bool       dst                    = dst_is_listening_table;
  TcpPortNumber     req_check_port         = 0;
  TcpPortNumber     req_swapped_check_port = 0;

  // Forward request according to port number, store table to keep order
  if (!rx_eng_to_ptable_req.empty()) {
    rx_eng_to_ptable_req.read(req_check_port);
    req_swapped_check_port = 0;  // SwapByte<16>(req_check_port);

    if (!req_swapped_check_port.bit(15)) {
      ptable_check_listening_req_fifo.write(req_swapped_check_port);
      ptable_check_dst_fifo_out.write(dst_is_listening_table);
    } else {
      ptable_check_used_req_fifo.write(req_swapped_check_port);
      ptable_check_dst_fifo_out.write(dst_is_free_table);
    }
  }
}

/** @ingroup port_table
 *
 */
void                 CheckOutMultiplexer(stream<bool> &ptable_check_dst_fifo_in,
                                         stream<bool> &ptable_check_listening_rsp_fifo,
                                         stream<bool> &ptable_check_used_rsp_fifo,
                                         stream<bool> &ptable_to_rx_eng_check_rsp) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static const bool dst_is_listening_table = true;
  static const bool dst_is_free_table      = false;

  static bool dst = dst_is_listening_table;
  bool        listening_table_rsp;
  bool        free_table_rsp;

  // Read out responses from tables in order and merge them
  enum cmFsmStateType { READ_DST, READ_LISTENING, READ_USED };
  static cmFsmStateType cm_fsmState = READ_DST;
#pragma HLS reset variable = cm_fsmState

  switch (cm_fsmState) {
    case READ_DST:
      if (!ptable_check_dst_fifo_in.empty()) {
        ptable_check_dst_fifo_in.read(dst);
        if (dst == dst_is_listening_table) {
          cm_fsmState = READ_LISTENING;
        } else {
          cm_fsmState = READ_USED;
        }
      }
      break;
    case READ_LISTENING:
      if (!ptable_check_listening_rsp_fifo.empty()) {
        ptable_check_listening_rsp_fifo.read(listening_table_rsp);
        ptable_to_rx_eng_check_rsp.write(listening_table_rsp);
        cm_fsmState = READ_DST;
      }

      break;
    case READ_USED:
      if (!ptable_check_used_rsp_fifo.empty()) {
        ptable_check_used_rsp_fifo.read(free_table_rsp);
        ptable_to_rx_eng_check_rsp.write(free_table_rsp);
        cm_fsmState = READ_DST;
      }

      break;
  }
}

/** @ingroup port_table
 *  The @ref port_table contains an array of 65536 entries, one for each port
 * number. It receives passive opening (listening) request from @ref rx_app_if,
 * Request to check if the port is open from the @ref rx_engine and requests for
 * a free port from the @ref tx_app_if.
 *  @param[in]		rx_eng_to_ptable_req
 *  @param[in]		rx_app_to_ptable_listen_port_req
 *  @param[in]		txApp2portTable_req
 *  @param[in]		slup_to_ptable_realease_port
 *  @param[out]		ptable_to_rx_eng_check_rsp
 *  @param[out]		ptable_to_rx_app_listen_port_rsp
 *  @param[out]		portTable2txApp_rsp
 */
void        port_table(stream<TcpPortNumber> &       rx_eng_to_ptable_req,
                       stream<NetAXISListenPortReq> &rx_app_to_ptable_listen_port_req,
                       stream<TcpPortNumber> &       slup_to_ptable_realease_port,
                       stream<bool> &                ptable_to_rx_eng_check_rsp,
                       stream<NetAXISListenPortRsp> &ptable_to_rx_app_listen_port_rsp,
                       stream<TcpPortNumber> &       ptable_to_tx_app_port_rsp) {
#pragma HLS DATAFLOW
  // #pragma HLS INLINE

  /*
   * Fifos necessary for multiplexing Check requests
   */
  static stream<ap_uint<15> > ptable_check_listening_req_fifo("ptable_check_listening_req_fifo");
  static stream<ap_uint<15> > ptable_check_used_req_fifo("ptable_check_used_req_fifo");
#pragma HLS STREAM variable = ptable_check_listening_req_fifo depth = 8
#pragma HLS STREAM variable = ptable_check_used_req_fifo depth = 8

  static stream<bool> ptable_check_listening_rsp_fifo("ptable_check_listening_rsp_fifo");
  static stream<bool> ptable_check_used_rsp_fifo("ptable_check_used_rsp_fifo");
#pragma HLS STREAM variable = ptable_check_listening_rsp_fifo depth = 8
#pragma HLS STREAM variable = ptable_check_used_rsp_fifo depth = 8

  static stream<bool> ptable_check_dst_fifo("ptable_check_dst_fifo");
#pragma HLS STREAM variable = ptable_check_dst_fifo depth = 32

  /*
   * Listening PortTable
   */
  ListeningPortTable(rx_app_to_ptable_listen_port_req,
                     ptable_check_listening_req_fifo,
                     ptable_to_rx_app_listen_port_rsp,
                     ptable_check_listening_rsp_fifo);

  /*
   * Free PortTable
   */
  FreePortTable(slup_to_ptable_realease_port,
                ptable_check_used_req_fifo,
                ptable_check_used_rsp_fifo,
                ptable_to_tx_app_port_rsp);

  /*
   * Multiplex this query
   */
  CheckInMultiplexer(rx_eng_to_ptable_req,
                     ptable_check_listening_req_fifo,
                     ptable_check_used_req_fifo,
                     ptable_check_dst_fifo);

  CheckOutMultiplexer(ptable_check_dst_fifo,
                      ptable_check_listening_rsp_fifo,
                      ptable_check_used_rsp_fifo,
                      ptable_to_rx_eng_check_rsp);
}
