#include "state_table.hpp"
#include "toe/mock/mock_toe.hpp"

using namespace hls;

struct readVerify {
  int  id;
  char fifo;
  int  exp;
};

void emptyFifos(std::ofstream &       out,
                stream<SessionState> &rxFifoOut,
                stream<SessionState> &appFifoOut,
                stream<SessionState> &app2FifoOut,
                stream<ap_uint<16> > &slupOut,
                int                   iter) {
  SessionState outData;
  ap_uint<16>  outDataId;

  while (!(rxFifoOut.empty())) {
    rxFifoOut.read(outData);
    out << "Step " << std::dec << iter << ": RX Fifo\t\t";
    out << outData << std::endl;
  }

  while (!(appFifoOut.empty())) {
    appFifoOut.read(outData);
    out << "Step " << std::dec << iter << ": App Fifo\t\t";
    out << outData << std::endl;
  }

  while (!(app2FifoOut.empty())) {
    app2FifoOut.read(outData);
    out << "Step " << std::dec << iter << ": App2 Fifo\t\t";
    out << outData << std::endl;
  }

  while (!(slupOut.empty())) {
    slupOut.read(outDataId);
    out << "Step " << std::dec << iter << ": Slup Fifo\t\t";
    out << std::hex;
    out << std::setfill('0');
    out << std::setw(4) << outDataId;
    out << std::endl;
  }
}

int main() {
  stream<StateTableReq> rxIn;
  stream<SessionState>  rxOut;
  stream<StateTableReq> txAppIn;
  stream<SessionState>  txAppOut;
  stream<TcpSessionID>  txApp2In;
  stream<SessionState>  txApp2Out;
  stream<TcpSessionID>  timerIn;
  stream<TcpSessionID>  slupOut;

  StateTableReq inData;
  SessionState  outData;

  std::ifstream inputFile;
  std::ofstream outputFile;

  outputFile.open("./out.dat");
  if (!outputFile) {
    std::cout << "Error: could not open test output file." << std::endl;
  }

  int count = 0;

  /*
   * Test 1: rx(x, ESTA); timer(x), rx(x, FIN_WAIT)
   */
  // ap_uint<16> id = rand() % 100;
  TcpSessionID id = 0x57;
  outputFile << "Test 1" << std::endl;
  outputFile << "Session ID: " << id << std::endl;

  while (count < 20) {
    switch (count) {
      case 1:
        rxIn.write(StateTableReq(id, ESTABLISHED, 1));
        break;
      case 2:
        timerIn.write(id);
        break;
      case 3:
        rxIn.write(StateTableReq(id));
        break;
      default:
        break;
    }
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    emptyFifos(outputFile, rxOut, txAppOut, txApp2Out, slupOut, count);
    count++;
  }
  outputFile << "------------------------------------------------" << std::endl;

  // BREAK
  count = 0;
  while (count < 200) {
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    count++;
  }

  /*
   * Test 2: rx(x, ESTA); rx(y, ESTA); timer(x), time(y)
   */
  // ap_uint<16> id = rand() % 100;
  count            = 0;
  id               = 0x6f;
  TcpSessionID id2 = 0x3a;
  outputFile << "Test 2" << std::endl;
  outputFile << "ID: " << id << " ID2: " << id2 << std::endl;
  while (count < 20) {
    switch (count) {
      case 1:
        rxIn.write(StateTableReq(id, ESTABLISHED, 1));
        break;
      case 2:
        rxIn.write(StateTableReq(id2, ESTABLISHED, 1));
        break;
      case 3:
        timerIn.write(id2);
        break;
      case 4:
        timerIn.write(id);
        break;
      case 5:
        timerIn.write(id);
        break;
      default:
        break;
    }
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    emptyFifos(outputFile, rxOut, txAppOut, txApp2Out, slupOut, count);
    count++;
  }
  outputFile << "------------------------------------------------" << std::endl;
  // BREAK
  count = 0;
  while (count < 200) {
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    count++;
  }

  /*
   * Test 3: rx(x); timer(x), rx(x, ESTABLISHED)
   * Pause then: rx(y), txApp(y), rx(y, ESTABLISHED), txApp(y, value)
   */
  // ap_uint<16> id = rand() % 100;
  count = 0;
  id    = 0x6f;
  id2   = 0x3a;
  outputFile << "Test 3" << std::endl;
  outputFile << "ID: " << id << " ID2: " << id2 << std::endl;
  while (count < 40) {
    switch (count) {
      case 1:
        rxIn.write(StateTableReq(id));
        break;
      case 2:
        timerIn.write(id);
        break;
      case 5:
        rxIn.write(StateTableReq(id, ESTABLISHED, 1));
        break;
      case 8:
        rxIn.write(StateTableReq(id));
        break;
      case 10:
        rxIn.write(StateTableReq(id2));
        break;
      case 11:
        txAppIn.write(StateTableReq(id2));
        break;
      case 14:
        rxIn.write(StateTableReq(id2, CLOSING, 1));
        break;
      case 15:
        txAppIn.write(StateTableReq(id2, SYN_SENT, 1));
        break;
      case 19:
        txApp2In.write(id2);
        break;
      default:
        break;
    }
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    emptyFifos(outputFile, rxOut, txAppOut, txApp2Out, slupOut, count);
    count++;
  }
  outputFile << "------------------------------------------------" << std::endl;
  // BREAK
  count = 0;
  while (count < 200) {
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    count++;
  }

  /*
   * Test 4: txApp(x); rx(x), txApp(x, SYN_SENT), rx(x, ESTABLISHED), txApp2(x)
   */
  // ap_uint<16> id = rand() % 100;
  count = 0;
  id    = 0xf3;
  outputFile << "Test 4" << std::endl;
  outputFile << "ID: " << id << std::endl;
  while (count < 40) {
    switch (count) {
      case 1:
        txAppIn.write(StateTableReq(id));
        break;
      case 2:
        rxIn.write(StateTableReq(id));
        break;
      case 5:
        txAppIn.write(StateTableReq(id, SYN_SENT, 1));
        break;
      case 7:
        rxIn.write(StateTableReq(id, ESTABLISHED, 1));
        break;
      case 19:
        txApp2In.write(id);
        break;
      default:
        break;
    }
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    emptyFifos(outputFile, rxOut, txAppOut, txApp2Out, slupOut, count);
    count++;
  }
  outputFile << "------------------------------------------------" << std::endl;
  // BREAK
  count = 0;
  while (count < 200) {
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    count++;
  }

  /*
   * Test 5: rxEng(x, ESTABLISED), timer(x), rxEng(x)
   */
  // ap_uint<16> id = rand() % 100;
  count = 0;
  id    = 0x83;
  outputFile << "Test 5" << std::endl;
  outputFile << "ID: " << id << std::endl;
  while (count < 40) {
    switch (count) {
      case 1:
        rxIn.write(StateTableReq(id, ESTABLISHED, 1));
        break;
      case 5:
        timerIn.write(id);
        break;
      case 6:
        rxIn.write(StateTableReq(id));
        break;
      default:
        break;
    }
    state_table(rxIn, txAppIn, txApp2In, timerIn, rxOut, txAppOut, txApp2Out, slupOut);
    emptyFifos(outputFile, rxOut, txAppOut, txApp2Out, slupOut, count);
    count++;
  }

  return 0;
}
