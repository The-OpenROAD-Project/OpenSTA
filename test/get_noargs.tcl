# get_* with no arguments

# Read in design and libraries
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
create_clock -name vclk -period 1000

# Test each SDC get_* command with no arguments
puts {[get_cells]}
report_object_full_names [get_cells]
puts {[get_clocks]}
report_object_full_names [get_clocks]
puts {[get_lib_cells]}
report_object_full_names [get_lib_cells]
puts {[get_lib_pins]}
report_object_full_names [get_lib_pins]
puts {[get_libs]}
report_object_full_names [get_libs]
puts {[get_nets]}
report_object_full_names [get_nets]
puts {[get_pins]}
report_object_full_names [get_pins]
puts {[get_ports]}
report_object_full_names [get_ports]
