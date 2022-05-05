#define __gmp_const const
#include "ap_int.h"
#include "toe/toe_intf.hpp"

#include <cstdlib>
#include <fstream>
#include <hls_stream.h>
#include <iostream>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string>

using namespace hls;
using namespace std;

const uint8_t ECHO_REQUEST  = 0x08;
const uint8_t ECHO_REPLY    = 0x00;
const uint8_t ICMP_PROTOCOL = 0x01;

/** @defgroup icmp_server ICMP(Ping) Server
 *
 */
void icmp_server(stream<NetAXIS> &dataIn, ap_uint<32> &myIpAddress, stream<NetAXIS> &dataOut);