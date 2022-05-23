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

set_top ethernet_header_inserter

add_files "${prj_src_dir}/${prj_name}.cpp 
			${src_top_dir}/utils/axi_utils.hpp \
            ${src_top_dir}/utils/axi_utils.cpp " -cflags "-I${src_top_dir} -DDEBUG"

add_files -tb "${prj_src_dir}/${prj_name}_test.cpp \
               ${src_top_dir}/icmp_server/icmp_server.cpp \
               ${src_top_dir}/utils/pcap/pcap_to_stream.cpp \
         	   ${src_top_dir}/utils/pcap/pcap.cpp \   
               ${src_top_dir}/utils/axi_utils.cpp"   -csimflags "-I${src_top_dir} -DDEBUG -DEBUG_PCAP" 


if {$hls_act == "csim"} {
   csim_design -clean -argv "${pcap_input_dir}/icmp_in.pcap ${pcap_output_dir}/icmp_golden_with_ipv4_checksum.pcap"
   exit
}

csynth_design
if {$hls_act == "cosim"} {
   cosim_design -rtl verilog -argv "${pcap_input_dir}/icmp_in.pcap ${pcap_output_dir}/icmp_golden_with_ipv4_checksum.pcap"
   exit
}

export_design -format ip_catalog -ipname "ethernet_header_inserter" 

if {$hls_act == "install_ip"} {
   file delete -force ${ip_repo}/${prj_name}
   exec unzip ${prj_name}/solution1/impl/ip/*.zip -d ${ip_repo}/${prj_name}/
} 


exit