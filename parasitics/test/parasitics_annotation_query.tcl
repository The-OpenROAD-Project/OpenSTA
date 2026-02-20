# Test parasitic annotation queries, find_pi_elmore, find_elmore, and
# detailed SPEF operations for coverage improvement.

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
# Test 1: Set manual pi model and query back
# Exercises: makePiElmore, setPiModel, setElmore, findPiElmore, findElmore
#---------------------------------------------------------------
puts "--- Test 1: set and query pi model ---"

# Set pi models on driver pins (both rise and fall)
set msg [sta::set_pi_model u1/Y 0.005 10.0 0.003]
puts "set_pi_model u1/Y: $msg"

set msg [sta::set_pi_model u2/Y 0.008 15.0 0.005]
puts "set_pi_model u2/Y: $msg"

set msg [sta::set_pi_model r1/Q 0.002 5.0 0.001]
puts "set_pi_model r1/Q: $msg"

set msg [sta::set_pi_model r2/Q 0.003 6.0 0.002]
puts "set_pi_model r2/Q: $msg"

set msg [sta::set_pi_model r3/Q 0.001 2.0 0.001]
puts "set_pi_model r3/Q: $msg"

# Query pi models back using find_pi_elmore
# find_pi_elmore returns {c2 rpi c1} or empty list
puts "--- query pi_elmore ---"
set pi_u1 [sta::find_pi_elmore [get_pins u1/Y] "rise" "max"]
puts "u1/Y rise max pi: $pi_u1"

set pi_u1_f [sta::find_pi_elmore [get_pins u1/Y] "fall" "max"]
puts "u1/Y fall max pi: $pi_u1_f"

set pi_u2 [sta::find_pi_elmore [get_pins u2/Y] "rise" "max"]
puts "u2/Y rise max pi: $pi_u2"

set pi_r1 [sta::find_pi_elmore [get_pins r1/Q] "rise" "max"]
puts "r1/Q rise max pi: $pi_r1"

set pi_r1_min [sta::find_pi_elmore [get_pins r1/Q] "rise" "min"]
puts "r1/Q rise min pi: $pi_r1_min"

set pi_r2 [sta::find_pi_elmore [get_pins r2/Q] "fall" "max"]
puts "r2/Q fall max pi: $pi_r2"

set pi_r3 [sta::find_pi_elmore [get_pins r3/Q] "rise" "max"]
puts "r3/Q rise max pi: $pi_r3"

#---------------------------------------------------------------
# Test 2: Set elmore delays and query back
# Exercises: setElmore, findElmore
#---------------------------------------------------------------
puts "--- Test 2: set and query elmore ---"

set msg [sta::set_elmore u1/Y u2/A 0.005]
puts "set_elmore u1/Y -> u2/A: $msg"

set msg [sta::set_elmore u2/Y r3/D 0.008]
puts "set_elmore u2/Y -> r3/D: $msg"

set msg [sta::set_elmore r1/Q u1/A 0.003]
puts "set_elmore r1/Q -> u1/A: $msg"

set msg [sta::set_elmore r2/Q u2/B 0.004]
puts "set_elmore r2/Q -> u2/B: $msg"

set msg [sta::set_elmore r3/Q out 0.002]
puts "set_elmore r3/Q -> out: $msg"

# Query elmore values back
set elm_u1 [sta::find_elmore [get_pins u1/Y] [get_pins u2/A] "rise" "max"]
puts "elmore u1/Y -> u2/A rise max: $elm_u1"

set elm_u2 [sta::find_elmore [get_pins u2/Y] [get_pins r3/D] "rise" "max"]
puts "elmore u2/Y -> r3/D rise max: $elm_u2"

set elm_r1 [sta::find_elmore [get_pins r1/Q] [get_pins u1/A] "rise" "max"]
puts "elmore r1/Q -> u1/A rise max: $elm_r1"

set elm_r1f [sta::find_elmore [get_pins r1/Q] [get_pins u1/A] "fall" "max"]
puts "elmore r1/Q -> u1/A fall max: $elm_r1f"

set elm_r2 [sta::find_elmore [get_pins r2/Q] [get_pins u2/B] "rise" "max"]
puts "elmore r2/Q -> u2/B rise max: $elm_r2"

catch {
  set elm_r3 [sta::find_elmore [get_pins r3/Q] [get_pins out] "rise" "max"]
  puts "elmore r3/Q -> out rise max: $elm_r3"
} msg

# Query min as well
catch {
  set elm_r3_min [sta::find_elmore [get_pins r3/Q] [get_pins out] "rise" "min"]
  puts "elmore r3/Q -> out rise min: $elm_r3_min"
} msg

#---------------------------------------------------------------
# Test 3: Report timing with manual parasitics
#---------------------------------------------------------------
puts "--- Test 3: timing with manual parasitics ---"

report_checks

report_checks -path_delay min

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

report_checks -fields {slew cap input_pins nets fanout}

#---------------------------------------------------------------
# Test 4: Report parasitic annotation
# Exercises: reportParasiticAnnotation
#---------------------------------------------------------------
puts "--- Test 4: parasitic annotation ---"

report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Test 5: Override manual with different values
# Exercises: setPiModel/setElmore override existing
#---------------------------------------------------------------
puts "--- Test 5: override manual parasitics ---"

# Re-set pi model with different values
set msg [sta::set_pi_model u1/Y 0.01 20.0 0.008]
puts "re-set pi_model u1/Y: $msg"

set msg [sta::set_pi_model u2/Y 0.02 30.0 0.01]
puts "re-set pi_model u2/Y: $msg"

# Re-set elmore with different values
set msg [sta::set_elmore u1/Y u2/A 0.01]
puts "re-set elmore u1/Y -> u2/A: $msg"

set msg [sta::set_elmore u2/Y r3/D 0.02]
puts "re-set elmore u2/Y -> r3/D: $msg"

report_checks

# Query overridden values
set pi_u1_new [sta::find_pi_elmore [get_pins u1/Y] "rise" "max"]
puts "u1/Y rise max pi (new): $pi_u1_new"

set elm_u1_new [sta::find_elmore [get_pins u1/Y] [get_pins u2/A] "rise" "max"]
puts "elmore u1/Y -> u2/A (new): $elm_u1_new"

#---------------------------------------------------------------
# Test 6: Now override with SPEF
# Exercises: readSpef overrides manual parasitics
#---------------------------------------------------------------
puts "--- Test 6: SPEF override ---"
read_spef ../../test/reg1_asap7.spef

report_checks

report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Test 7: query parasitics after SPEF read
# Exercises: findPiElmore after SPEF reduction
#---------------------------------------------------------------
puts "--- Test 7: query parasitics after SPEF ---"

set pi_u1_spef [sta::find_pi_elmore [get_pins u1/Y] "rise" "max"]
puts "u1/Y pi after SPEF: $pi_u1_spef"

set pi_u2_spef [sta::find_pi_elmore [get_pins u2/Y] "rise" "max"]
puts "u2/Y pi after SPEF: $pi_u2_spef"

set pi_r1_spef [sta::find_pi_elmore [get_pins r1/Q] "rise" "max"]
puts "r1/Q pi after SPEF: $pi_r1_spef"

set elm_u1_spef [sta::find_elmore [get_pins u1/Y] [get_pins u2/A] "rise" "max"]
puts "elmore u1/Y->u2/A after SPEF: $elm_u1_spef"

set elm_r1_spef [sta::find_elmore [get_pins r1/Q] [get_pins u1/A] "rise" "max"]
puts "elmore r1/Q->u1/A after SPEF: $elm_r1_spef"

catch {
  set elm_r3_spef [sta::find_elmore [get_pins r3/Q] [get_pins out] "rise" "max"]
  puts "elmore r3/Q->out after SPEF: $elm_r3_spef"
} msg

#---------------------------------------------------------------
# Test 8: Detailed report with various formats
#---------------------------------------------------------------
puts "--- Test 8: detailed reports ---"

report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

# report_net with SPEF parasitics
foreach net_name {r1q r2q u1z u2z out in1 in2 clk1 clk2 clk3} {
  report_net -digits 6 $net_name
  puts "report_net $net_name: done"
}

# Dcalc reports
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max -digits 6
puts "dcalc u1 6 digits: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max -digits 4
puts "dcalc u2 4 digits: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "dcalc r1 CK->Q: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max
puts "dcalc r3 CK->Q: done"
