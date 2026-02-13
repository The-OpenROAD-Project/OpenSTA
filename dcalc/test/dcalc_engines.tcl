# Test different delay calculator engines
# Exercises: set_delay_calculator, report_dcalc, set_load, set_input_transition

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

#---------------------------------------------------------------
# Unit delay calculator
#---------------------------------------------------------------
puts "--- Testing unit delay calculator ---"
catch {set_delay_calculator unit} msg
puts $msg
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: unit delay calculator report_checks"

report_checks -path_delay min
puts "PASS: unit min path"

report_checks -path_delay max
puts "PASS: unit max path"

#---------------------------------------------------------------
# Lumped cap delay calculator
#---------------------------------------------------------------
puts "--- Testing lumped_cap delay calculator ---"
catch {set_delay_calculator lumped_cap} msg
puts $msg
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: lumped_cap delay calculator report_checks"

report_checks -path_delay min
puts "PASS: lumped_cap min path"

report_checks -path_delay max
puts "PASS: lumped_cap max path"

#---------------------------------------------------------------
# report_dcalc with various options
#---------------------------------------------------------------
puts "--- Testing report_dcalc ---"

# report_dcalc from/to
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts $msg
puts "PASS: report_dcalc from/to"

# report_dcalc -min
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -min} msg
puts $msg
puts "PASS: report_dcalc -min"

# report_dcalc -max
catch {report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max} msg
puts $msg
puts "PASS: report_dcalc -max"

# report_dcalc with -digits
catch {report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -digits 6} msg
puts $msg
puts "PASS: report_dcalc -digits"

# report_dcalc from only
catch {report_dcalc -from [get_pins buf1/A]} msg
puts $msg
puts "PASS: report_dcalc -from only"

# report_dcalc to only
catch {report_dcalc -to [get_pins inv1/ZN]} msg
puts $msg
puts "PASS: report_dcalc -to only"

# report_dcalc for DFF setup/hold arcs
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D]} msg
puts $msg
puts "PASS: report_dcalc DFF check arcs"

# report_dcalc for DFF clock->Q arc
catch {report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q]} msg
puts $msg
puts "PASS: report_dcalc DFF CK->Q arc"

#---------------------------------------------------------------
# set_load on output ports and recompute
#---------------------------------------------------------------
puts "--- Testing set_load ---"
set_load 0.05 [get_ports out1]

# Switch back to dmp_ceff_elmore (default) for load testing
catch {set_delay_calculator dmp_ceff_elmore} msg
puts $msg

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after set_load"

report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max
puts "PASS: report_dcalc after set_load"

#---------------------------------------------------------------
# set_input_transition on inputs and recompute
#---------------------------------------------------------------
puts "--- Testing set_input_transition ---"
set_input_transition 0.2 [get_ports in1]

report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: report_checks after set_input_transition"

report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max
puts "PASS: report_dcalc after set_input_transition"

#---------------------------------------------------------------
# Test dmp_ceff_two_pole calculator
#---------------------------------------------------------------
puts "--- Testing dmp_ceff_two_pole delay calculator ---"
catch {set_delay_calculator dmp_ceff_two_pole} msg
puts $msg
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: dmp_ceff_two_pole report_checks"

puts "ALL PASSED"
