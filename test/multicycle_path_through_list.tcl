# set_multicycle_path -through with long multi-line pin name lists
source helpers.tcl
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

set sdc_file [make_result_file multicycle_path_through_list.sdc]
set stream [open $sdc_file w]
puts $stream "set_multicycle_path -setup 2 -through \[list \\"
for { set i 0 } { $i < 2000 } { incr i } {
  puts $stream "    u1/A \\"
  puts $stream "    u1/Z \\"
}
puts $stream "    \[get_pins u2/A2\] \\"
puts $stream "  \]"
puts $stream "set_multicycle_path -hold 1 -through {"
for { set i 0 } { $i < 2000 } { incr i } {
  puts $stream "    u1/A"
  puts $stream "    u1/Z"
}
puts $stream "  }"
puts $stream "puts \"read"
puts $stream "  multicycle paths\""
close $stream

read_sdc $sdc_file
report_checks -through u1/Z -path_delay min_max -format end
report_checks -through u2/A1 -path_delay min_max -format end
