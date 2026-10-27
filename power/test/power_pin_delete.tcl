# Pin activity annotations survive network_changed_non_sdc (scan_replace)
# and are removed with their pins.
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog power_test1.v
link_design power_test1
create_clock -name clk -period 10 [get_ports clk]
set_power_activity -input_ports in1 -activity 0.01
set_power_activity -pins [get_pins reg1/Q] -activity 0.05
sta::network_changed_non_sdc
report_activity_annotation -report_annotated
delete_instance reg1
report_activity_annotation -report_annotated
