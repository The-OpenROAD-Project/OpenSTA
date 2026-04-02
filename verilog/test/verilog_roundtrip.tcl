# Test verilog read/write roundtrip
source ../../test/helpers.tcl
set test_name verilog_roundtrip

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_test1.v
link_design verilog_test1

# Write verilog
set verilog_out [make_result_file $test_name.v]
write_verilog $verilog_out

diff_files $test_name.vok $verilog_out
