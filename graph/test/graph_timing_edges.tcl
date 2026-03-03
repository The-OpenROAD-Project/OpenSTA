# Test graph timing edge queries and disable_timing
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog graph_test1.v
link_design graph_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports d]
set_output_delay -clock clk 0 [get_ports q]

puts "--- get_timing_edges -of_objects instance ---"
set edges [get_timing_edges -of_objects [get_cells reg1]]
puts "reg1 timing edges count: [llength $edges]"

puts "--- get_timing_edges -from/-to on instance ---"
set edges_ft [get_timing_edges -from [get_pins reg1/CK] -to [get_pins reg1/Q]]
puts "CK->Q edges count: [llength $edges_ft]"

puts "--- get_timing_edges -from only ---"
set edges_from [get_timing_edges -from [get_pins reg1/CK]]
puts "edges from CK count: [llength $edges_from]"

puts "--- get_timing_edges -to only ---"
set edges_to [get_timing_edges -to [get_pins reg1/Q]]
puts "edges to Q count: [llength $edges_to]"

puts "--- report_edges -from/-to ---"
report_edges -from [get_pins reg1/CK] -to [get_pins reg1/Q]

puts "--- report_edges -from ---"
report_edges -from [get_pins reg1/CK]

puts "--- report_edges -to ---"
report_edges -to [get_pins reg2/D]

puts "--- report_disabled_edges (baseline) ---"
report_disabled_edges

puts "--- set_disable_timing on instance ---"
set_disable_timing [get_cells reg1]

puts "--- report_disabled_edges after disable ---"
report_disabled_edges

puts "--- report_checks after disable ---"
report_checks

puts "--- unset_disable_timing on instance ---"
unset_disable_timing [get_cells reg1]

puts "--- report_disabled_edges after unset ---"
report_disabled_edges

puts "--- set_disable_timing with -from/-to on lib cell ---"
set_disable_timing -from CK -to Q [get_lib_cells NangateOpenCellLibrary/DFF_X1]

puts "--- report_disabled_edges after lib cell disable ---"
report_disabled_edges

puts "--- unset_disable_timing lib cell ---"
unset_disable_timing -from CK -to Q [get_lib_cells NangateOpenCellLibrary/DFF_X1]

puts "--- report_checks baseline ---"
report_checks

puts "--- report_checks -path_delay max ---"
report_checks -path_delay max

puts "--- report_checks -path_delay min ---"
report_checks -path_delay min

puts "--- report_checks from d to q ---"
report_checks -from [get_ports d] -to [get_ports q]

puts "--- report_edges -from port d ---"
report_edges -from [get_ports d]

puts "--- report_edges -to port q ---"
report_edges -to [get_ports q]

puts "--- get_timing_edges -of_objects reg2 ---"
set edges_r2 [get_timing_edges -of_objects [get_cells reg2]]
puts "reg2 timing edges count: [llength $edges_r2]"

puts "--- report_slews on d port ---"
report_slews [get_ports d]

puts "--- report_slews on q port ---"
report_slews [get_ports q]
