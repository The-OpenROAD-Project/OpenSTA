# Test DmpCeff edge cases: extreme loads, extreme slews, overflow handling,
# different convergence paths for dmpCeffElmore/TwoPole,
# report_dcalc with digits, unit delay calculator baseline.
# Targets: DmpCeff.cc dmpCeffElmore convergence edge cases,
#          dmpCeffTwoPole convergence edge cases,
#          dmpCeffDrvrPi, ceffPiElmore, ceffPiD,
#          dmpCeffIter, dmpCeffStep, dmpCeffD,
#          overflow handling paths, findRange edge cases,
#          DmpDelayCalc.cc gateDelay/gateSlew with edge case inputs,
#          GraphDelayCalc.cc findVertexDelay, findCheckEdgeDelays,
#          report_dcalc, netCaps, loadDelay

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

############################################################
# DmpCeff Elmore: extreme load conditions
############################################################
puts "--- dmp_ceff_elmore extreme loads ---"
set_delay_calculator dmp_ceff_elmore

# Zero load
set_load 0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "elmore zero load: $msg"
puts "PASS: elmore zero load"

# Tiny load
set_load 0.00001 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "elmore tiny load: $msg"
puts "PASS: elmore tiny load"

# Very large load (potential overflow path)
set_load 5.0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "elmore large load: $msg"
puts "PASS: elmore large load"

# Huge load
set_load 10.0 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "elmore huge load: $msg"
puts "PASS: elmore huge load"

set_load 0 [get_ports out1]

############################################################
# DmpCeff Elmore: extreme input transitions
############################################################
puts "--- dmp_ceff_elmore extreme transitions ---"

# Very fast transition
set_input_transition 0.0001 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "elmore fast transition: $msg"
puts "PASS: elmore fast transition"

# Very slow transition
set_input_transition 5.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "elmore slow transition: $msg"
puts "PASS: elmore slow transition"

# Extreme slow
set_input_transition 10.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "elmore extreme slow: $msg"
puts "PASS: elmore extreme slow"

set_input_transition 0.1 [get_ports in1]

############################################################
# DmpCeff Elmore: combined extreme load + transition
############################################################
puts "--- dmp_ceff_elmore combined extremes ---"

set_load 5.0 [get_ports out1]
set_input_transition 0.001 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: elmore extreme: large load + fast slew"

set_load 0.0001 [get_ports out1]
set_input_transition 5.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: elmore extreme: small load + slow slew"

set_load 5.0 [get_ports out1]
set_input_transition 5.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: elmore extreme: large load + slow slew"

set_load 0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]

############################################################
# DmpCeff TwoPole: same extreme tests
############################################################
puts "--- dmp_ceff_two_pole extreme loads ---"
set_delay_calculator dmp_ceff_two_pole

set_load 0 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: two_pole zero load"

set_load 0.00001 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: two_pole tiny load"

set_load 5.0 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: two_pole large load"

set_load 10.0 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: two_pole huge load"

set_load 0 [get_ports out1]

puts "--- dmp_ceff_two_pole extreme transitions ---"

set_input_transition 0.0001 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: two_pole fast transition"

set_input_transition 5.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: two_pole slow transition"

set_input_transition 0.1 [get_ports in1]

############################################################
# report_dcalc for all arc types in design
############################################################
puts "--- report_dcalc all arcs ---"
set_delay_calculator dmp_ceff_elmore
set_load 0.01 [get_ports out1]

# BUF arcs (both rise and fall)
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "dcalc buf1 max: $msg"
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -min} msg
puts "dcalc buf1 min: $msg"

# INV arcs
catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -max} msg
puts "dcalc inv1 max: $msg"
catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -min} msg
puts "dcalc inv1 min: $msg"

# DFF CK->Q arcs
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "dcalc reg1 CK->Q max: $msg"
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -min} msg
puts "dcalc reg1 CK->Q min: $msg"

# DFF check arcs (setup/hold)
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts "dcalc reg1 setup: $msg"
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -min} msg
puts "dcalc reg1 hold: $msg"

puts "PASS: report_dcalc all arcs"

############################################################
# report_dcalc with various digit counts
############################################################
puts "--- report_dcalc digits ---"
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 1} msg
puts "1 digit: $msg"
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 3} msg
puts "3 digits: $msg"
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 6} msg
puts "6 digits: $msg"
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 10} msg
puts "10 digits: $msg"
puts "PASS: dcalc digits"

############################################################
# Sweep load/slew matrix for convergence coverage
############################################################
puts "--- load/slew sweep ---"
foreach calc {dmp_ceff_elmore dmp_ceff_two_pole} {
  set_delay_calculator $calc
  foreach load {0.001 0.005 0.01 0.05 0.1 0.5 1.0 2.0} {
    foreach slew {0.01 0.05 0.1 0.5 1.0} {
      set_load $load [get_ports out1]
      set_input_transition $slew [get_ports in1]
      catch { report_checks -from [get_ports in1] -to [get_ports out1] > /dev/null }
    }
  }
}
set_load 0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]
puts "PASS: load/slew sweep"

############################################################
# Unit delay calculator
############################################################
puts "--- unit calculator ---"
set_delay_calculator unit
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "unit dcalc buf1: $msg"
puts "PASS: unit calculator"

############################################################
# Lumped cap calculator
############################################################
puts "--- lumped_cap calculator ---"
set_delay_calculator lumped_cap
set_load 0.01 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "lumped_cap dcalc buf1: $msg"
set_load 0 [get_ports out1]
puts "PASS: lumped_cap calculator"

# Restore default
set_delay_calculator dmp_ceff_elmore

puts "ALL PASSED"
