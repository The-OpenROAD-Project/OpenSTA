# Tests whether Verilog attributes can be parsed and retrieved correctly
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog verilog_attribute.v
link_design counter
create_clock -name clk [get_ports clk] -period 50

set instance [sta::top_instance]
set cell [$instance cell]
set cell_name [$cell name]
set src_location [$cell get_attribute "src"]
puts "top_instance:\"$cell_name\" attribute \"src\" = $src_location"

set instance_name "_1415_"
set instance_src_location [[sta::find_instance $instance_name] get_attribute "src"]
set instance_attr1 [[sta::find_instance $instance_name] get_attribute "attr1"]
set instance_attr2 [[sta::find_instance $instance_name] get_attribute "attr2"]
puts "instance: $instance_name attribute \"src\" = $instance_src_location"
puts "instance: $instance_name attribute \"attr1\" = $instance_attr1"
puts "instance: $instance_name attribute \"attr2\" = $instance_attr2"
