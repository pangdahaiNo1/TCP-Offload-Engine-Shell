#ifndef _TOE_CONFIG_HPP_
#define _TOE_CONFIG_HPP_
#include <stdint.h>

#define TCP_MAX_SESSIONS 128
#define TCP_MSS 1460
#define TCP_MSS_TEN_TIMES 14600

// TCP_NODELAY flag, to disable Nagle's Algorithm
// and data from tx app write to tx engine directly,
#define TCP_NODELAY 0

// Rx data write to DDR or write to App directly
#define TCP_RX_DDR_BYPASS 0

// TCP fast recovery/fast retransmit
#define TCP_FAST_RETRANSMIT 1

#define TCP_MAX_RETX_ATTEMPTS 4

// TCP window scaling option
#define TCP_WINDOW_SCALE 1
const uint32_t WINDOW_SCALE_BITS = 2;
// Each Session has 1M buffer
const uint32_t WINDOW_BITS = 16 + WINDOW_SCALE_BITS;

const uint32_t BUFFER_SIZE           = (1 << WINDOW_BITS);
const uint32_t CONGESTION_WINDOW_MAX = (BUFFER_SIZE - 2048);

#define LISTENING_PORT_CNT (1 << 15)
#define FREE_PORT_CNT (1 << 15)

#define TCP_DDR_ADDR_WIDTH 40
const uint64_t TCP_DDR_BASE_ADDR    = 0x4800000000;
const uint64_t TCP_DDR_RX_BASE_ADDR = TCP_DDR_BASE_ADDR + 0x300000000;
const uint64_t TCP_DDR_TX_BASE_ADDR = TCP_DDR_BASE_ADDR + 0x350000000;

#endif  // _TOE_CONFIG_HPP_