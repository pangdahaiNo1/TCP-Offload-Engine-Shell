CMAC_SRC_DIR=$(CMAC_DIR)/src
CONSTRAINS_DIR=$(CMAC_DIR)/constaints
CMAC_SCRIPTS_DIR=$(CMAC_DIR)/scripts
IP_REPO_CMAC_DIR=$(IP_REPO_DIR)/cmac

projects = cmac_sync

projects : create_project_folder  $(projects)
	@echo "\e[94mIP Completed: $(projects)\e[39m"
	@echo "Using Vivado in $(VIVADO) \t FPGA_PART is $(FPGA_PART)"

$(projects) : % : $(IP_REPO_CMAC_DIR)/%/component.xml
	

cmac_sync_recursive_dependencies := $(shell find $(CMAC_SRC_DIR) -print)
$(IP_REPO_CMAC_DIR)/cmac_sync/component.xml: $(cmac_sync_recursive_dependencies) \
								   $(CMAC_SCRIPTS_DIR)/cmac_sync.tcl \
								   $(CMAC_SRC_DIR)/types.svh
	rm -rf 	$(IP_REPO_CMAC_DIR)/cmac_sync
	$(VIVADO) -nojournal -nolog -mode batch -source $(CMAC_SCRIPTS_DIR)/cmac_sync.tcl -tclargs $(CMAC_DIR) $(VIVADO_PRJ_DIR) $(CMAC_SRC_DIR) $(CONSTRAINS_DIR) $(FPGA_PART) $(IP_REPO_CMAC_DIR)



.PHONY: create_project_folder clean distclean 

create_project_folder:
	mkdir -p $(IP_REPO_CMAC_DIR)

clean:
	rm -rf vivado*.log vivado*.jou file* *.bak synlog.tcl .Xil fsm_encoding.os  $(VIVADO_PRJ_DIR) 

distclean: clean
	rm -rf $(IP_REPO_CMAC_DIR) 
