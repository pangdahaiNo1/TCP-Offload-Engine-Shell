set src_top_dir [lindex $argv 2]
set prj_name [lindex $argv 3]
set fpga_part [lindex $argv 4]
set hls_act [lindex $argv 5]
set ip_repo [lindex $argv 6]


set prj_src_dir ${src_top_dir}/toe/${prj_name}
set pcap_input_dir ${src_top_dir}/pcap/input
set pcap_output_dir ${src_top_dir}/pcap/output

# Create project
open_project ${prj_name}

open_solution solution1
set_part ${fpga_part}
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

set_top retransmit_timer

add_files "${prj_src_dir}/${prj_name}.cpp \
  ${src_top_dir}/utils/axi_utils.hpp " -cflags "-I${src_top_dir} -DDEBUG"

add_files -tb "${prj_src_dir}/${prj_name}_test.cpp \
  ${src_top_dir}/toe/mock/mock_logger.hpp \
  ${src_top_dir}/toe/mock/mock_memory.hpp \
  ${src_top_dir}/toe/mock/mock_net_app.hpp \
  ${src_top_dir}/toe/mock/mock_cam.hpp "  -cflags "-I${src_top_dir} -I${src_top_dir}/toe -DDEBUG"

if {$hls_act == "csim"} {
  csim_design -clean
}
if {$hls_act == "synth"} {
  csynth_design
}
if {$hls_act == "cosim"} {
  cosim_design -rtl verilog
}

exit
