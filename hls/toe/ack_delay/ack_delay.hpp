#include "toe/mock/mock_toe.hpp"

using namespace hls;

void ack_delay(stream<EventWithTuple> &input,
               stream<EventWithTuple> &output,
               stream<ap_uint<1> > &   readCountFifo,
               stream<ap_uint<1> > &   writeCountFifo);
