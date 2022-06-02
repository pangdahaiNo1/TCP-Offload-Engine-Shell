#include "port_table.hpp"

#include "toe/toe_config.hpp"
// logger
#include "toe/mock/mock_logger.hpp"
extern MockLogger logger;

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
void                 ListeningPortTable(stream<ListenPortReq> &   rx_app_to_ptable_listen_port_req,
                                        stream<ap_uint<15> > &    ptable_check_listening_req_fifo,
                                        stream<ListenPortRsp> &   ptable_to_rx_app_listen_port_rsp,
                                        stream<PtableToRxEngRsp> &ptable_check_listening_rsp_fifo) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static PortTableEntry listening_port_table[LISTENING_PORT_CNT];
#pragma HLS bind_storage variable = listening_port_table type = RAM_T2P impl = BRAM
#pragma HLS DEPENDENCE variable = listening_port_table inter false

  ListenPortReq curr_req;
  ap_uint<15>   check_port_15;
  ListenPortRsp listen_rsp;

  if (!rx_app_to_ptable_listen_port_req.empty()) {
    // check range, TODO make sure currPort is not equal in 2 consecutive cycles
    rx_app_to_ptable_listen_port_req.read(curr_req);
    logger.Info("Rx App to Ptable", curr_req.to_string(), false);

    listen_rsp.data.wrong_port_number = (curr_req.data.bit(15));
    listen_rsp.data.port_number       = curr_req.data;

    PortTableEntry cur_entry              = listening_port_table[curr_req.data(14, 0)];
    bool           cur_entry_port_is_open = cur_entry.is_open;
    if ((cur_entry_port_is_open == false) && !listen_rsp.data.wrong_port_number) {
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
    ptable_check_listening_rsp_fifo.write(PtableToRxEngRsp(
        listening_port_table[check_port_15].is_open, listening_port_table[check_port_15].role_id));
  }
}
/** @ingroup port_table
 *  Assumption: We are never going to run out of free ports, since 10K session
 * <<< 32K ports
 * rxEng: read
 * txApp: pt_cursor: read -> write
 * sLookup: write If a free port is found it is written into @ptable_to_tx_app_rsp and cached
 * until @ref tx_app_stream_if reads it out
 *  @param[in]		slookup_to_ptable_release_port_req_req
 *  @param[in]		ptable_check_used_req_fifo
 *  @param[out]		ptable_check_used_rsp_fifo
 *  @param[out]		ptable_to_tx_app_rsp
 */
void                 FreePortTable(stream<TcpPortNumber> &   slookup_to_ptable_release_port_req_req,
                                   stream<ap_uint<15> > &    ptable_check_used_req_fifo,
                                   stream<NetAXISDest> &     tx_app_to_ptable_req,
                                   stream<PtableToRxEngRsp> &ptable_check_used_rsp_fifo,
                                   stream<TcpPortNumber> &   ptable_to_tx_app_rsp) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static PortTableEntry free_port_table[FREE_PORT_CNT];
#pragma HLS bind_storage variable = free_port_table type = RAM_T2P impl = BRAM
#pragma HLS DEPENDENCE variable = free_port_table inter false

  static ap_uint<15> pt_cursor = 0;
#pragma HLS RESET variable = pt_cursor

  TcpPortNumber slup_port_req;
  TcpPortNumber cur_free_port;
  ap_uint<15>   port_check_used;
  NetAXISDest   tx_app_tdest;

  if (!slookup_to_ptable_release_port_req_req.empty()) {
    slookup_to_ptable_release_port_req_req.read(slup_port_req);
    if (slup_port_req.bit(15)) {
      free_port_table[slup_port_req(14, 0)].is_open = false;  // shift
    }
  } else if (!ptable_check_used_req_fifo.empty()) {
    ptable_check_used_req_fifo.read(port_check_used);
    ptable_check_used_rsp_fifo.write(PtableToRxEngRsp(free_port_table[port_check_used].is_open,
                                                      free_port_table[port_check_used].role_id));
  } else if (!tx_app_to_ptable_req.empty()) {
    tx_app_tdest = tx_app_to_ptable_req.read();
    logger.Info("Tx App to Ptable", tx_app_tdest.to_string(16), false);
    if (!free_port_table[pt_cursor].is_open && !ptable_to_tx_app_rsp.full()) {
      cur_free_port(14, 0)               = pt_cursor;
      cur_free_port[15]                  = 1;
      free_port_table[pt_cursor].is_open = true;
      free_port_table[pt_cursor].role_id = tx_app_tdest;
      ptable_to_tx_app_rsp.write(cur_free_port);
    }
    // only receive a tx app request, then increase the port cursor
    pt_cursor++;
  }
}

void                 CheckInMultiplexer(stream<TcpPortNumber> &rx_eng_to_ptable_check_req,
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
  if (!rx_eng_to_ptable_check_req.empty()) {
    rx_eng_to_ptable_check_req.read(req_check_port);
    // request port is swapped
    logger.Info("Ptable check port", req_check_port.to_string(16), false);
    req_swapped_check_port = (req_check_port);

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
void                 CheckOutMultiplexer(stream<bool> &            ptable_check_dst_fifo_in,
                                         stream<PtableToRxEngRsp> &ptable_check_listening_rsp_fifo,
                                         stream<PtableToRxEngRsp> &ptable_check_used_rsp_fifo,
                                         stream<PtableToRxEngRsp> &ptable_to_rx_eng_check_rsp) {
#pragma HLS PIPELINE II = 1
#pragma HLS INLINE   off

  static const bool dst_is_listening_table = true;
  static const bool dst_is_free_table      = false;

  static bool      dst = dst_is_listening_table;
  PtableToRxEngRsp listening_table_rsp;
  PtableToRxEngRsp free_table_rsp;

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
        logger.Info(
            "Ptable check port from listen_port rsp", listening_table_rsp.to_string(), false);
        ptable_to_rx_eng_check_rsp.write(listening_table_rsp);
        cm_fsmState = READ_DST;
      }

      break;
    case READ_USED:
      if (!ptable_check_used_rsp_fifo.empty()) {
        ptable_check_used_rsp_fifo.read(free_table_rsp);
        logger.Info("Ptable check port from free_port rsp", free_table_rsp.to_string(), false);
        ptable_to_rx_eng_check_rsp.write(free_table_rsp);
        cm_fsmState = READ_DST;
      }

      break;
  }
}

/** @ingroup port_table
 *  The @ref port_table contains an array of 65536 entries, one for each port
 * number. It receives passive opening (listening) request from @ref rx_app_intf,
 * Request to check if the port is open from the @ref rx_engine and requests for
 * a free port from the @ref tx_app_if.
 */
void port_table(
    // from other module req
    stream<TcpPortNumber> &slookup_to_ptable_release_port_req,
    // rx app listen port req/rsp
    stream<ListenPortReq> &rx_app_to_ptable_listen_port_req,
    stream<ListenPortRsp> &ptable_to_rx_app_listen_port_rsp,
    // rx eng check req/rsp
    stream<TcpPortNumber> &   rx_eng_to_ptable_check_req,
    stream<PtableToRxEngRsp> &ptable_to_rx_eng_check_rsp,
    // tx app get avail port req/rsp
    stream<NetAXISDest> &  tx_app_to_ptable_req,
    stream<TcpPortNumber> &ptable_to_tx_app_rsp) {
#pragma HLS DATAFLOW
  /*
   * Fifos necessary for multiplexing Check requests
   */
  static stream<ap_uint<15> > ptable_check_listening_req_fifo("ptable_check_listening_req_fifo");
  static stream<ap_uint<15> > ptable_check_used_req_fifo("ptable_check_used_req_fifo");
#pragma HLS STREAM variable = ptable_check_listening_req_fifo depth = 8
#pragma HLS STREAM variable = ptable_check_used_req_fifo depth = 8

  static stream<PtableToRxEngRsp> ptable_check_listening_rsp_fifo(
      "ptable_check_listening_rsp_fifo");
  static stream<PtableToRxEngRsp> ptable_check_used_rsp_fifo("ptable_check_used_rsp_fifo");
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
  FreePortTable(slookup_to_ptable_release_port_req,
                ptable_check_used_req_fifo,
                tx_app_to_ptable_req,
                ptable_check_used_rsp_fifo,
                ptable_to_tx_app_rsp);

  /*
   * Multiplex this query
   */
  CheckInMultiplexer(rx_eng_to_ptable_check_req,
                     ptable_check_listening_req_fifo,
                     ptable_check_used_req_fifo,
                     ptable_check_dst_fifo);

  CheckOutMultiplexer(ptable_check_dst_fifo,
                      ptable_check_listening_rsp_fifo,
                      ptable_check_used_rsp_fifo,
                      ptable_to_rx_eng_check_rsp);
}
