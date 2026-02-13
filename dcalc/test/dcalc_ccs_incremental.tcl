# Test CCS effective capacitance delay calculator and incremental updates.
# Targets: CcsCeffDelayCalc.cc (0.0% -> all paths)
#   GraphDelayCalc.cc (87.3% -> incremental update, arrival annotation)
#   DmpDelayCalc.cc (51.8% -> ccs_ceff falls back to dmp when no CCS data)
#   DelayCalcBase.cc (65.3% -> additional base class paths)
#   ArcDelayCalc.cc (59.7% -> makeArcDcalcArg error paths)
#   DmpCeff.cc (79.1% -> more DmpCeff computation paths)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]

#---------------------------------------------------------------
# CCS effective capacitance delay calculator
# (Falls back to table-based for NLDM libraries but exercises
#  constructor, copy, name, and fallback paths)
#---------------------------------------------------------------
puts "--- ccs_ceff delay calculator ---"
catch {set_delay_calculator ccs_ceff} msg
puts "set_delay_calculator ccs_ceff: $msg"

report_checks
puts "PASS: ccs_ceff report_checks"

report_checks -path_delay min
puts "PASS: ccs_ceff min path"

report_checks -path_delay max
puts "PASS: ccs_ceff max path"

# report_dcalc with ccs_ceff
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts "ccs_ceff dcalc buf1 max: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -min} msg
puts "ccs_ceff dcalc buf1 min: $msg"

catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -max} msg
puts "ccs_ceff dcalc inv1 max: $msg"

catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -min} msg
puts "ccs_ceff dcalc inv1 min: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max} msg
puts "ccs_ceff dcalc reg1 CK->Q max: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -min} msg
puts "ccs_ceff dcalc reg1 CK->Q min: $msg"

# Setup/hold check arcs
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts "ccs_ceff dcalc reg1 setup: $msg"

catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -min} msg
puts "ccs_ceff dcalc reg1 hold: $msg"

# With fields
report_checks -fields {slew cap input_pins}
puts "PASS: ccs_ceff with fields"

report_checks -format full_clock
puts "PASS: ccs_ceff full_clock format"

#---------------------------------------------------------------
# Incremental delay update: change constraints and recompute
#---------------------------------------------------------------
puts "--- incremental delay update ---"

# Change load and recompute (exercises GraphDelayCalc incremental update)
set_load 0.01 [get_ports out1]
report_checks
puts "PASS: incremental after set_load 0.01"

set_load 0.05 [get_ports out1]
report_checks
puts "PASS: incremental after set_load 0.05"

set_load 0.1 [get_ports out1]
report_checks
puts "PASS: incremental after set_load 0.1"

# Change input transition and recompute
set_input_transition 0.01 [get_ports in1]
report_checks
puts "PASS: incremental after input_transition 0.01"

set_input_transition 0.5 [get_ports in1]
report_checks
puts "PASS: incremental after input_transition 0.5"

# Change clock period (triggers incremental update)
create_clock -name clk -period 5 [get_ports clk]
report_checks
puts "PASS: incremental after clock period change"

# Change delays
set_input_delay -clock clk 1.0 [get_ports in1]
report_checks
puts "PASS: incremental after input_delay change"

set_output_delay -clock clk 2.0 [get_ports out1]
report_checks
puts "PASS: incremental after output_delay change"

#---------------------------------------------------------------
# Switch between calculators to exercise copy/init paths
#---------------------------------------------------------------
puts "--- calculator switching ---"

set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: switch to dmp_ceff_elmore"

set_delay_calculator ccs_ceff
report_checks
puts "PASS: switch back to ccs_ceff"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: switch to dmp_ceff_two_pole"

set_delay_calculator lumped_cap
report_checks
puts "PASS: switch to lumped_cap"

set_delay_calculator unit
report_checks
puts "PASS: switch to unit"

set_delay_calculator ccs_ceff
report_checks
puts "PASS: switch back to ccs_ceff final"

#---------------------------------------------------------------
# report_dcalc with -digits (exercises formatting paths)
#---------------------------------------------------------------
puts "--- report_dcalc with various digits ---"
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 2} msg
puts "dcalc 2 digits: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 4} msg
puts "dcalc 4 digits: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 8} msg
puts "dcalc 8 digits: $msg"

catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -digits 12} msg
puts "dcalc 12 digits: $msg"

puts "ALL PASSED"
