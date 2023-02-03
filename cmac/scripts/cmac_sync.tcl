# input
set top_dir [lindex $argv 0]
set prj_dir [lindex $argv 1]
set src_dir [lindex $argv 2]
set constrains_dir [lindex $argv 3]
set fpga_part [lindex $argv 4]
# output
set iprepo_dir [lindex $argv 5]

set project_name "cmac_sync_prj"
set synthesize_ip false
set core_name "cmac_sync"


create_project -force $project_name $prj_dir/$project_name   -part $fpga_part


read_verilog -sv $src_dir/cmac_sync.sv
read_verilog -sv $src_dir/cmac_sync_wrapper.sv
read_verilog -sv $src_dir/cmac_0_axi4_lite_user_if.v
read_verilog -sv $src_dir/rx_sync.v
read_verilog -sv $src_dir/tx_sync.v
read_verilog -sv $src_dir/cmac_0_cdc.v
read_verilog -sv $src_dir/types.svh

add_files -fileset constrs_1 -norecurse $constrains_dir/cmac_sync_false_path.xdc

set_property top cmac_sync_wrapper [current_fileset]



ipx::package_project  -import_files -root_dir $iprepo_dir/$core_name -vendor naudit -library cmac -taxonomy /UserIP

set_property vendor naudit [ipx::current_core]
set_property library cmac [ipx::current_core]
set_property name $core_name [ipx::current_core]
set_property display_name $core_name [ipx::current_core]
set_property description $core_name [ipx::current_core]
set_property version 1 [ipx::current_core]
set_property core_revision 1 [ipx::current_core]

set_property display_name {Ultrascale_Plus} [ipgui::get_guiparamspec -name "ULTRASCALE_PLUS" -component [ipx::current_core] ]
set_property widget {checkBox} [ipgui::get_guiparamspec -name "ULTRASCALE_PLUS" -component [ipx::current_core] ]
set_property value false [ipx::get_user_parameters ULTRASCALE_PLUS -of_objects [ipx::current_core]]
set_property value false [ipx::get_hdl_parameters ULTRASCALE_PLUS -of_objects [ipx::current_core]]
set_property value_format bool [ipx::get_user_parameters ULTRASCALE_PLUS -of_objects [ipx::current_core]]
set_property value_format bool [ipx::get_hdl_parameters ULTRASCALE_PLUS -of_objects [ipx::current_core]]

ipx::create_xgui_files [ipx::current_core]
ipx::update_checksums [ipx::current_core]
ipx::save_core [ipx::current_core]


close_project
