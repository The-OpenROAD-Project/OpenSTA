# verilog attribute parse/access via properties
read_liberty ../examples/asap7_small_ss.lib.gz
read_verilog verilog_attribute.v
link_design counter
create_clock -name clk [get_ports clk] -period 50

set top_inst [sta::top_instance]
set top_cell [$top_inst cell]
set src_location [get_property $top_cell "src"]
puts "top_cell: attribute \"src\" = $src_location"

set instance_name "_1415_"
set inst [get_cell $instance_name]
set instance_src_location [get_property $inst src]
set instance_attr1 [get_property $inst attr1]
set instance_attr2 [get_property $inst attr2]
puts "instance: $instance_name attribute \"src\" = $instance_src_location"
puts "instance: $instance_name attribute \"attr1\" = $instance_attr1"
puts "instance: $instance_name attribute \"attr2\" = $instance_attr2"

define_property -object_type pin -type string src
set pin [get_pin _1415_/Q]
set_property $pin src "synthesis/tests/counter.v:25.5-25.10"
puts "pin: _1415_/Q attribute \"src\" = [get_property $pin src]"

report_checks -fields {src_attr}
