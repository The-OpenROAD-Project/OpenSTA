# get_* on object references

# Read in design and libraries
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
create_clock -name vclk -period 1000

# Test that set_driving_cell works with an object reference
set_driving_cell [all_inputs] -lib_cell [lindex [get_lib_cells] 0]

# Test each SDC get_* command on object references
puts {[get_cells [all_registers -cells]]}
report_object_full_names [get_cells [all_registers -cells]]
puts {[get_clocks [all_clocks]]}
report_object_full_names [get_clocks [all_clocks]]
puts {[get_lib_cells [get_lib_cells]]}
report_object_full_names [get_lib_cells [get_lib_cells]]
puts {[get_lib_pins [get_lib_pins]]}
report_object_full_names [get_lib_pins [get_lib_pins]]
puts {[get_libs [get_libs]]}
report_object_full_names [get_libs [get_libs]]
puts {[get_nets [get_nets]]}
report_object_full_names [get_nets [get_nets]]
puts {[get_pins [all_registers -data_pins]]}
report_object_full_names [get_pins [all_registers -data_pins]]
puts {[get_ports [all_inputs]]}
report_object_full_names [get_ports [all_inputs]]
