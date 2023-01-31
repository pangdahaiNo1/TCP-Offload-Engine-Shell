set src_top_dir [lindex $argv 2]
set prj_name [lindex $argv 3]
set fpga_part [lindex $argv 4]
set hls_act [lindex $argv 5]
set ip_repo [lindex $argv 6]


set prj_src_dir ${src_top_dir}/${prj_name}
set pcap_input_dir ${src_top_dir}/pcap/input
set pcap_output_dir ${src_top_dir}/pcap/output

# Create project
open_project ${prj_name}

open_solution solution1
set_part ${fpga_part}
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

set_top toe_top

add_files " ${prj_src_dir}/${prj_name}.cpp \
  ${prj_src_dir}/ack_delay/ack_delay.cpp \
  ${prj_src_dir}/event_engine/event_engine.cpp \
  ${prj_src_dir}/port_table/port_table.cpp \
  ${prj_src_dir}/rx_app_intf/rx_app_intf.cpp \
  ${prj_src_dir}/rx_engine/rx_engine.cpp \
  ${prj_src_dir}/rx_sar_table/rx_sar_table.cpp \
  ${prj_src_dir}/session_lookup_controller/session_lookup_controller.cpp \
  ${prj_src_dir}/state_table/state_table.cpp \
  ${prj_src_dir}/timer_wrapper/timer_wrapper.cpp \
  ${prj_src_dir}/close_timer/close_timer.cpp \
  ${prj_src_dir}/probe_timer/probe_timer.cpp \
  ${prj_src_dir}/retransmit_timer/retransmit_timer.cpp \
  ${prj_src_dir}/memory_access/memory_access.hpp \
  ${prj_src_dir}/tx_app_intf/tx_app_intf.cpp \
  ${prj_src_dir}/tx_engine/tx_engine.cpp \
  ${prj_src_dir}/tx_sar_table/tx_sar_table.cpp \
  ${src_top_dir}/utils/axi_utils.cpp " -cflags "-I${src_top_dir} -DDEBUG"

add_files -tb "${prj_src_dir}/${prj_name}_test.cpp \
  ${src_top_dir}/toe/mock/mock_logger.hpp \
  ${src_top_dir}/toe/mock/mock_memory.hpp \
  ${src_top_dir}/toe/mock/mock_net_app.hpp \
  ${src_top_dir}/toe/mock/mock_cam.hpp "  -cflags "-I${src_top_dir} -I${src_top_dir}/toe -DDEBUG"

if {$hls_act == "csim"} {
  csim_design -clean   -argv "${pcap_input_dir}/echo_client_golden_tiny.pcap"
  exit
}

if {$hls_act == "synth"} {
  csynth_design
  export_design -rtl verilog -format ip_catalog -ipname "mr_toe" -display_name "Multi-Role TOE"
}

if {$hls_act == "cosim"} {
  csim_design -clean   -argv "${pcap_input_dir}/echo_client_golden_tiny.pcap"
}

if {$hls_act == "install_ip"} {
  file delete -force ${ip_repo}/${prj_name}
  exec unzip ${prj_name}/solution1/impl/ip/*.zip -d ${ip_repo}/${prj_name}/
}

exit
