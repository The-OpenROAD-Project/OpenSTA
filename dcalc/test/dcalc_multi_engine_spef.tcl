# Test multiple delay calc engines with SPEF parasitics and prima reduce order.
# Targets:
#   PrimaDelayCalc.cc: setPrimaReduceOrder, primaDelay, primaReduceRc,
#     prima2, prima3, primaResStamp, primaCapStamp, primaPostReduction
#   ArnoldiDelayCalc.cc: arnoldiDelay, arnoldiReduceRc, arnoldi2, arnoldi3,
#     arnoldiExpand, loadDelay, gateDelay, gateSlew
#   ArnoldiReduce.cc: arnoldi reduce matrix, arnoldi iteration
#   GraphDelayCalc.cc: findDelays with parasitic calc engine changes,
#     findVertexDelay with different parasitic reduction
#   DmpCeff.cc: ceffPiElmore, dmpCeffIter with SPEF parasitics
#   DelayCalc.i: delay_calc_names, is_delay_calc_name, set_prima_reduce_order

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
# Test 1: Enumerate delay calculator names
# Exercises: delayCalcNames, isDelayCalcName
#---------------------------------------------------------------
puts "--- Test 1: delay calc names ---"

set calc_names [sta::delay_calc_names]
puts "delay calc names: $calc_names"

foreach name {lumped_cap dmp_ceff_elmore dmp_ceff_two_pole arnoldi prima unit} {
  set valid [sta::is_delay_calc_name $name]
  puts "is_delay_calc_name $name: $valid"
}

# Invalid name
set invalid [sta::is_delay_calc_name "nonexistent_calc"]
puts "is_delay_calc_name nonexistent: $invalid"

#---------------------------------------------------------------
# Test 2: Read SPEF and run with default calculator
#---------------------------------------------------------------
puts "--- Test 2: SPEF with default calc ---"
read_spef ../../test/reg1_asap7.spef

report_checks

report_checks -path_delay min

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

#---------------------------------------------------------------
# Test 3: Prima with varying reduce order
# Exercises: setPrimaReduceOrder, prima reduction order paths
#---------------------------------------------------------------
puts "--- Test 3: prima with reduce order ---"
catch {set_delay_calculator prima} msg
puts "set prima: $msg"

# Default prima
report_checks

# Prima reduce order 1 (minimal)
catch {sta::set_prima_reduce_order 1} msg
puts "set_prima_reduce_order 1: $msg"
report_checks

# Prima reduce order 2
catch {sta::set_prima_reduce_order 2} msg
puts "set_prima_reduce_order 2: $msg"
report_checks

# Prima reduce order 3
catch {sta::set_prima_reduce_order 3} msg
puts "set_prima_reduce_order 3: $msg"
report_checks

# Prima reduce order 5 (higher order)
catch {sta::set_prima_reduce_order 5} msg
puts "set_prima_reduce_order 5: $msg"
report_checks

# report_dcalc with different orders
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "prima dcalc u1 order=5: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "prima dcalc u2 order=5: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "prima dcalc r1 order=5: done"

# Switch back to lower order
sta::set_prima_reduce_order 2
report_checks

# report_dcalc at order 2
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "prima dcalc u1 order=2: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max
puts "prima dcalc r3 order=2: done"

# Various slew values with prima
foreach slew_val {1 10 50 100 200} {
  set_input_transition $slew_val {in1 in2 clk1 clk2 clk3}
  report_checks
  puts "prima slew=$slew_val: done"
}
set_input_transition 10 {in1 in2 clk1 clk2 clk3}

#---------------------------------------------------------------
# Test 4: Arnoldi with SPEF
# Exercises: arnoldiDelay, arnoldiReduceRc, arnoldi expansion
#---------------------------------------------------------------
puts "--- Test 4: arnoldi with SPEF ---"
catch {set_delay_calculator arnoldi} msg
puts "set arnoldi: $msg"

report_checks

report_checks -path_delay min

# Various slew values with arnoldi
foreach slew_val {1 10 50 100} {
  set_input_transition $slew_val {in1 in2 clk1 clk2 clk3}
  report_checks
  puts "arnoldi slew=$slew_val: done"
}
set_input_transition 10 {in1 in2 clk1 clk2 clk3}

# Various load values with arnoldi
foreach load_val {0.0001 0.001 0.01 0.05} {
  set_load $load_val [get_ports out]
  report_checks
  puts "arnoldi load=$load_val: done"
}
set_load 0 [get_ports out]

# report_dcalc with arnoldi
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "arnoldi dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "arnoldi dcalc u2 A: done"

report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max
puts "arnoldi dcalc u2 B: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "arnoldi dcalc r1: done"

report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -min
puts "arnoldi dcalc r2 min: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max
puts "arnoldi dcalc r3: done"

# DFF check arcs
report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/D] -max
puts "arnoldi r1 setup: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/D] -min
puts "arnoldi r1 hold: done"

#---------------------------------------------------------------
# Test 5: Rapid engine switching with SPEF (reinit paths)
# Exercises: calculator copy/reinit during switching
#---------------------------------------------------------------
puts "--- Test 5: rapid engine switching ---"

set_delay_calculator lumped_cap
report_checks

set_delay_calculator dmp_ceff_elmore
report_checks

set_delay_calculator dmp_ceff_two_pole
report_checks

set_delay_calculator prima
report_checks

set_delay_calculator arnoldi
report_checks

set_delay_calculator unit
report_checks

set_delay_calculator dmp_ceff_elmore
report_checks

#---------------------------------------------------------------
# Test 6: find_delays explicit call
# Exercises: findDelays direct path
#---------------------------------------------------------------
puts "--- Test 6: find_delays ---"

sta::find_delays

sta::delays_invalid
sta::find_delays

#---------------------------------------------------------------
# Test 7: Detailed report_checks with various formats after SPEF
#---------------------------------------------------------------
puts "--- Test 7: report formats ---"

report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

report_checks -format full_clock_expanded

report_checks -endpoint_count 3

report_checks -group_count 2

report_checks -digits 6
