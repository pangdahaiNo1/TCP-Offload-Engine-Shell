#ifndef _MOCK_LOGGER_HPP_
#define _MOCK_LOGGER_HPP_

#include "toe/toe_conn.hpp"
#include "utils/pcap/pcap_to_stream.hpp"

#include <fstream>
#include <string>

using std::ofstream;
using std::string;

#ifndef __SYNTHESIS__
class MockLogger {
public:
  uint64_t sim_cycle;
  ofstream output_stream;

  MockLogger(string log_file) {
    sim_cycle = 0;
    output_stream.open(log_file);
    if (!output_stream) {
      std::cout << "Error: could not open test output file." << std::endl;
    }
  }

  INLINE void Info(string description, string state, bool state_in_new_line) {
    if (!state_in_new_line) {
      output_stream << "Cycle " << sim_cycle << ": " << description << "\t" << state << "\n";
    } else {
      output_stream << "Cycle " << sim_cycle << ": " << description << "\n"
                    << "\t\t" << state << "\n";
    }
  }

  INLINE void IncreaseSimCycle() { sim_cycle++; }
  INLINE void SetSimCycle(uint64_t cycle) { sim_cycle = cycle; };
};
#else

class MockLogger {
public:
  INLINE void Info(const char *fmt...) { _AP_UNUSED_PARAM(fmt); }
};
#endif

#endif  // _MOCK_LOGGER_HPP_