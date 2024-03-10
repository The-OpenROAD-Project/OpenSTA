read_liberty ../examples/sky130hd_tt.lib
read_verilog attribute_parsing.v
link_design counter
create_clock -name clk [get_ports clk] -period 50

set instance [sta::top_instance]
set cell [$instance cell]
set cell_name [$cell name]
set src_location [$cell get_attribute "src"]
puts "top_instance:\"$cell_name\" attribute \"src\" = $src_location"

set instance_name "_1415_"
set instance_src_location [[sta::find_instance $instance_name] get_attribute "src"]
puts "instance: $instance_name attribute \"src\" = $instance_src_location"