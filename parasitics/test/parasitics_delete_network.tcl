# Test parasitic deletion, network cleanup, and re-read flows.
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
# Test 1: Set pi+elmore, run timing, then delete parasitics
# Exercises: makePiElmore, deleteParasitics, deleteReducedParasitics
#---------------------------------------------------------------
puts "--- Test 1: set then delete manual parasitics ---"

sta::set_pi_model u1/Y 0.005 10.0 0.003
sta::set_elmore u1/Y u2/A 0.005
sta::set_pi_model u2/Y 0.008 15.0 0.005
sta::set_elmore u2/Y r3/D 0.008
sta::set_pi_model r1/Q 0.002 5.0 0.001
sta::set_elmore r1/Q u1/A 0.003
sta::set_pi_model r2/Q 0.003 6.0 0.002
sta::set_elmore r2/Q u2/B 0.004
sta::set_pi_model r3/Q 0.001 2.0 0.001
sta::set_elmore r3/Q out 0.002

# Run timing to trigger reduction and build caches
report_checks

# Run with arnoldi delay calc to exercise different reduction paths
set_delay_calculator arnoldi
report_checks

# Switch back
set_delay_calculator dmp_ceff_elmore

# Delete all parasitics by reading SPEF which overrides manual
# This exercises deleteReducedParasitics paths
read_spef ../../test/reg1_asap7.spef

report_checks

#---------------------------------------------------------------
# Test 2: Read SPEF, run timing, then re-read SPEF
# Exercises: deleteParasiticNetworks, re-read paths
#---------------------------------------------------------------
puts "--- Test 2: SPEF re-read ---"

# Run timing with different calculators to build caches
report_checks -fields {slew cap input_pins}

set_delay_calculator arnoldi
report_checks

set_delay_calculator dmp_ceff_two_pole
report_checks

# Now re-read the same SPEF - this triggers deleteParasiticNetworks + deleteReducedParasitics
set_delay_calculator dmp_ceff_elmore
read_spef ../../test/reg1_asap7.spef

report_checks

#---------------------------------------------------------------
# Test 3: Query parasitics state
# Exercises: findPiElmore, findElmore, capacitance
#---------------------------------------------------------------
puts "--- Test 3: query parasitic state ---"

set pi_u1 [sta::find_pi_elmore [get_pins u1/Y] "rise" "max"]
puts "u1/Y rise max pi: $pi_u1"

set pi_u1_fall [sta::find_pi_elmore [get_pins u1/Y] "fall" "max"]
puts "u1/Y fall max pi: $pi_u1_fall"

set pi_r1 [sta::find_pi_elmore [get_pins r1/Q] "rise" "max"]
puts "r1/Q rise max pi: $pi_r1"

set elm [sta::find_elmore [get_pins u1/Y] [get_pins u2/A] "rise" "max"]
puts "elmore u1/Y->u2/A: $elm"

set elm_min [sta::find_elmore [get_pins u1/Y] [get_pins u2/A] "rise" "min"]
puts "elmore u1/Y->u2/A min: $elm_min"

set elm_fall [sta::find_elmore [get_pins u1/Y] [get_pins u2/A] "fall" "max"]
puts "elmore u1/Y->u2/A fall: $elm_fall"

# Query for non-existent paths
set elm_none [sta::find_elmore [get_pins r3/Q] [get_pins u1/A] "rise" "max"]
puts "elmore r3/Q->u1/A (should be empty): $elm_none"

#---------------------------------------------------------------
# Test 4: Manual pi model THEN SPEF THEN manual again
# Exercises: deleteReducedParasitics on second manual set
#---------------------------------------------------------------
puts "--- Test 4: manual -> SPEF -> manual ---"

# Set manual again on top of SPEF
sta::set_pi_model u1/Y 0.01 20.0 0.008
sta::set_elmore u1/Y u2/A 0.01

report_checks

# Query back the manually set values
set pi_u1_new [sta::find_pi_elmore [get_pins u1/Y] "rise" "max"]
puts "u1/Y pi after re-set: $pi_u1_new"

set elm_u1_new [sta::find_elmore [get_pins u1/Y] [get_pins u2/A] "rise" "max"]
puts "elmore u1/Y->u2/A after re-set: $elm_u1_new"

#---------------------------------------------------------------
# Test 5: Report annotation in various states
# Exercises: reportParasiticAnnotation
#---------------------------------------------------------------
puts "--- Test 5: annotation reports ---"

report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Test 6: Report net with various digit counts
# Exercises: report_net paths with different formatting
#---------------------------------------------------------------
puts "--- Test 6: report_net ---"

foreach net_name {r1q r2q u1z u2z out in1 in2 clk1 clk2 clk3} {
  report_net -digits 6 $net_name
}

#---------------------------------------------------------------
# Test 7: Delay calc with dcalc reports
# Exercises: various parasitic query paths in dcalc
#---------------------------------------------------------------
puts "--- Test 7: dcalc reports ---"

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max -digits 6
report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max -digits 4
report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max
report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max

# Final read SPEF again to exercise re-read cleanup
read_spef ../../test/reg1_asap7.spef
report_checks
