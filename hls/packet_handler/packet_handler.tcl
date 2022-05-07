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

open_solution "solution1"
set_part ${fpga_part} 
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

set_top PacketHandler

add_files "${prj_src_dir}/packet_handler.cpp" -cflags "-I${src_top_dir}"

add_files -tb  "${prj_src_dir}/packet_handler_test.cpp \
               ${src_top_dir}/utils/pcap/pcap_to_stream.cpp \
               ${src_top_dir}/utils/axi_utils_test.hpp \
               ${src_top_dir}/utils/pcap/pcap.cpp " -csimflags "-I${src_top_dir} -DDEBUG " 

if {$hls_act == "csim"} {
   csim_design -clean -argv "${pcap_input_dir}/packet_handler_input.pcap \
                     ${pcap_input_dir}/packet_handler_input_arp.pcap \
                     ${pcap_input_dir}/packet_handler_input_icmp.pcap \
                     ${pcap_input_dir}/packet_handler_input_tcp.pcap \
                     ${pcap_input_dir}/packet_handler_input_udp.pcap \
                     ${pcap_output_dir}/packet_handler_output"
}


csynth_design

if {$hls_act == "cosim"} {
   cosim_design -rtl verilog -argv "${pcap_input_dir}/packet_handler_input.pcap \
                     ${pcap_input_dir}/packet_handler_input_arp.pcap \
                     ${pcap_input_dir}/packet_handler_input_icmp.pcap \
                     ${pcap_input_dir}/packet_handler_input_tcp.pcap \
                     ${pcap_input_dir}/packet_handler_input_udp.pcap \
                     ${pcap_output_dir}/packet_handler_output"
}
export_design -rtl verilog -format ip_catalog

if {$hls_act == "install_ip"} {
   file delete -force ${ip_repo}/${prj_name}
   exec unzip ${prj_name}/solution1/impl/ip/*.zip -d ${ip_repo}/${prj_name}/
} 

 
exit