# verilog attribute parse/access via properties
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog verilog_attribute.v
link_design counter
create_clock -name clk [get_ports clk] -period 50

set instance [sta::top_instance]
set cell [$instance cell]
set cell_name [$cell name]
set src_location [get_property $cell src]
puts "top_instance:\"$cell_name\" attribute \"src\" = $src_location"

set instance_name "_1415_"
set inst [sta::find_instance $instance_name]
set instance_src_location [get_property $inst src]
set instance_attr1 [get_property $inst attr1]
set instance_attr2 [get_property $inst attr2]
puts "instance: $instance_name attribute \"src\" = $instance_src_location"
puts "instance: $instance_name attribute \"attr1\" = $instance_attr1"
puts "instance: $instance_name attribute \"attr2\" = $instance_attr2"
