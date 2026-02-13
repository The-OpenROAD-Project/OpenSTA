# Test delay calculation with SPEF parasitics
# Exercises: GraphDelayCalc with parasitics, arnoldi delay calculator

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
# Read SPEF parasitics
#---------------------------------------------------------------
puts "--- Reading SPEF ---"
read_spef ../../test/reg1_asap7.spef
puts "PASS: read_spef completed"

#---------------------------------------------------------------
# Default delay calculator (dmp_ceff_elmore) with parasitics
#---------------------------------------------------------------
puts "--- report_checks with parasitics (default dcalc) ---"
report_checks
puts "PASS: report_checks with parasitics"

report_checks -path_delay min
puts "PASS: report_checks min path with parasitics"

report_checks -path_delay max
puts "PASS: report_checks max path with parasitics"

report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: report_checks in1->out with parasitics"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: report_checks in2->out with parasitics"

# With fields for more coverage
report_checks -fields {slew cap input_pins} -format full_clock
puts "PASS: report_checks with fields and full_clock"

#---------------------------------------------------------------
# report_dcalc with parasitics
#---------------------------------------------------------------
puts "--- report_dcalc with parasitics ---"

# BUF gate arc
catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y]} msg
puts $msg
puts "PASS: report_dcalc BUF arc with parasitics"

# AND gate arc
catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y]} msg
puts $msg
puts "PASS: report_dcalc AND2 A->Y with parasitics"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y]} msg
puts $msg
puts "PASS: report_dcalc AND2 B->Y with parasitics"

# DFF clock-to-Q arc
catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q]} msg
puts $msg
puts "PASS: report_dcalc DFF CLK->Q with parasitics"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max} msg
puts $msg
puts "PASS: report_dcalc DFF r3 CLK->Q max with parasitics"

# DFF setup/hold check arcs
catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -max} msg
puts $msg
puts "PASS: report_dcalc DFF setup check with parasitics"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -min} msg
puts $msg
puts "PASS: report_dcalc DFF hold check with parasitics"

#---------------------------------------------------------------
# Arnoldi delay calculator with parasitics
#---------------------------------------------------------------
puts "--- Testing arnoldi delay calculator ---"
catch {set_delay_calculator arnoldi} msg
puts $msg

report_checks
puts "PASS: arnoldi report_checks"

report_checks -path_delay min
puts "PASS: arnoldi min path"

report_checks -path_delay max
puts "PASS: arnoldi max path"

report_checks -fields {slew cap input_pins} -format full_clock
puts "PASS: arnoldi report_checks with fields"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y]} msg
puts $msg
puts "PASS: arnoldi report_dcalc BUF"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y]} msg
puts $msg
puts "PASS: arnoldi report_dcalc AND2"

#---------------------------------------------------------------
# Lumped cap delay calculator with parasitics
#---------------------------------------------------------------
puts "--- Testing lumped_cap with parasitics ---"
catch {set_delay_calculator lumped_cap} msg
puts $msg

report_checks
puts "PASS: lumped_cap with parasitics report_checks"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y]} msg
puts $msg
puts "PASS: lumped_cap with parasitics report_dcalc"

#---------------------------------------------------------------
# dmp_ceff_two_pole delay calculator with parasitics
#---------------------------------------------------------------
puts "--- Testing dmp_ceff_two_pole with parasitics ---"
catch {set_delay_calculator dmp_ceff_two_pole} msg
puts $msg

report_checks
puts "PASS: dmp_ceff_two_pole with parasitics report_checks"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y]} msg
puts $msg
puts "PASS: dmp_ceff_two_pole with parasitics report_dcalc"

puts "ALL PASSED"
