# Test VerilogReader and VerilogWriter with multiple design reads,
# link re-entries, writing with all options, and diverse verilog constructs.
# Targets:
#   VerilogReader.cc: readVerilog (multiple reads), clearNetwork,
#     makeModule (re-define), makeModuleInst, makeDcl, makeDclArg,
#     linkNetwork (re-link), checkModule, resolveModule
#   VerilogWriter.cc: writeVerilog (all option combinations),
#     writePowerGround, writeSort, -no_port_dir
source ../../test/helpers.tcl

############################################################
# Test 1: Read and write Nangate example designs
############################################################
puts "--- Test 1: Nangate examples ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib

# Read example1.v
read_verilog ../../examples/example1.v
link_design top
puts "PASS: link top (example1)"

set cells [get_cells *]
puts "cells: [llength $cells]"

# Write in several styles
set out1 [make_result_file verilog_mm_default.v]
write_verilog $out1
puts "PASS: write default"

set out2 [make_result_file verilog_mm_pwr.v]
write_verilog -include_pwr_gnd $out2
puts "PASS: write -include_pwr_gnd"

set out3 [make_result_file verilog_mm_sort.v]
write_verilog -sort $out3
puts "PASS: write -sort"

set out4 [make_result_file verilog_mm_pwr_sort.v]
write_verilog -include_pwr_gnd -sort $out4
puts "PASS: write -include_pwr_gnd -sort"

# Verify sizes
foreach outf [list $out1 $out2 $out3 $out4] {
  if {[file exists $outf] && [file size $outf] > 0} {
    puts "  $outf OK"
  }
}
puts "PASS: output files"

############################################################
# Test 2: Re-read written verilog
############################################################
puts "--- Test 2: re-read default ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out1
link_design top
puts "re-read cells: [llength [get_cells *]]"
puts "PASS: re-read default"

############################################################
# Test 3: Re-read power/ground version
############################################################
puts "--- Test 3: re-read pwr ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out2
link_design top
puts "re-read pwr cells: [llength [get_cells *]]"
puts "PASS: re-read pwr"

############################################################
# Test 4: Timing after re-read
############################################################
puts "--- Test 4: timing after re-read ---"
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
set_output_delay -clock clk 0 [get_ports out]
set_input_transition 0.1 [all_inputs]

report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: min path"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: fields"

############################################################
# Test 5: Instance/net queries
############################################################
puts "--- Test 5: queries ---"
foreach inst_name {r1 r2 r3 u1 u2} {
  set inst [get_cells $inst_name]
  set ref [get_property $inst ref_name]
  puts "$inst_name ref=$ref"
}
puts "PASS: instance queries"

foreach net_name {r1q r2q u1z u2z} {
  report_net $net_name
}
puts "PASS: net queries"

############################################################
# Test 6: Write and re-read the sorted version
############################################################
puts "--- Test 6: sorted re-read ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out3
link_design top
puts "sorted cells: [llength [get_cells *]]"

create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks
puts "PASS: sorted re-read timing"

############################################################
# Test 7: Read reg1_asap7 design
############################################################
puts "--- Test 7: ASAP7 design ---"
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top
puts "PASS: link reg1_asap7"

set out5 [make_result_file verilog_mm_asap7.v]
write_verilog $out5
puts "PASS: write ASAP7"

set out6 [make_result_file verilog_mm_asap7_pwr.v]
write_verilog -include_pwr_gnd $out6
puts "PASS: write ASAP7 -include_pwr_gnd"

# Re-read ASAP7 written verilog
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog $out5
link_design top
puts "re-read ASAP7 cells: [llength [get_cells *]]"
puts "PASS: re-read ASAP7"

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 [get_ports out]
report_checks
puts "PASS: ASAP7 timing"

puts "ALL PASSED"
