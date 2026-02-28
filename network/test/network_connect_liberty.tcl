# Test network modify operations using liberty cell references
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]

# Force timing graph to build
report_checks

#---------------------------------------------------------------
# Make instance using liberty cell reference
#---------------------------------------------------------------
puts "--- make_instance using liberty cell ---"
set new_inst [make_instance lib_buf NangateOpenCellLibrary/BUF_X2]

#---------------------------------------------------------------
# Make nets for connections
#---------------------------------------------------------------
set net_a [make_net net_a]
set net_b [make_net net_b]

#---------------------------------------------------------------
# Connect using port names
#---------------------------------------------------------------
connect_pin net_a lib_buf/A

connect_pin net_b lib_buf/Z

#---------------------------------------------------------------
# Verify connections
#---------------------------------------------------------------
set lib_buf_pins [get_pins lib_buf/*]
puts "lib_buf pins: [llength $lib_buf_pins]"

foreach p $lib_buf_pins {
  puts "  pin: [get_full_name $p] dir=[get_property $p direction]"
}

#---------------------------------------------------------------
# Test get_fanin/get_fanout on new instances
#---------------------------------------------------------------
puts "--- fanin/fanout on new cells ---"
set fi [get_fanin -to [get_pins lib_buf/Z] -flat]
puts "fanin to lib_buf/Z: [llength $fi]"

set fo [get_fanout -from [get_pins lib_buf/A] -flat]
puts "fanout from lib_buf/A: [llength $fo]"

#---------------------------------------------------------------
# Disconnect and reconnect
#---------------------------------------------------------------
puts "--- disconnect and reconnect ---"
disconnect_pin net_a lib_buf/A

# Reconnect to a different net
connect_pin net_b lib_buf/A

# Disconnect everything
disconnect_pin net_b lib_buf/A
disconnect_pin net_b lib_buf/Z

#---------------------------------------------------------------
# Delete instance and nets
#---------------------------------------------------------------
delete_instance lib_buf

delete_net net_a
delete_net net_b

#---------------------------------------------------------------
# Test making multiple instances and verifying
#---------------------------------------------------------------
puts "--- multiple instance creation ---"
set inst1 [make_instance test_inv1 NangateOpenCellLibrary/INV_X1]
set inst2 [make_instance test_inv2 NangateOpenCellLibrary/INV_X2]
set inst3 [make_instance test_nand NangateOpenCellLibrary/NAND2_X1]

set all_cells [get_cells *]
puts "total cells after add: [llength $all_cells]"

# Test finding cells by pattern
set test_insts [get_cells test_*]
puts "test_* cells: [llength $test_insts]"

# Delete them
delete_instance test_inv1
delete_instance test_inv2
delete_instance test_nand

set all_cells2 [get_cells *]
puts "total cells after delete: [llength $all_cells2]"

#---------------------------------------------------------------
# Test replace_cell more extensively
#---------------------------------------------------------------
puts "--- replace_cell tests ---"

# Replace buf1 with BUF_X4
set rc1 [replace_cell buf1 NangateOpenCellLibrary/BUF_X4]
puts "replace_cell buf1 -> BUF_X4: $rc1"

set buf1_ref [get_property [get_cells buf1] ref_name]
puts "buf1 ref after replace: $buf1_ref"

# Report checks to ensure timing still works after replacement
report_checks

# Replace back
replace_cell buf1 NangateOpenCellLibrary/BUF_X1
