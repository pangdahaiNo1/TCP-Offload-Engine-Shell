# FPGA PART& BOARD
FPGA_PART=xczu19eg-ffvc1760-2-e
BOARD_PART=sugon:nf_card:part0:2.0
# Toolchains set
VIVADO=/opt/Xilinx_2020.2/Vivado/2020.2/bin/vivado
#VIVADO_HLS=/opt/Xilinx_2019.1/Vivado/2019.1/bin/vivado_hls
VIVADO_HLS=/opt/Xilinx_2022.2/Vitis_HLS/2022.2/bin/vitis_hls
# Top directory for ucas-cod-shell design
TOP_DIR=$(shell dirname `pwd`)

# Source directory
HW_DIR=$(shell pwd)
CMAC_DIR=$(HW_DIR)/cmac
HLS_SRC_DIR=$(HW_DIR)/hls

# Output directory
VIVADO_PRJ_DIR=$(HW_DIR)/vivado_prj
HLS_PRJ_DIR=$(HW_DIR)/vivado_hls_prj
VITIS_HLS_PRJ_DIR=$(HW_DIR)/vitis_hls_prj
IP_REPO_DIR=$(HW_DIR)/ip_repo

# HLS_ACT must be one of the following actions: csim, csynth, export_ip, install_ip
# csim:  		Compiles and runs pre-synthesis C simulation using the provided C test bench.
# synth: 		Synthesizes the Vivado HLS project for the active solution.
# install_ip: 	Extract packaged IP to $(IP_REPO_DIR) directory
HLS_ACT = csim
VITIS_HLS_ACT = csim
# export all varibles to sub makefile 
export

.PHONY: FORCE clean hls_prj

cmac: FORCE
	mkdir -p $(VIVADO_PRJ_DIR)
	make -C $(CMAC_DIR) 

ifeq ($(HLS_ACT),)
hls_prj: FORCE
	$(error Please specify the action to be lanuched for Vivado hardware design)
else
hls_prj: FORCE
	@mkdir -p $(HLS_PRJ_DIR)
	@cp $(HLS_SRC_DIR)/hls.mk $(HLS_PRJ_DIR)/Makefile
	@make -C $(HLS_PRJ_DIR) 
endif


clean:
	rm -rf $(HLS_PRJ_DIR)/Makefile
	rm -rf $(HLS_PRJ_DIR) 
	rm -rf $(VITIS_HLS_PRJ_DIR) 

format:
	make -C $(HLS_SRC_DIR) -f hls.mk format