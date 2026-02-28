# Test VerilogReader and VerilogWriter with isolated roundtrip scenarios.
# Each scenario starts from a clean STA state to keep output stable.
source ../../test/helpers.tcl

proc load_nangate_design {verilog_file top_name} {
  global nangate_lib_loaded
  sta::clear_sta
  if { !$nangate_lib_loaded } {
    read_liberty ../../test/nangate45/Nangate45_typ.lib
    set nangate_lib_loaded 1
  }
  read_verilog $verilog_file
  link_design $top_name
}

proc load_asap7_design {verilog_file top_name} {
  sta::clear_sta
  read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
  read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
  read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
  read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
  read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz
  read_verilog $verilog_file
  link_design $top_name
}

############################################################
# Scenario 1: Nangate write options
############################################################
puts "--- Nangate write options ---"
set nangate_lib_loaded 0
load_nangate_design ../../examples/example1.v top
puts "cells: [llength [get_cells *]]"

set out1 [make_result_file verilog_mm_default.v]
write_verilog $out1

set out2 [make_result_file verilog_mm_pwr.v]
write_verilog -include_pwr_gnd $out2

set out3 [make_result_file verilog_mm_sort.v]
write_verilog -sort $out3

set out4 [make_result_file verilog_mm_pwr_sort.v]
write_verilog -include_pwr_gnd -sort $out4

############################################################
# Scenario 2: Nangate default roundtrip + timing/queries
############################################################
puts "--- Nangate default roundtrip ---"
load_nangate_design $out1 top
puts "re-read default cells: [llength [get_cells *]]"

create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
set_output_delay -clock clk 0 [get_ports out]
set_input_transition 0.1 [all_inputs]

report_checks
report_checks -path_delay min
report_checks -fields {slew cap input_pins fanout}

foreach inst_name {r1 r2 r3 u1 u2} {
  set inst [get_cells $inst_name]
  puts "$inst_name ref=[get_property $inst ref_name]"
}

foreach net_name {r1q r2q u1z u2z} {
  report_net $net_name
}

############################################################
# Scenario 3: Nangate alternative roundtrip inputs
############################################################
puts "--- Nangate pwr roundtrip ---"
load_nangate_design $out2 top
puts "re-read pwr cells: [llength [get_cells *]]"

puts "--- Nangate sorted roundtrip ---"
load_nangate_design $out3 top
puts "re-read sorted cells: [llength [get_cells *]]"

create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks

############################################################
# Scenario 4: ASAP7 write options
############################################################
puts "--- ASAP7 write options ---"
load_asap7_design ../../test/reg1_asap7.v top
puts "asap7 cells: [llength [get_cells *]]"

set out5 [make_result_file verilog_mm_asap7.v]
write_verilog $out5

set out6 [make_result_file verilog_mm_asap7_pwr.v]
write_verilog -include_pwr_gnd $out6

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 [get_ports out]
report_checks
