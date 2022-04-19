set src_top_dir [lindex $argv 2]
set prj_name [lindex $argv 3]
set fpga_part [lindex $argv 4]
set hls_act [lindex $argv 5]
set ip_repo [lindex $argv 6]

set prj_src_dir ${src_top_dir}/${prj_name}
set pcap_input_dir ${prj_src_dir}/input
set pcap_output_dir ${prj_src_dir}/output

# Create project
open_project ${prj_name}

open_solution "solution1"
set_part ${fpga_part} 
create_clock -period 3.1 -name default
set_clock_uncertainty 0.2

set_top ${prj_name}_top

add_files ${prj_src_dir}/hash_table.cpp -cflags "-std=c++11 -I${src_top_dir}"


add_files -tb ${prj_src_dir}/hash_table_test.cpp -cflags "-std=c++11 -I${src_top_dir}"


if {$hls_act == "csim"} {
   csim_design -clean 
   exit
}

if {$hls_act == "cosim"} {
   cosim_design -rtl verilog 
   exit
}

csynth_design
export_design -rtl verilog -format ip_catalog -ipname "hash_table" -display_name "Hash Table (cuckoo)" -description "" -vendor "ethz.systems.fpga" -version "1.0"

if {$hls_act == "install_ip"} {
   file delete -force ${ip_repo}/${prj_name}
   exec unzip ${prj_name}/solution1/impl/ip/*.zip -d ${ip_repo}/${prj_name}/
} 


exit

exit
