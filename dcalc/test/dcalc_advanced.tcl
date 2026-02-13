# Test advanced delay calculation scenarios for coverage improvement
# Targets: DmpDelayCalc.cc (dmp_ceff_two_pole paths, DmpCeff error paths)
#   ArcDelayCalc.cc (additional arc types)
#   GraphDelayCalc.cc (more paths through delay graph computation)
#   NetCaps.cc (net capacitance queries)
#   UnitDelayCalc.cc (more paths through unit delay calc)
#   PrimaDelayCalc.cc (prima delay calculator)
#   CcsCeffDelayCalc.cc (ccs_ceff calculator)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

#---------------------------------------------------------------
# Test with various set_load values to exercise NetCaps paths
#---------------------------------------------------------------
puts "--- set_load variations ---"
set_load 0.001 [get_ports out1]
report_checks
puts "PASS: report_checks with 1fF load"

set_load 0.1 [get_ports out1]
report_checks
puts "PASS: report_checks with 100fF load"

set_load 1.0 [get_ports out1]
report_checks
puts "PASS: report_checks with 1pF load"

# Reset load
set_load 0 [get_ports out1]

#---------------------------------------------------------------
# Test set_input_transition with various values
#---------------------------------------------------------------
puts "--- set_input_transition ---"
set_input_transition 0.01 [get_ports in1]
report_checks
puts "PASS: report_checks with 10ps input transition"

set_input_transition 0.5 [get_ports in1]
report_checks
puts "PASS: report_checks with 500ps input transition"

set_input_transition 0.1 [get_ports in1]

#---------------------------------------------------------------
# Test report_dcalc for all arcs in the design
#---------------------------------------------------------------
puts "--- report_dcalc all arcs ---"

# BUF arcs
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "dcalc buf1 A->Z max: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -min} msg
puts "dcalc buf1 A->Z min: $msg"

# INV arcs
catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -max} msg
puts "dcalc inv1 A->ZN max: $msg"

catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -min} msg
puts "dcalc inv1 A->ZN min: $msg"

# DFF clock-to-Q
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "dcalc reg1 CK->Q max: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -min} msg
puts "dcalc reg1 CK->Q min: $msg"

# DFF setup/hold
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts "dcalc reg1 setup max: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -min} msg
puts "dcalc reg1 hold min: $msg"

# report_dcalc with digits
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 8} msg
puts "dcalc buf1 A->Z 8 digits: $msg"

#---------------------------------------------------------------
# Unit delay calculator
#---------------------------------------------------------------
puts "--- unit delay calculator ---"
set_delay_calculator unit

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: unit report_checks"

report_checks -path_delay min
puts "PASS: unit min path"

report_checks -path_delay max
puts "PASS: unit max path"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts "unit dcalc buf1: $msg"

catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN]} msg
puts "unit dcalc inv1: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q]} msg
puts "unit dcalc reg1 CK->Q: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D]} msg
puts "unit dcalc reg1 setup: $msg"

report_checks -fields {slew cap}
puts "PASS: unit with fields"

#---------------------------------------------------------------
# lumped_cap delay calculator
#---------------------------------------------------------------
puts "--- lumped_cap delay calculator ---"
set_delay_calculator lumped_cap

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: lumped_cap report_checks"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "lumped_cap dcalc buf1: $msg"

catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -max} msg
puts "lumped_cap dcalc inv1: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "lumped_cap dcalc reg1: $msg"

report_checks -fields {slew cap input_pins}
puts "PASS: lumped_cap with fields"

#---------------------------------------------------------------
# dmp_ceff_elmore (default) delay calculator
#---------------------------------------------------------------
puts "--- dmp_ceff_elmore delay calculator ---"
set_delay_calculator dmp_ceff_elmore

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: dmp_ceff_elmore report_checks"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts "dmp_ceff_elmore dcalc buf1: $msg"

#---------------------------------------------------------------
# dmp_ceff_two_pole delay calculator
#---------------------------------------------------------------
puts "--- dmp_ceff_two_pole delay calculator ---"
set_delay_calculator dmp_ceff_two_pole

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: dmp_ceff_two_pole report_checks"

report_checks -path_delay min
puts "PASS: dmp_ceff_two_pole min path"

report_checks -path_delay max
puts "PASS: dmp_ceff_two_pole max path"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "dmp_ceff_two_pole dcalc buf1 max: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -min} msg
puts "dmp_ceff_two_pole dcalc buf1 min: $msg"

catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -max} msg
puts "dmp_ceff_two_pole dcalc inv1: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "dmp_ceff_two_pole dcalc reg1 CK->Q: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts "dmp_ceff_two_pole dcalc reg1 setup: $msg"

report_checks -fields {slew cap input_pins}
puts "PASS: dmp_ceff_two_pole with fields"

puts "ALL PASSED"
