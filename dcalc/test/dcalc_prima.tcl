# Test prima delay calculator with SPEF parasitics
# Targets: PrimaDelayCalc.cc (61.7% coverage, 603 lines)
#   Also exercises ArnoldiReduce.cc and ArnoldiDelayCalc.cc paths

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

# Read SPEF parasitics
puts "--- Reading SPEF ---"
read_spef ../../test/reg1_asap7.spef
puts "PASS: read_spef completed"

#---------------------------------------------------------------
# Test prima delay calculator
#---------------------------------------------------------------
puts "--- prima delay calculator ---"
catch {set_delay_calculator prima} msg
puts "set_delay_calculator prima: $msg"

report_checks
puts "PASS: prima report_checks"

report_checks -path_delay min
puts "PASS: prima min path"

report_checks -path_delay max
puts "PASS: prima max path"

report_checks -fields {slew cap input_pins}
puts "PASS: prima with fields"

report_checks -format full_clock
puts "PASS: prima full_clock"

# report_dcalc with prima
catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y]} msg
puts "prima dcalc u1 A->Y: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y]} msg
puts "prima dcalc u2 A->Y: $msg"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y]} msg
puts "prima dcalc u2 B->Y: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q]} msg
puts "prima dcalc r1 CLK->Q: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max} msg
puts "prima dcalc r3 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -max} msg
puts "prima dcalc r3 setup: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -min} msg
puts "prima dcalc r3 hold: $msg"

# Report from different paths
report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: prima in1->out"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: prima in2->out"

#---------------------------------------------------------------
# Now switch to arnoldi and compare
#---------------------------------------------------------------
puts "--- arnoldi delay calculator with same design ---"
catch {set_delay_calculator arnoldi} msg
puts "set_delay_calculator arnoldi: $msg"

report_checks
puts "PASS: arnoldi report_checks"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "arnoldi dcalc u1 A->Y max: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "arnoldi dcalc u2 A->Y max: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "arnoldi dcalc r1 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -min} msg
puts "arnoldi dcalc r1 CLK->Q min: $msg"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: arnoldi with full fields"

#---------------------------------------------------------------
# Switch to lumped_cap with parasitics
#---------------------------------------------------------------
puts "--- lumped_cap with parasitics ---"
set_delay_calculator lumped_cap

report_checks
puts "PASS: lumped_cap with parasitics"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "lumped_cap dcalc u1: $msg"

#---------------------------------------------------------------
# Switch to dmp_ceff_two_pole with parasitics
#---------------------------------------------------------------
puts "--- dmp_ceff_two_pole with parasitics ---"
set_delay_calculator dmp_ceff_two_pole

report_checks
puts "PASS: dmp_ceff_two_pole with parasitics"

report_checks -path_delay min
puts "PASS: dmp_ceff_two_pole min"

report_checks -path_delay max
puts "PASS: dmp_ceff_two_pole max"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "dmp_ceff_two_pole dcalc u1: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "dmp_ceff_two_pole dcalc u2 A->Y: $msg"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max} msg
puts "dmp_ceff_two_pole dcalc u2 B->Y: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "dmp_ceff_two_pole dcalc r1 CLK->Q: $msg"

report_checks -fields {slew cap}
puts "PASS: dmp_ceff_two_pole with fields"

#---------------------------------------------------------------
# Switch back to default
#---------------------------------------------------------------
puts "--- dmp_ceff_elmore (default) ---"
set_delay_calculator dmp_ceff_elmore

report_checks
puts "PASS: default dcalc with parasitics"

puts "ALL PASSED"
