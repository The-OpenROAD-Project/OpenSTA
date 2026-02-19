# report_checks all fields enabled
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog verilog_attribute.v
link_design counter

create_clock -name clk -period 10 clk
set_input_delay -clock clk 0 [all_inputs -no_clocks]
report_checks -path_group clk -fields {capacitance slew input_pin net src_attr}

set module_name "_141"
set module_pattern "${module_name}*"
set all_module_cells [get_cells $module_pattern]
puts "Creating path group with [llength $all_module_cells] cells"

group_path -from $all_module_cells -to $all_module_cells -name "crash_group"
set x [sta::path_group_names]
puts $x
report_checks -path_group "crash_group"

group_path -from $all_module_cells -to $all_module_cells -default
set x [sta::path_group_names]
puts $x
report_checks -path_group "crash_group"
