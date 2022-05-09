TOE_SRC=$(HLS_SRC_DIR)/toe
ARK_DELAY_SRC=$(TOE_SRC)/ack_delay
CLOSE_TIMER_SRC=$(TOE_SRC)/close_timer
EVENT_ENGINE_SRC=$(TOE_SRC)/event_engine
MEMORY_ACCESS_SRC=$(TOE_SRC)/memory_access
PORT_TABLE_SRC=$(TOE_SRC)/port_table
RETRANSMIT_TIMER_SRC=$(TOE_SRC)/retransmit_timer
RX_APP_INTF_SRC=$(TOE_SRC)/rx_app_intf
RX_ENGINE_SRC=$(TOE_SRC)/rx_engine
RX_SAR_TABLE_SRC=$(TOE_SRC)/rx_sar_table
SESSION_LOOKUP_SRC=$(TOE_SRC)/session_lookup_controller
STATE_TABLE_SRC=$(TOE_SRC)/state_table
STATISTICS_SRC=$(TOE_SRC)/statistics
TX_APP_INTF_SRC=$(TOE_SRC)/tx_app_intf
TX_ENGINE_SRC=$(TOE_SRC)/tx_engine
TX_SAR_TABLE_SRC=$(TOE_SRC)/tx_sar_table
TIMER_WRAPPER_SRC=$(TOE_SRC)/timer_wrapper
TEST_PORT_SRC=$(TOE_SRC)/test_port

IPERF_SRC=$(HLS_SRC_DIR)/iperf2_client
HASH_TABLE_SRC=$(HLS_SRC_DIR)/hash_table
ECHO_SRC=$(HLS_SRC_DIR)/echo_replay
ARP_SRC=$(HLS_SRC_DIR)/arp_server
ETHERNET_SRC=$(HLS_SRC_DIR)/ethernet_header_inserter
ICMP_SRC=$(HLS_SRC_DIR)/icmp_server
PKT_SRC=$(HLS_SRC_DIR)/packet_handler
USR_ABS_SRC=$(HLS_SRC_DIR)/user_abstraction
UDP_SRC=$(HLS_SRC_DIR)/udp
PORT_SRC=$(HLS_SRC_DIR)/port_handler


VIVADO_HLS_ARGS ?= $(HLS_SRC_DIR) $@ $(FPGA_PART) $(HLS_ACT) $(IP_REPO_DIR)



project = TOE \
		iperf2_client \
		echo_replay \
		arp_server \
		ethernet_inserter \
		icmp_server \
		packet_handler \
		user_abstraction \
		udp \
		port_handler \
		ack_delay \
		close_timer \
		event_engine \
		memory_access \
		port_table \
		probe_timer \
		retransmit_timer \
		rx_app_intf \
		rx_engine \
		rx_sar_table \
		session_lookup_controller \
		state_table \
		statistics \
		tx_app_intf \
		tx_engine \
		tx_sar_table \
		timer_wrapper


all: build

build: $(project)
	@echo "\e[94mIP Completed: $(project)\e[39m"
	@echo "Using Vivado HLS in $(VIVADO_HLS) \n FPGA_PART is $(FPGA_PART)"

clean:
	rm -rf *.log *.jou file* *.bak vivado*.str synlog.tcl .Xil fsm_encoding.os $(HLS_PRJ_DIR)

distclean: clean
	rm -rf $(project)


TOE: $(shell find $(TOE_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

iperf2_client: $(shell find $(IPERF_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

hash_table: $(shell find $(HASH_TABLE_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

echo_replay: $(shell find $(ECHO_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

arp_server: $(shell find $(ARP_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

ethernet_header_inserter: $(shell find $(ETHERNET_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

icmp_server: $(shell find $(ICMP_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

packet_handler: $(shell find $(PKT_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

user_abstraction: $(shell find $(USR_ABS_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

port_handler: $(shell find $(PORT_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

udp: $(shell find $(UDP_SRC) -type f) 
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(HLS_SRC_DIR)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

# TOE sub module
ack_delay: $(shell find $(ACK_DELAY_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

close_timer: $(shell find $(CLOSE_TIMER_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

event_engine: $(shell find $(EVENT_ENGINE_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

memory_access: $(shell find $(MEMORY_ACCESS_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

port_table: $(shell find $(PORT_TABLE_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

probe_timer: $(shell find $(PROBE_TIMER_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

retransmit_timer: $(shell find $(RETRANSMIT_TIMER_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

rx_app_intf: $(shell find $($RX_APP_INTF_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

rx_engine: $(shell find $(RX_ENGINE_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

rx_sar_table: $(shell find $(RX_SAR_TABLE_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

session_lookup_controller: $(shell find $(SESSION_LOOKUP_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

state_table: $(shell find $(STATE_TABLE_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

tx_app_intf: $(shell find $(TX_APP_INTF_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

tx_engine: $(shell find $(TX_ENGINE_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

tx_sar_table: $(shell find $(TX_SAR_TABLE_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

timer_wrapper: $(shell find $(TIMER_WRAPPER_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)

test_port: $(shell find $(TEST_PORT_SRC) -type f)  
	mkdir -p $(HLS_PRJ_DIR)/$@
	$(VIVADO_HLS) -f $(TOE_SRC)/$@/$@.tcl -tclargs $(VIVADO_HLS_ARGS)


.PHONY: list help
list:
	@(make -rpn | sed -n -e '/^$$/ { n ; /^[^ .#][^% ]*:/p ; }' | sort | egrep --color '^[^ ]*:' )


help:
	@echo "The basic usage of this makefile is:"
	@echo "1) Create the IPs"
	@echo "    \e[94mmake $(project)\e[39m"
	@echo ""
	@echo "Remember that you can always review this help with"
	@echo "    \e[94mmake help\e[39m"

format:
	@find . -regex '.*\.\(cpp\|h\|hpp\|cxx\)' | xargs clang-format -i