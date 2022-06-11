#ifndef _MOCK_LOGGER_HPP_
#define _MOCK_LOGGER_HPP_

enum ToeModule {
  NET_APP,
  ACK_DELAY,
  CLOSE_TMR,
  EVENT_ENG,
  PORT_TBLE,
  PROBE_TMR,
  RTRMT_TMR,
  RX_APP_IF,
  RX_ENGINE,
  RX_SAR_TB,
  SLUP_CTRL,
  CUKOO_CAM,
  STAT_TBLE,
  TX_APP_IF,
  TX_ENGINE,
  TX_SAR_TB,
  TOE_TOP,
  MISC_MDLE,  // could be some of modules
  DATA_MVER,
  MOCK_MEMY  // mock memory
};

#ifndef __SYNTHESIS__
#include <ap_int.h>
#include <fstream>
#include <string>
#include <unordered_map>
using std::endl;
using std::ofstream;
using std::string;
using std::unordered_map;
class MockLogger {
private:
  // all modules in 10 chars, including `\0`
  unordered_map<ToeModule, string> module_name = {
      {NET_APP, "NET_APP  "},   {ACK_DELAY, "ACK_DELAY"}, {CLOSE_TMR, "CLOSE_TMR"},
      {EVENT_ENG, "EVENT_ENG"}, {PORT_TBLE, "PORT_TBLE"}, {PROBE_TMR, "PROBE_TMR"},
      {RTRMT_TMR, "RTRMT_TMR"}, {RX_APP_IF, "RX_APP_IF"}, {RX_ENGINE, "RX_ENGINE"},
      {RX_SAR_TB, "RX_SAR_TB"}, {SLUP_CTRL, "SLUP_CTRL"}, {CUKOO_CAM, "CUKOO_CAM"},
      {STAT_TBLE, "STAT_TBLE"}, {TX_APP_IF, "TX_APP_IF"}, {TX_ENGINE, "TX_ENGINE"},
      {TX_SAR_TB, "TX_SAR_TB"}, {TOE_TOP, "TOE_TOP  "},   {MISC_MDLE, "MISC_MDLE"},
      {DATA_MVER, "DATA_MVER"}, {MOCK_MEMY, "MOCK_MEMY"}};

public:
  uint64_t  sim_cycle;
  ofstream  output_stream;
  bool      enable_recv_log;  // receive signals from other modules
  bool      enable_send_log;  // send signals to other modules
  ToeModule log_module;       // current log module

  MockLogger(string log_file, ToeModule cur_log_module) {
    sim_cycle = 0;
    output_stream.open(log_file);
    if (!output_stream) {
      std::cout << "Error: could not open test output file." << std::endl;
    }
    enable_send_log = true;
    enable_recv_log = true;
    log_module      = cur_log_module;
  }

  std::string replace(std::string str, const std::string &substr1, const std::string &substr2) {
    for (size_t index = str.find(substr1, 0); index != std::string::npos && substr1.length();
         index        = str.find(substr1, index + substr2.length()))
      str.replace(index, substr1.length(), substr2);
    return str;
  }
  // signal recv/send
  INLINE void Info(ToeModule     from_module,
                   ToeModule     to_module,
                   const string &signal_name,
                   const string &signal_state,
                   bool          state_in_new_line = false) {
    string delimiter = state_in_new_line ? ("\n\t\t\t") : ("\t");
    if (enable_recv_log || enable_send_log) {
      string from_module_str  = "[" + module_name[from_module] + "]";
      string to_module_str    = "[" + module_name[to_module] + "]\t";
      string signal_state_str = signal_state;
      if (state_in_new_line) {
        signal_state_str = replace(signal_state_str, "\n", delimiter);
      }
      if ((enable_recv_log && to_module == log_module) ||
          (enable_send_log && from_module == log_module) || (log_module == TOE_TOP)) {
        output_stream << "C/" << sim_cycle << ":  ";
        output_stream << from_module_str << "->" << to_module_str << "{" << signal_name << "}"
                      << delimiter << signal_state_str << endl;
      }
    }
  }
  // inner state
  INLINE void Info(const string &description,
                   const string &state             = "",
                   bool          state_in_new_line = false) {
    string delimiter      = state_in_new_line ? ("\n\t\t") : ("\t");
    string log_module_str = "[" + module_name[log_module] + "]";
    output_stream << "C/" << sim_cycle << ":  ";

    output_stream << log_module_str << "\t\t\t\t"
                  << "{" << description << "}" << delimiter << state << endl;
  }
  // // inner state
  // INLINE void Info(const string &description) {
  //   string log_module_str = "[" + module_name[log_module] + "]";
  //   output_stream << "Cycle " << sim_cycle << ":\t";
  //   output_stream << log_module_str << "\t\t\t\t\t" << description << endl;
  // }

  INLINE void NewLine(const string &description) {
    output_stream << "---------" << description << "---------\n";
  }

  INLINE void IncreaseSimCycle() { sim_cycle++; }
  INLINE void SetSimCycle(uint64_t cycle) { sim_cycle = cycle; };
  INLINE void DisableSendLog() { enable_send_log = false; }
  INLINE void EnableSendLog() { enable_send_log = true; }
  INLINE void DisableRecvLog() { enable_recv_log = false; }
  INLINE void EnableRecvLog() { enable_recv_log = true; }
};
#else

class MockLogger {
public:
  INLINE void Info(const char *fmt...) { _AP_UNUSED_PARAM(fmt); }
  INLINE void Info(const int fmt...) { _AP_UNUSED_PARAM(fmt); }
};
#endif

#endif  // _MOCK_LOGGER_HPP_