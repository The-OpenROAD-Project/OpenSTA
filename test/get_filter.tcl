# Read in design and libraries
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
create_clock -name vclk -period 1000

# Test filters for each SDC command
puts "get_cells"
report_object_full_names [get_cells -filter liberty_cell==BUFx2_ASAP7_75t_R *]
puts "get_clocks"
report_object_full_names [get_clocks -filter is_virtual==0 *]
puts "get_clocks 2"
report_object_full_names [get_clocks -filter is_virtual==1 *]
puts "get_clocks 3"
report_object_full_names [get_clocks -filter is_virtual *]
puts "get_clocks 4"
report_object_full_names [get_clocks -filter is_virtual&&is_generated *]
puts "get_clocks 5"
report_object_full_names [get_clocks -filter is_virtual&&is_generated==0 *]
puts "get_clocks 6"
report_object_full_names [get_clocks -filter is_virtual||is_generated *]
puts "get_clocks 7"
report_object_full_names [get_clocks -filter is_virtual==0||is_generated *]
puts "get_lib_cells"
report_object_full_names [get_lib_cells -filter is_buffer==1 *]
puts "get_lib_cells 2"
report_object_full_names [get_lib_cells -filter is_inverter==0 *]
puts "get_lib_pins"
report_object_full_names [get_lib_pins -filter direction==input BUFx2_ASAP7_75t_R/*]
puts "get_lib_pins 2"
report_object_full_names [get_lib_pins -filter direction==output BUFx2_ASAP7_75t_R/*]
puts "get_libs"
report_object_full_names [get_libs -filter name==asap7_small *]
puts "get_nets"
report_object_full_names [get_nets -filter name=~*q *]
puts "get_pins"
report_object_full_names [get_pins -filter direction==input *]
puts "get_pins 2"
report_object_full_names [get_pins -filter direction==output *]
puts "get_ports"
report_object_full_names [get_ports -filter direction==input *]
puts "get_ports 2"
report_object_full_names [get_ports -filter direction==output *]
