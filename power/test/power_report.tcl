# Test power reporting
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog power_test1.v
link_design power_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]

set_power_activity -global -activity 0.5 -duty 0.5

# Report power with explicit checks on returned content.
with_output_to_variable pwr_default { report_power }
if {![regexp {Total} $pwr_default]} {
  error "report_power output missing Total section"
}

with_output_to_variable pwr_inst { report_power -instances [get_cells reg1] }
if {![regexp {reg1} $pwr_inst]} {
  error "instance power report missing reg1 entry"
}

with_output_to_variable pwr_json { report_power -format json }
if {![regexp {"Total"} $pwr_json]} {
  error "json power report missing Total field"
}
