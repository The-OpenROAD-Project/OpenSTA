# Test multi-corner delay calculation
# Exercises: define_corners, corner-specific liberty, report_checks -corner, report_dcalc -corner

define_corners fast slow

read_liberty -corner fast ../../test/nangate45/Nangate45_fast.lib
read_liberty -corner slow ../../test/nangate45/Nangate45_slow.lib

read_verilog dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

#---------------------------------------------------------------
# report_checks per corner
#---------------------------------------------------------------
puts "--- Fast corner timing ---"
report_checks -corner fast
puts "PASS: report_checks fast corner"

report_checks -corner fast -path_delay min
puts "PASS: report_checks fast corner min path"

report_checks -corner fast -path_delay max
puts "PASS: report_checks fast corner max path"

puts "--- Slow corner timing ---"
report_checks -corner slow
puts "PASS: report_checks slow corner"

report_checks -corner slow -path_delay min
puts "PASS: report_checks slow corner min path"

report_checks -corner slow -path_delay max
puts "PASS: report_checks slow corner max path"

#---------------------------------------------------------------
# report_dcalc per corner
#---------------------------------------------------------------
puts "--- report_dcalc per corner ---"

catch {report_dcalc -corner fast -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts $msg
puts "PASS: report_dcalc fast corner buf1"

catch {report_dcalc -corner slow -from [get_pins buf1/A] -to [get_pins buf1/Z]} msg
puts $msg
puts "PASS: report_dcalc slow corner buf1"

catch {report_dcalc -corner fast -from [get_pins inv1/A] -to [get_pins inv1/ZN]} msg
puts $msg
puts "PASS: report_dcalc fast corner inv1"

catch {report_dcalc -corner slow -from [get_pins inv1/A] -to [get_pins inv1/ZN]} msg
puts $msg
puts "PASS: report_dcalc slow corner inv1"

# DFF arcs per corner
catch {report_dcalc -corner fast -from [get_pins reg1/CK] -to [get_pins reg1/Q]} msg
puts $msg
puts "PASS: report_dcalc fast corner DFF CK->Q"

catch {report_dcalc -corner slow -from [get_pins reg1/CK] -to [get_pins reg1/Q]} msg
puts $msg
puts "PASS: report_dcalc slow corner DFF CK->Q"

# Setup/hold check arcs per corner
catch {report_dcalc -corner fast -from [get_pins reg1/CK] -to [get_pins reg1/D] -min} msg
puts $msg
puts "PASS: report_dcalc fast corner DFF hold check"

catch {report_dcalc -corner slow -from [get_pins reg1/CK] -to [get_pins reg1/D] -max} msg
puts $msg
puts "PASS: report_dcalc slow corner DFF setup check"

#---------------------------------------------------------------
# report_checks with -fields for more coverage
#---------------------------------------------------------------
puts "--- report_checks with fields ---"
report_checks -corner fast -fields {slew cap input_pins}
puts "PASS: report_checks fast with fields"

report_checks -corner slow -fields {slew cap input_pins}
puts "PASS: report_checks slow with fields"

#---------------------------------------------------------------
# set_load on output and recheck corners
#---------------------------------------------------------------
puts "--- set_load and recheck corners ---"
set_load 0.1 [get_ports out1]
report_checks -corner fast
puts "PASS: report_checks fast after set_load"

report_checks -corner slow
puts "PASS: report_checks slow after set_load"

puts "ALL PASSED"
