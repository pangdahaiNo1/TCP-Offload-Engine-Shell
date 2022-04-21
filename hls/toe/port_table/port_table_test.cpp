#include "port_table.hpp"
#include "toe/mock/mock_toe.hpp"

using namespace hls;

ap_uint<16> swappedBytes(ap_uint<16> in) {
  ap_uint<16> swapped;
  swapped(7, 0)  = in(15, 8);
  swapped(15, 8) = in(7, 0);
  return swapped;
}

int main() {
  stream<ap_uint<16> >         rx_eng_to_ptable_req;
  stream<NetAXISListenPortReq> rx_app_to_ptable_listen_port_req;
  stream<ap_uint<16> >         slup_to_ptable_realease_port;
  stream<bool>                 ptable_to_rx_eng_check_rsp;
  stream<NetAXISListenPortRsp> ptable_to_rx_app_listen_port_rsp;
  stream<ap_uint<16> >         ptable_to_tx_app_port_rsp;

  int pkgCount = 0;

  bool currBool = false;
  // listenPortStatus currStat;
  ap_uint<16>          currPort = 0;
  ap_uint<16>          port;
  bool                 isOpen = false;
  int                  mod    = 0;
  NetAXISListenPortReq randPort;
  randPort.data = 32767;
  randPort.dest = 1;
  ap_uint<16> temp;
  int         count = 0;
  while (count < 500)  // was 250
  {
    mod = count % 10;
    switch (mod) {
      case 2:
        port = 80;
        rx_eng_to_ptable_req.write(swappedBytes(port));  // swap bytes, this is 80
        break;
      case 5:
        std::cout << "[PORT_TABLE CLIENT]-Try to listen on " << randPort.data << std::endl;
        rx_app_to_ptable_listen_port_req.write(randPort);
        // randPort = rand() % 100;
        break;
      case 7:
        if (!isOpen) {
          randPort.data = 80;
          rx_app_to_ptable_listen_port_req.write(randPort);
          std::cout << "[PORT_TABLE CLIENT]-Start listening on port 80" << std::endl;
          isOpen = true;
        }
        break;
      default:
        break;
    }
    port_table(rx_eng_to_ptable_req,
               rx_app_to_ptable_listen_port_req,
               slup_to_ptable_realease_port,
               ptable_to_rx_eng_check_rsp,
               ptable_to_rx_app_listen_port_rsp,
               ptable_to_tx_app_port_rsp);

    // if (count == 20) {
    //   rxEng2portTable_req.write(0x0007);
    // }
    // if (count == 40) {
    //   rxEng2portTable_req.write(0x0700);  // 0x0700 = 1792
    // }
    // if (!portTable2rxEng_check_rsp.empty()) {
    //   portTable2rxEng_check_rsp.read(currBool);
    //   std::cout << "[PORT_TABLE CLIENT]-Port 80 is open: " << (currBool ? "yes" : "no")
    //             << std::endl;
    // }
    // if (!portTable2rxApp_listen_rsp.empty()) {
    //   portTable2rxApp_listen_rsp.read(currStat);
    //   std::cout << "[PORT_TABLE CLIENT]-Listening " << (currStat.open_successfully ? "" : "not")
    //             << " successful" << std::endl;
    // }
    // if (!portTable2txApp_port_rsp.empty()) {
    //   portTable2txApp_port_rsp.read(currPort);
    //   assert(currPort >= 32768);
    // }
    count++;
  }

  // should return comparison

  return 0;
}