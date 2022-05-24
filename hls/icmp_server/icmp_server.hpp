#ifndef _ICMP_SERVER_HPP_
#define _ICMP_SERVER_HPP_
#include "utils/axi_intf.hpp"

using namespace hls;
using namespace std;

const uint8_t ECHO_REQUEST  = 0x08;
const uint8_t ECHO_REPLY    = 0x00;
const uint8_t ICMP_PROTOCOL = 0x01;

/** @defgroup icmp_server ICMP(Ping) Server
 *
 */
void icmp_server(stream<NetAXIS> &dataIn, ap_uint<32> &myIpAddress, stream<NetAXIS> &dataOut);

#endif