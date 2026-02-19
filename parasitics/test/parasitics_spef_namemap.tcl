# Test SPEF reader with different name mapping styles and corner handling
# Targets: SpefReader.cc uncovered functions:
#   findPinRelative (line 176, hit=0)
#   findPortPinRelative (line 182, hit=0)
#   findInstanceRelative (line 159, hit=0)
#   setDesignFlow (line 300)
#   setDivider, setDelimiter, setBusBrackets
#   setCapScale, setResScale, setTimeScale, setInductScale
# Also targets: SpefParse.yy (more parser paths)
#   SpefLex.ll (more lexer paths)
#   ConcreteParasitics.cc (parasitic network creation/query paths)
#   Parasitics.cc (findParasiticNetwork, makeParasiticNetwork)

# Read ASAP7 libraries
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 [get_ports out]
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
set_propagated_clock {clk1 clk2 clk3}

#---------------------------------------------------------------
# Test 1: Read SPEF and check parasitic annotation
#---------------------------------------------------------------
puts "--- Test 1: basic SPEF read ---"
read_spef ../../test/reg1_asap7.spef

report_checks

#---------------------------------------------------------------
# Test 2: Check parasitic annotation reporting
#---------------------------------------------------------------
puts "--- Test 2: parasitic annotation ---"
report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Test 3: Verify net-level parasitic queries
#---------------------------------------------------------------
puts "--- Test 3: net parasitic queries ---"
foreach net_name {r1q r2q u1z u2z in1 in2 clk1 clk2 clk3 out} {
  report_net $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Test 4: Report with various digit precisions
#---------------------------------------------------------------
puts "--- Test 4: report_net with digits ---"
foreach digits {2 4 6 8} {
  report_net -digits $digits r1q
  puts "report_net -digits $digits r1q: done"
}

foreach digits {2 4 6} {
  report_net -digits $digits u1z
  puts "report_net -digits $digits u1z: done"
}

#---------------------------------------------------------------
# Test 5: Timing with SPEF across different paths
#---------------------------------------------------------------
puts "--- Test 5: timing paths with SPEF ---"
report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

report_checks -path_delay min

report_checks -path_delay max

report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock_expanded

#---------------------------------------------------------------
# Test 6: Delay calculation with SPEF parasitics
#---------------------------------------------------------------
puts "--- Test 6: delay calculation ---"
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "dcalc u1 A->Y: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "dcalc u2 A->Y: done"

report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max
puts "dcalc u2 B->Y: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "dcalc r1 CLK->Q: done"

report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max
puts "dcalc r2 CLK->Q: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max
puts "dcalc r3 CLK->Q: done"

#---------------------------------------------------------------
# Test 7: Annotated delay reporting
#---------------------------------------------------------------
puts "--- Test 7: annotated delay ---"
report_annotated_delay -cell -net
puts "annotated -cell -net: done"

report_annotated_delay -from_in_ports -to_out_ports
puts "annotated -from_in_ports -to_out_ports: done"

report_annotated_delay -report_annotated
puts "annotated -report_annotated: done"

report_annotated_delay -report_unannotated
puts "annotated -report_unannotated: done"

#---------------------------------------------------------------
# Test 8: Report checks with different endpoint grouping
#---------------------------------------------------------------
puts "--- Test 8: endpoint grouping ---"
report_checks -group_path_count 5

report_checks -sort_by_slack
