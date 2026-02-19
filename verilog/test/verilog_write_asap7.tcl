# Test write verilog ASAP7 design (different cell naming)

source ../../test/helpers.tcl

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

set out1 [make_result_file verilog_write_asap7.v]
write_verilog $out1

set out2 [make_result_file verilog_write_asap7_pwr.v]
write_verilog -include_pwr_gnd $out2

# Write with remove_cells
set out3 [make_result_file verilog_write_asap7_remove.v]
write_verilog -remove_cells {} $out3

# Compare sizes
set sz1 [file size $out1]
set sz2 [file size $out2]
set sz3 [file size $out3]
puts "ASAP7 basic: $sz1, pwr_gnd: $sz2, remove_cells: $sz3"
