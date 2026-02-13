# Test SDF read and annotation
source ../../test/helpers.tcl
set test_name sdf_read_write

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdf_test1.v
link_design sdf_test1

create_clock -name clk -period 10 [get_ports clk]

# Read SDF
read_sdf sdf_test1.sdf
puts "PASS: read_sdf completed"

# Write SDF
set sdf_out [make_result_file $test_name.sdf]
write_sdf $sdf_out
puts "PASS: write_sdf completed"

diff_files $test_name.sdfok $sdf_out {\(DATE}

puts "ALL PASSED"
