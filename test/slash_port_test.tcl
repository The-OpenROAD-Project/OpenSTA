# Test: ports with escaped names containing slashes
# Exercises Port-to-Pin conversion paths with escaped Verilog identifiers
# like \level1/level2/level3 that contain hierarchy separators
read_liberty asap7_small.lib.gz
read_verilog slash_port_test.v
link_design slash_port_test

create_clock -name clk -period 500 [get_ports out]

puts {get_ports *}
report_object_full_names [get_ports *]

puts {all_inputs}
report_object_full_names [all_inputs]

puts {all_outputs}
report_object_full_names [all_outputs]

puts {get_pins *}
report_object_full_names [get_pins *]

puts {get_cells *}
report_object_full_names [get_cells *]

puts {get_nets *}
report_object_full_names [get_nets *]

puts {get_ports * -filter direction==input}
report_object_full_names [get_ports * -filter {direction == input}]

puts {get_ports * -filter direction==output}
report_object_full_names [get_ports * -filter {direction == output}]

# get_port_pins_error: Port objects via set_input_delay [all_inputs]
puts {set_input_delay -clock clk 0 [all_inputs]}
set_input_delay -clock clk 0 [all_inputs]

# get_port_pins_error: individual Port objects
foreach port [all_inputs] {
  puts "set_input_delay -clock clk 0 <Port '[get_name $port]'>"
  set_input_delay -clock clk 0 $port
}

# parse_clk_inst_port_pin_arg: Port objects via set_false_path
puts {set_false_path -from [all_inputs] -to [all_outputs]}
set_false_path -from [all_inputs] -to [all_outputs]

# get_port_pin_error (singular): set_data_check with Port objects
set input_ports [all_inputs]
set from_port [lindex $input_ports 0]
set to_port [lindex $input_ports 1]
puts "set_data_check -from <Port '[get_name $from_port]'> -to <Port '[get_name $to_port]'> 0"
set_data_check -from $from_port -to $to_port 0

# get_port_pin_error (singular): set_case_analysis with Port objects
foreach port [all_inputs] {
  puts "set_case_analysis 0 <Port '[get_name $port]'>"
  set_case_analysis 0 $port
}

puts {report_checks}
report_checks
