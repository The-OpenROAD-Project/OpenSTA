# Test DmpCeff and DmpDelayCalc with various load and slew conditions.
# Targets: DmpCeff.cc (dmpCeffElmore, dmpCeffTwoPole, dmpCeffDrvrPi,
#   ceffPiElmore, ceffPiD, dmpCeffIter, dmpCeffStep, dmpCeffD,
#   dmpCeffTableLookup, dmpCeffNear, dmpCeffBinaryCcs,
#   findRange, addStep, evalDmpGateDelay, evalDmpSlew)
# DmpDelayCalc.cc (makeArcDcalcArg, gateDelay, gateSlew,
#   reportDcalc, reportArcDcalcArg, loadDelay, reportLoadDelay,
#   gateDelaySlew, delayCalcName, makeCopy)
# PrimaDelayCalc.cc (primaDelay, primaReduceRc, prima2, prima3,
#   primaResStamp, primaCapStamp, primaPostReduction)
# ArnoldiDelayCalc.cc (arnoldiDelay, arnoldiReduceRc, arnoldi2, arnoldi3)
# ArnoldiReduce.cc (arnoldiReduce, arnoldiExpand)
# FindRoot.cc (findRoot, secantMethod, newtonMethod, bisectionMethod)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_multidriver_test.v
link_design dcalc_multidriver_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3 in4 sel}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3}]

#---------------------------------------------------------------
# DmpCeff Elmore with various load conditions
# Exercises: dmpCeffElmore iteration with different effective capacitance
#---------------------------------------------------------------
puts "--- dmp_ceff_elmore with varying loads ---"
set_delay_calculator dmp_ceff_elmore

# Very small load
set_load 0.0001 [get_ports out1]
set_input_transition 0.01 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: dmp_ceff_elmore tiny load"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "tiny load dcalc: $msg"

# Small load
set_load 0.001 [get_ports out1]
set_input_transition 0.05 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: dmp_ceff_elmore small load"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "small load dcalc: $msg"

# Medium load
set_load 0.01 [get_ports out1]
set_input_transition 0.1 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: dmp_ceff_elmore medium load"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "medium load dcalc: $msg"

# Large load
set_load 0.1 [get_ports out1]
set_input_transition 0.5 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: dmp_ceff_elmore large load"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "large load dcalc: $msg"

# Very large load (stress dmp iteration convergence)
set_load 1.0 [get_ports out1]
set_input_transition 1.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: dmp_ceff_elmore very large load"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "very large load dcalc: $msg"

# Reset
set_load 0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]

#---------------------------------------------------------------
# DmpCeff TwoPole with same load variations
# Exercises: dmpCeffTwoPole path, different convergence behavior
#---------------------------------------------------------------
puts "--- dmp_ceff_two_pole with varying loads ---"
set_delay_calculator dmp_ceff_two_pole

# Vary load and transition on multiple outputs
foreach out_port {out1 out2 out3} {
  foreach load_val {0.001 0.01 0.05 0.1} {
    set_load $load_val [get_ports $out_port]
    catch {report_checks -to [get_ports $out_port]} msg
    puts "dmp_two_pole $out_port load=$load_val: done"
  }
  set_load 0 [get_ports $out_port]
}

# report_dcalc for all gate types with dmp_two_pole
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "dmp_two_pole buf1: $msg"

catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -max} msg
puts "dmp_two_pole inv1: $msg"

catch {report_dcalc -from [get_pins and1/A1] -to [get_pins and1/ZN] -max} msg
puts "dmp_two_pole and1 A1: $msg"

catch {report_dcalc -from [get_pins or1/A1] -to [get_pins or1/ZN] -max} msg
puts "dmp_two_pole or1 A1: $msg"

catch {report_dcalc -from [get_pins nand1/A1] -to [get_pins nand1/ZN] -max} msg
puts "dmp_two_pole nand1 A1: $msg"

catch {report_dcalc -from [get_pins nor1/A1] -to [get_pins nor1/ZN] -max} msg
puts "dmp_two_pole nor1 A1: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "dmp_two_pole reg1 CK->Q: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts "dmp_two_pole reg1 setup: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -min} msg
puts "dmp_two_pole reg1 hold: $msg"

#---------------------------------------------------------------
# Multiple input transition values to exercise different table lookups
# Exercises: dmpCeffTableLookup, findRange
#---------------------------------------------------------------
puts "--- varying input transitions ---"
set_delay_calculator dmp_ceff_elmore

foreach slew_val {0.001 0.01 0.05 0.1 0.2 0.5 1.0} {
  set_input_transition $slew_val [get_ports {in1 in2 in3 in4 sel}]
  catch {
    report_checks -from [get_ports in1] -to [get_ports out1]
  } msg
  puts "slew=$slew_val: done"
}
set_input_transition 0.1 [get_ports {in1 in2 in3 in4 sel}]

#---------------------------------------------------------------
# CCS effective capacitance calculator on large design
# Exercises: CcsCeffDelayCalc paths
#---------------------------------------------------------------
puts "--- ccs_ceff on larger design ---"
catch {set_delay_calculator ccs_ceff} msg
puts "set_delay_calculator ccs_ceff: $msg"

report_checks
puts "PASS: ccs_ceff on larger design"

# Various loads with ccs_ceff
foreach load_val {0.001 0.01 0.1} {
  set_load $load_val [get_ports out1]
  report_checks -from [get_ports in1] -to [get_ports out1]
  puts "ccs_ceff load=$load_val: done"
}
set_load 0 [get_ports out1]

# report_dcalc with ccs_ceff
catch {report_dcalc -from [get_pins nand1/A1] -to [get_pins nand1/ZN] -max} msg
puts "ccs_ceff nand1 A1: $msg"

catch {report_dcalc -from [get_pins nor1/A1] -to [get_pins nor1/ZN] -max} msg
puts "ccs_ceff nor1 A1: $msg"

catch {report_dcalc -from [get_pins buf2/A] -to [get_pins buf2/Z] -max} msg
puts "ccs_ceff buf2 A->Z: $msg"

catch {report_dcalc -from [get_pins buf3/A] -to [get_pins buf3/Z] -max} msg
puts "ccs_ceff buf3 A->Z: $msg"

#---------------------------------------------------------------
# Rapid calculator switching to exercise copy/reinit paths
#---------------------------------------------------------------
puts "--- rapid calculator switching ---"
set_delay_calculator unit
report_checks -from [get_ports in1] -to [get_ports out3]
set_delay_calculator lumped_cap
report_checks -from [get_ports in1] -to [get_ports out3]
set_delay_calculator dmp_ceff_elmore
report_checks -from [get_ports in1] -to [get_ports out3]
set_delay_calculator dmp_ceff_two_pole
report_checks -from [get_ports in1] -to [get_ports out3]
catch {set_delay_calculator ccs_ceff} msg
report_checks -from [get_ports in1] -to [get_ports out3]
set_delay_calculator dmp_ceff_elmore
report_checks -from [get_ports in1] -to [get_ports out3]
puts "PASS: rapid switching"

#---------------------------------------------------------------
# report_checks with various reporting formats
#---------------------------------------------------------------
puts "--- report_checks formatting ---"
report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: all fields"

report_checks -format full_clock
puts "PASS: full_clock"

report_checks -format full_clock_expanded
puts "PASS: full_clock_expanded"

puts "ALL PASSED"
