#include "toe/tx_engine/tx_engine.hpp"
// DONOT change the header files sequence
#include "toe/mock/mock_logger.hpp"
#include "toe/mock/mock_toe.hpp"
#include "toe/rx_engine/rx_engine.hpp"
#include "utils/axi_utils_test.hpp"
#include "utils/pcap/pcap_to_stream.hpp"
MockLogger logger("./tx_eng_inner.log", TX_ENGINE);

void TestTcpTxConstructIpv4Pkt(stream<NetAXISWord> &input_tcp_packet) {
  // open output file
  // std::ofstream outputFile;
  // outputFile.open("./out_tcp_pseudo_header.dat");
  // if (!outputFile) {
  //   std::cout << "Error: could not open test output file." << std::endl;
  // }
  // some fifos
  stream<NetAXISWord> input_tcp_packet_ip_header("input_tcp_packet_ip_header");
  stream<NetAXISWord> tx_tcp_pseudo_packet_for_checksum_fifo(
      "tx_tcp_pseudo_packet_for_checksum_fifo");
  stream<NetAXISWord>  tx_tcp_packet_fifo("tx_tcp_packet_fifo");
  stream<NetAXISWord>  tx_tcp_pseudo_packet_for_tx_eng_fifo("tx_tcp_pseudo_packet_for_tx_eng_fifo");
  stream<SubChecksum>  tcp_pseudo_packet_subchecksum_fifo("tcp_pseudo_packet_subchecksum_fifo");
  stream<ap_uint<16> > tcp_pseudo_packet_checksum_fifo("tcp_pseudo_packet_checksum_fifo");
  stream<NetAXIS>      tx_tcp_ip_packet_fifo("tx_tcp_ip_packet_fifo");

  stream<NetAXIS> input_tcp_packet_copy;
  stream<NetAXIS> input_tcp_packet_copy2;
  // simulation
  // construct ip header from input tcp packet
  int sim_cycle = 0;
  while (sim_cycle < 200) {
    // construct ip header from input tcp packet
    if (!input_tcp_packet.empty()) {
      NetAXISWord cur_word = input_tcp_packet.read();
      input_tcp_packet_copy.write(cur_word.to_net_axis());
      input_tcp_packet_copy2.write(cur_word.to_net_axis());
      NetAXISWord ip_word(cur_word.data(IPV4_HEADER_WIDTH - 1, 0), 0, 0xFFFFF, 1);
      input_tcp_packet_ip_header.write(ip_word);
      while (!cur_word.last) {
        cur_word = input_tcp_packet.read();
        input_tcp_packet_copy.write(cur_word.to_net_axis());
        input_tcp_packet_copy2.write(cur_word.to_net_axis());
      }
    }
    // construct tcp pseudo packet from input tcp packet
    RxEngTcpPseudoHeaderInsert(input_tcp_packet_copy,
                               tx_tcp_pseudo_packet_for_checksum_fifo,
                               tx_tcp_pseudo_packet_for_tx_eng_fifo);

    // the tcp pseudo packet input contains the valid checksum, then calculate checksum here will
    // make all tcp checksum = 0, if not zero, the testbench are failed
    ComputeSubChecksum<0>(tx_tcp_pseudo_packet_for_checksum_fifo,
                          tcp_pseudo_packet_subchecksum_fifo);
    CheckChecksum(tcp_pseudo_packet_subchecksum_fifo, tcp_pseudo_packet_checksum_fifo);
    TxEngRemovePseudoHeader(tx_tcp_pseudo_packet_for_tx_eng_fifo, tx_tcp_packet_fifo);
    TxEngConstructIpv4Packet(input_tcp_packet_ip_header,
                             tcp_pseudo_packet_checksum_fifo,
                             tx_tcp_packet_fifo,
                             tx_tcp_ip_packet_fifo);
    sim_cycle++;
    logger.SetSimCycle(sim_cycle);
  }
  ComparePacpPacketsWithGolden(tx_tcp_ip_packet_fifo, input_tcp_packet_copy2, true);
  // StreamToPcap("tx_ip_tcp.pcap", true, true, tx_tcp_ip_packet_fifo, true);
}

void TestRandom() {
  ap_uint<32> rand     = 0;
  uint32_t    init_val = 0x562301af;
  for (int i = 0; i < 10; i++) {
    init_val = (init_val * 8) ^ init_val;
    rand     = RandomVal();
    assert(init_val == rand.to_uint64());
    // cout << init_val << " " << rand.to_string(10) << endl;
  }
  return;
}

void EmptyTxEngFsmFifo(MockLogger &                    logger,
                       stream<ap_uint<1> > &           tx_eng_read_count_fifo,
                       stream<TcpSessionID> &          tx_eng_to_rx_sar_lup_req,
                       stream<TxEngToTxSarReq> &       tx_eng_to_tx_sar_req,
                       stream<TxEngToRetransTimerReq> &tx_eng_to_timer_set_rtimer,
                       stream<TcpSessionID> &          tx_eng_to_timer_set_ptimer,
                       // to session lookup req
                       stream<ap_uint<16> > &tx_eng_to_slookup_rev_table_req) {
  while (!tx_eng_read_count_fifo.empty()) {
    ap_uint<1> a = tx_eng_read_count_fifo.read();
    logger.Info(TX_ENGINE, ACK_DELAY, "Read One Event", "");
  }
  while (!tx_eng_to_rx_sar_lup_req.empty()) {
    auto req = tx_eng_to_rx_sar_lup_req.read();
    logger.Info(TX_ENGINE, RX_SAR_TB, "SAR Lup", req.to_string(16));
  }
  while (!tx_eng_to_tx_sar_req.empty()) {
    auto req = tx_eng_to_tx_sar_req.read();
    logger.Info(TX_ENGINE, TX_SAR_TB, "SAR Req", req.to_string());
  }
  while (!tx_eng_to_timer_set_rtimer.empty()) {
    auto req = tx_eng_to_timer_set_rtimer.read();
    logger.Info(TX_ENGINE, RTRMT_TMR, "Set RTimer", req.to_string());
  }
  while (!tx_eng_to_timer_set_ptimer.empty()) {
    auto req = tx_eng_to_timer_set_ptimer.read();
    logger.Info(TX_ENGINE, PROBE_TMR, "Set PTimer", req.to_string(16));
  }
  while (!tx_eng_to_slookup_rev_table_req.empty()) {
    auto req = tx_eng_to_slookup_rev_table_req.read();
    logger.Info(TX_ENGINE, SLUP_CTRL, "Four Tuple Lup", req.to_string(16));
  }
}

void EmptyTxEngDataFifo(MockLogger &logger,
                        // to datamover cmd
                        stream<DataMoverCmd> &tx_eng_to_mover_read_cmd,
                        // to outer
                        stream<NetAXIS> &tx_ip_pkt_out,
                        stream<NetAXIS> &tx_ip_pkt_to_save) {
  NetAXISWord  ip_pkt_out;
  DataMoverCmd cmd_out;

  while (!tx_eng_to_mover_read_cmd.empty()) {
    tx_eng_to_mover_read_cmd.read(cmd_out);
    logger.Info(TX_ENGINE, DATA_MVER, "ReadMem Cmd", cmd_out.to_string());
  }
  while (!tx_ip_pkt_out.empty()) {
    ip_pkt_out = tx_ip_pkt_out.read();
    logger.Info(TX_ENGINE, TOE_TOP, "Trans Seg", ip_pkt_out.to_string());
    tx_ip_pkt_to_save.write(ip_pkt_out.to_net_axis());
  }
}

void TestTxEngine() {
  stream<EventWithTuple> ack_delay_to_tx_eng_event;
  stream<ap_uint<1> >    tx_eng_read_count_fifo;
  // rx sar
  stream<TcpSessionID>   tx_eng_to_rx_sar_lup_req;
  stream<RxSarLookupRsp> rx_sar_to_tx_eng_lup_rsp;
  // tx sar
  stream<TxEngToTxSarReq> tx_eng_to_tx_sar_req;
  stream<TxSarToTxEngRsp> tx_sar_to_tx_eng_rsp;
  // timer
  stream<TxEngToRetransTimerReq> tx_eng_to_timer_set_rtimer;
  stream<TcpSessionID>           tx_eng_to_timer_set_ptimer;
  // to session lookup req/rsp
  stream<ap_uint<16> >           tx_eng_to_slookup_rev_table_req;
  stream<ReverseTableToTxEngRsp> slookup_rev_table_to_tx_eng_rsp;
  // to datamover cmd
  stream<DataMoverCmd> tx_eng_to_mover_read_cmd;
  // read data from data mem
  stream<NetAXIS> mover_to_tx_eng_read_data;
  // to outer
  stream<NetAXIS> tx_ip_pkt_out;
  // save output
  stream<NetAXIS> tx_ip_pkt_out_to_save;

  // req varibles
  EventWithTuple         event;
  ReverseTableToTxEngRsp slup_rsp;
  TxSarToTxEngRsp        tx_sar_rsp;
  RxSarLookupRsp         rx_sar_rsp;
  stream<NetAXIS>        rand_data;

  MockLogger top_logger("tx_eng.log", TX_ENGINE);
  int        sim_cycle       = 0;
  int        total_sim_cycle = 2000;
  while (sim_cycle < total_sim_cycle) {
    switch (sim_cycle) {
      case 1:
        // test1 SYN from mock-tuple
        event = EventWithTuple(Event(SYN, 0x1), FourTuple());
        ack_delay_to_tx_eng_event.write(event);
        break;
      case 2:
        slup_rsp.four_tuple = mock_tuple;
        slup_rsp.role_id    = 0x1;
        slookup_rev_table_to_tx_eng_rsp.write(slup_rsp);
        break;
      case 5:
        // test2 SYN-ACK to mock-tuple
        event = EventWithTuple(Event(SYN_ACK, 0x1), FourTuple());
        ack_delay_to_tx_eng_event.write(event);
        break;
      case 6:
        // when rx engine receive a SYN, acked in tx_sar set to 0
        rx_sar_rsp.recvd     = 0xABCDABCD;
        rx_sar_rsp.win_shift = 0x2;
        rx_sar_rsp.win_size  = -1;
        rx_sar_to_tx_eng_lup_rsp.write(rx_sar_rsp);
        tx_sar_rsp.ackd = 0;
        tx_sar_to_tx_eng_rsp.write(tx_sar_rsp);
        break;
      case 7:
        slup_rsp.four_tuple = reverse_mock_tuple;
        slup_rsp.role_id    = 0x1;
        slookup_rev_table_to_tx_eng_rsp.write(slup_rsp);
        break;
      case 8:
        // test3-ACK from mock-tuple
        event = EventWithTuple(Event(ACK, 0x1), FourTuple());
        ack_delay_to_tx_eng_event.write(event);
        break;
      case 9:
        // test3 ACK
        slup_rsp.four_tuple = mock_tuple;
        slup_rsp.role_id    = 0x1;
        slookup_rev_table_to_tx_eng_rsp.write(slup_rsp);
        break;
      case 10:
        rx_sar_rsp.recvd    = 0xABCDABCD;
        rx_sar_rsp.win_size = -1;
        rx_sar_to_tx_eng_lup_rsp.write(rx_sar_rsp);
        tx_sar_rsp.not_ackd = 0xABCDABCE;
        tx_sar_to_tx_eng_rsp.write(tx_sar_rsp);
        break;
      case 16:
        // test 4 Transmit a packet from mock tuple
        event = EventWithTuple(Event(TX, 0x1), FourTuple());
        ack_delay_to_tx_eng_event.write(event);

        slup_rsp.four_tuple = mock_tuple;
        slup_rsp.role_id    = 0x1;
        slookup_rev_table_to_tx_eng_rsp.write(slup_rsp);
        break;
      case 17:
        rx_sar_rsp.recvd    = 0xABCDABCD;
        rx_sar_rsp.win_size = -1;
        rx_sar_to_tx_eng_lup_rsp.write(rx_sar_rsp);
        tx_sar_rsp.ackd        = 0xABCDABCD;
        tx_sar_rsp.not_ackd    = 0xABCDABCE;
        tx_sar_rsp.app_written = 0xABCDABCE + 0xF1;
        tx_sar_rsp.curr_length =
            tx_sar_rsp.app_written(WINDOW_BITS - 1, 0) - tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0);
        tx_sar_rsp.used_length = tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0) - tx_sar_rsp.ackd;
        tx_sar_rsp.used_length_rst =
            tx_sar_rsp.not_ackd(WINDOW_BITS - 1, 0) - tx_sar_rsp.ackd - tx_sar_rsp.fin_is_sent;
        tx_sar_rsp.usable_window = 0xFFFF << 2 - tx_sar_rsp.used_length;
        tx_sar_rsp.min_window    = 0xFFFF;
        tx_sar_to_tx_eng_rsp.write(tx_sar_rsp);
        break;
      case 18:
        GenRandStream(0xF1, mover_to_tx_eng_read_data);
        break;
      case 20:
        // test 5 transmit a ACK to mock tuple
        event = EventWithTuple(Event(ACK, 0x1), FourTuple());
        ack_delay_to_tx_eng_event.write(event);

        slup_rsp.four_tuple = reverse_mock_tuple;
        slup_rsp.role_id    = 0x1;
        slookup_rev_table_to_tx_eng_rsp.write(slup_rsp);
        break;
      case 21:
        rx_sar_rsp.recvd    = 0xABCDABCE + 0xF1;
        rx_sar_rsp.win_size = -1;
        rx_sar_to_tx_eng_lup_rsp.write(rx_sar_rsp);
        tx_sar_rsp.not_ackd = 0xABCDABCE;
        tx_sar_to_tx_eng_rsp.write(tx_sar_rsp);
        break;
      case 25:
        // test 6 transmit a FIN-ACK from mock tuple
        event = EventWithTuple(Event(FIN, 0x1), FourTuple());
        ack_delay_to_tx_eng_event.write(event);

        slup_rsp.four_tuple = mock_tuple;
        slup_rsp.role_id    = 0x1;
        slookup_rev_table_to_tx_eng_rsp.write(slup_rsp);
        break;
      case 26:
        rx_sar_rsp.recvd    = 0xABCDABCE;
        rx_sar_rsp.win_size = -1;
        rx_sar_to_tx_eng_lup_rsp.write(rx_sar_rsp);
        tx_sar_rsp.not_ackd    = 0xABCDABCE + 0xF1;
        tx_sar_rsp.app_written = 0xABCDABCE + 0xF1;  // 0xF1 bytes was transmited
        tx_sar_to_tx_eng_rsp.write(tx_sar_rsp);
        break;
      case 28:
        // test 7 transmit a FIN-ACK to mock tuple
        event = EventWithTuple(Event(FIN, 0x1), FourTuple());
        ack_delay_to_tx_eng_event.write(event);

        slup_rsp.four_tuple = reverse_mock_tuple;
        slup_rsp.role_id    = 0x1;
        slookup_rev_table_to_tx_eng_rsp.write(slup_rsp);
        break;
      case 29:
        rx_sar_rsp.recvd    = 0xABCDABCE + 0xF1 + 1;  // 0xF1 bytes data + 1 B for FIN flag
        rx_sar_rsp.win_size = -1;
        rx_sar_to_tx_eng_lup_rsp.write(rx_sar_rsp);
        tx_sar_rsp.not_ackd    = 0xABCDABCE;
        tx_sar_rsp.app_written = 0xABCDABCE;

        tx_sar_to_tx_eng_rsp.write(tx_sar_rsp);
        break;

      default:
        break;
    }
    tx_engine(ack_delay_to_tx_eng_event,
              tx_eng_read_count_fifo,
              tx_eng_to_rx_sar_lup_req,
              rx_sar_to_tx_eng_lup_rsp,
              tx_eng_to_tx_sar_req,
              tx_sar_to_tx_eng_rsp,
              tx_eng_to_timer_set_rtimer,
              tx_eng_to_timer_set_ptimer,
              tx_eng_to_slookup_rev_table_req,
              slookup_rev_table_to_tx_eng_rsp,
              tx_eng_to_mover_read_cmd,
              mover_to_tx_eng_read_data,
              tx_ip_pkt_out

    );
    EmptyTxEngDataFifo(top_logger, tx_eng_to_mover_read_cmd, tx_ip_pkt_out, tx_ip_pkt_out_to_save);
    EmptyTxEngFsmFifo(top_logger,
                      tx_eng_read_count_fifo,
                      tx_eng_to_rx_sar_lup_req,
                      tx_eng_to_tx_sar_req,
                      tx_eng_to_timer_set_rtimer,
                      tx_eng_to_timer_set_ptimer,
                      tx_eng_to_slookup_rev_table_req);
    sim_cycle++;
    logger.SetSimCycle(sim_cycle);
    top_logger.SetSimCycle(sim_cycle);
  }
  StreamToPcap("tx_out.pcap", true, true, tx_ip_pkt_out_to_save, true);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "[ERROR] missing arguments " __FILE__ << " <INPUT_PCAP_FILE>" << endl;
    return -1;
  }
  char *              input_tcp_pcap_file = argv[1];
  stream<NetAXIS>     input_tcp_ip_pkt_read_in("input_tcp_ip_pkt_read_in");
  stream<NetAXISWord> input_tcp_ip_pkt("input_tcp_ip_pkt");
  // PcapToStream(input_tcp_pcap_file, true, input_tcp_ip_pkt_read_in);
  // NetAXIStreamToNetAXIStreamWord(input_tcp_ip_pkt_read_in, input_tcp_ip_pkt);

  // TestTcpTxConstructIpv4Pkt(input_tcp_ip_pkt);
  // TestRandom();
  TestTxEngine();
  // GenRandStream(0xF1, input_tcp_ip_pkt_read_in);
  // while (!input_tcp_ip_pkt_read_in.empty()) {
  //   NetAXISWord cur = input_tcp_ip_pkt_read_in.read();
  //   cout << cur.data.to_string(16) << endl;
  //   cout << cur.to_string() << endl;
  // }
  return 0;
}
