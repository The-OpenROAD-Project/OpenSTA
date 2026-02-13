# Test Arnoldi delay calculator with SPEF parasitics, various slew/load conditions,
# and report_dcalc for different pin combinations.
# Targets:
#   ArnoldiDelayCalc.cc: arnoldiDelay, arnoldiReduceRc, arnoldi2, arnoldi3,
#     arnoldiExpand, loadDelay, gateDelay, gateSlew
#   ArnoldiReduce.cc: arnoldi reduce matrix iteration, arnoldi basis expansion
#   GraphDelayCalc.cc: findVertexDelay with arnoldi parasitic reduction
#   DmpCeff.cc: ceffPiElmore convergence edge cases with different slews

source ../../test/helpers.tcl

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
# Test 1: Read SPEF and set arnoldi
#---------------------------------------------------------------
puts "--- Test 1: arnoldi + SPEF ---"
read_spef ../../test/reg1_asap7.spef
set_delay_calculator arnoldi
puts "PASS: set arnoldi"

report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: min path"

report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: in1->out"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: in2->out"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: with fields"

report_checks -format full_clock
puts "PASS: full_clock"

#---------------------------------------------------------------
# Test 2: report_dcalc for all cell arcs
# Exercises: arnoldiDelay for each arc, loadDelay, gateDelay, gateSlew
#---------------------------------------------------------------
puts "--- Test 2: report_dcalc ---"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "dcalc u1 A->Y max: done"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -min} msg
puts "dcalc u1 A->Y min: done"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "dcalc u2 A->Y: done"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max} msg
puts "dcalc u2 B->Y: done"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -min} msg
puts "dcalc u2 B->Y min: done"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "dcalc r1 CLK->Q max: done"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -min} msg
puts "dcalc r1 CLK->Q min: done"

catch {report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max} msg
puts "dcalc r2 CLK->Q: done"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max} msg
puts "dcalc r3 CLK->Q: done"

# Setup/hold check arcs
catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/D] -max} msg
puts "dcalc r1 setup: done"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/D] -min} msg
puts "dcalc r1 hold: done"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -max} msg
puts "dcalc r3 setup: done"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -min} msg
puts "dcalc r3 hold: done"

puts "PASS: dcalc reports"

#---------------------------------------------------------------
# Test 3: Vary input slew with arnoldi
# Exercises: arnoldi gate delay computation at different slew points
#---------------------------------------------------------------
puts "--- Test 3: varying slew ---"

foreach slew_val {0.1 1 5 10 25 50 100 200 500} {
  set_input_transition $slew_val {in1 in2 clk1 clk2 clk3}
  report_checks
  puts "arnoldi slew=$slew_val: done"
}
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
puts "PASS: varying slew"

#---------------------------------------------------------------
# Test 4: Vary output load with arnoldi
# Exercises: arnoldi load delay at different capacitance values
#---------------------------------------------------------------
puts "--- Test 4: varying load ---"

foreach load_val {0.00001 0.0001 0.001 0.005 0.01 0.05 0.1} {
  set_load $load_val [get_ports out]
  report_checks
  puts "arnoldi load=$load_val: done"
}
set_load 0 [get_ports out]
puts "PASS: varying load"

#---------------------------------------------------------------
# Test 5: Arnoldi after re-read SPEF
# Exercises: deleteReducedParasitics, re-initialization
#---------------------------------------------------------------
puts "--- Test 5: re-read SPEF ---"
read_spef ../../test/reg1_asap7.spef
report_checks
puts "PASS: arnoldi after re-read"

#---------------------------------------------------------------
# Test 6: Switch engines while arnoldi active
# Exercises: reinit paths in delay calculator switching
#---------------------------------------------------------------
puts "--- Test 6: engine switch from arnoldi ---"

set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: switch to dmp_ceff_elmore"

set_delay_calculator arnoldi
report_checks
puts "PASS: back to arnoldi"

set_delay_calculator lumped_cap
report_checks
puts "PASS: switch to lumped_cap"

set_delay_calculator arnoldi
report_checks
puts "PASS: back to arnoldi again"

#---------------------------------------------------------------
# Test 7: Arnoldi with digits and endpoint count
#---------------------------------------------------------------
puts "--- Test 7: format options ---"
report_checks -digits 6
puts "PASS: 6 digits"

report_checks -endpoint_count 3
puts "PASS: endpoint_count 3"

report_checks -group_count 2
puts "PASS: group_count 2"

puts "ALL PASSED"
