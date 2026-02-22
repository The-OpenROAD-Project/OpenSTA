# Test 6: Write assign/tristate design with removes
source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 6: Write assign/tristate design with removes
#---------------------------------------------------------------
puts "--- Test 6: supply/tristate with removes ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_supply_tristate.v
link_design verilog_supply_tristate

set out_st_rm [make_result_file verilog_remove_supply_buf.v]
write_verilog -remove_cells {NangateOpenCellLibrary/BUF_X1} $out_st_rm

set out_st_pwr [make_result_file verilog_remove_supply_pwr.v]
write_verilog -include_pwr_gnd -remove_cells {NangateOpenCellLibrary/INV_X1} $out_st_pwr

# Sizes
set sz_st_rm [file size $out_st_rm]
set sz_st_pwr [file size $out_st_pwr]
puts "supply remove sizes: buf=$sz_st_rm inv_pwr=$sz_st_pwr"
