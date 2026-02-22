# Test advanced verilog writer options - Write after modification
source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 2: Write after network modification
#---------------------------------------------------------------
puts "--- Test 2: Write after modification ---"

# Need to load a design first to modify
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_verilog verilog_test1.v
link_design verilog_test1

# Add an instance and net
set new_net [make_net extra_net]
# Using ASAP7 cell here as in original test
set new_inst [make_instance extra_buf asap7sc7p5t_INVBUF_RVT/BUFx2_ASAP7_75t_R]
connect_pin extra_net extra_buf/A

set out4 [make_result_file verilog_advanced_out4.v]
write_verilog $out4

set sz4 [file size $out4]
puts "modified size: $sz4"
# Disconnect and delete
disconnect_pin extra_net extra_buf/A
delete_instance extra_buf
delete_net extra_net
