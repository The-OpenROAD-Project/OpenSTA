# Test multi-corner liberty reading for code coverage
define_corners fast slow

read_liberty -corner fast ../../test/nangate45/Nangate45_fast.lib
read_liberty -corner slow ../../test/nangate45/Nangate45_slow.lib
puts "PASS: multi-corner liberty read"

# Verify both corners loaded
set fast_lib [get_libs NangateOpenCellLibrary_fast]
if { $fast_lib == "" } {
  puts "FAIL: fast library not found"
  exit 1
}
puts "PASS: fast library loaded"

set slow_lib [get_libs NangateOpenCellLibrary_slow]
if { $slow_lib == "" } {
  puts "FAIL: slow library not found"
  exit 1
}
puts "PASS: slow library loaded"

# Query cells in each corner
set fast_inv [get_lib_cells NangateOpenCellLibrary_fast/INV_X1]
if { $fast_inv == "" } {
  puts "FAIL: fast INV_X1 not found"
  exit 1
}
puts "PASS: fast INV_X1 found"

set slow_inv [get_lib_cells NangateOpenCellLibrary_slow/INV_X1]
if { $slow_inv == "" } {
  puts "FAIL: slow INV_X1 not found"
  exit 1
}
puts "PASS: slow INV_X1 found"

# Read verilog and link
read_verilog ../../sdc/test/sdc_test1.v
link_design sdc_test1
puts "PASS: link_design"

# Setup constraints
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 2.0 [get_ports in1]
set_output_delay -clock clk 3.0 [get_ports out1]
puts "PASS: constraints set"

# Report for each corner
report_checks -corner fast
puts "PASS: report_checks -corner fast"

report_checks -corner slow
puts "PASS: report_checks -corner slow"

# Report clock properties
report_clock_properties
puts "PASS: report_clock_properties"

# Report with path details
report_checks -corner fast -path_delay min
puts "PASS: report_checks -corner fast -path_delay min"

report_checks -corner slow -path_delay max
puts "PASS: report_checks -corner slow -path_delay max"

# Query lib cells from both corners
report_lib_cell NangateOpenCellLibrary_fast/BUF_X1
puts "PASS: report_lib_cell fast BUF_X1"

report_lib_cell NangateOpenCellLibrary_slow/BUF_X1
puts "PASS: report_lib_cell slow BUF_X1"

report_lib_cell NangateOpenCellLibrary_fast/DFF_X1
puts "PASS: report_lib_cell fast DFF_X1"

report_lib_cell NangateOpenCellLibrary_slow/DFF_X1
puts "PASS: report_lib_cell slow DFF_X1"

# Get lib pins from both corners
set fast_buf_pins [get_lib_pins NangateOpenCellLibrary_fast/BUF_X1/*]
puts "PASS: fast BUF_X1 pins ([llength $fast_buf_pins] pins)"

set slow_buf_pins [get_lib_pins NangateOpenCellLibrary_slow/BUF_X1/*]
puts "PASS: slow BUF_X1 pins ([llength $slow_buf_pins] pins)"

puts "ALL PASSED"
