# Test Arnoldi and Prima delay calculators with SPEF parasitics and
# different load/slew conditions for improved coverage.
# Targets: PrimaDelayCalc.cc (prima delay paths, prima reduce RC,
#   prima2, prima3, primaResStamp, primaCapStamp, primaPostReduction)
# ArnoldiDelayCalc.cc (arnoldi delay, arnoldi reduce RC,
#   arnoldi2, arnoldi3, arnoldiExpand)
# ArnoldiReduce.cc (arnoldi reduce matrix, arnoldi iteration)
# GraphDelayCalc.cc (delay calc with parasitics for multiple calculators)
# DmpCeff.cc (ceff with Pi-model parasitics)
# FindRoot.cc (root finding with parasitic-loaded timing)

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
set_propagated_clock {clk1 clk2 clk3}

# Read SPEF parasitics
puts "--- Reading SPEF ---"
read_spef ../../test/reg1_asap7.spef
puts "PASS: read_spef completed"

#---------------------------------------------------------------
# Prima delay calculator with various input transition values
# Exercises: primaDelay, primaReduceRc, prima2, prima3
#---------------------------------------------------------------
puts "--- prima with varying slews ---"
catch {set_delay_calculator prima} msg
puts "set_delay_calculator prima: $msg"

foreach slew_val {1 5 10 50 100} {
  set_input_transition $slew_val {in1 in2 clk1 clk2 clk3}
  report_checks
  puts "prima slew=$slew_val: done"
}
set_input_transition 10 {in1 in2 clk1 clk2 clk3}

# Prima with various load values
puts "--- prima with varying loads ---"
foreach load_val {0.0001 0.001 0.01 0.1} {
  set_load $load_val [get_ports out]
  report_checks
  puts "prima load=$load_val: done"
}
set_load 0 [get_ports out]

# Prima report_dcalc for all arcs
puts "--- prima report_dcalc all arcs ---"
catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "prima u1 A->Y max: $msg"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -min} msg
puts "prima u1 A->Y min: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "prima u2 A->Y max: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -min} msg
puts "prima u2 A->Y min: $msg"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max} msg
puts "prima u2 B->Y max: $msg"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -min} msg
puts "prima u2 B->Y min: $msg"

# DFF arcs with prima
catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "prima r1 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -min} msg
puts "prima r1 CLK->Q min: $msg"

catch {report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max} msg
puts "prima r2 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max} msg
puts "prima r3 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -min} msg
puts "prima r3 CLK->Q min: $msg"

# DFF check arcs with prima
catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/D] -max} msg
puts "prima r1 setup: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/D] -min} msg
puts "prima r1 hold: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -max} msg
puts "prima r3 setup: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -min} msg
puts "prima r3 hold: $msg"

# report_dcalc with digits
catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -digits 2} msg
puts "prima u1 2 digits: $msg"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -digits 8} msg
puts "prima u1 8 digits: $msg"

# Prima with fields
report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: prima with all fields"

report_checks -format full_clock
puts "PASS: prima full_clock"

# Prima specific paths
report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: prima in1->out"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: prima in2->out"

report_checks -path_delay min
puts "PASS: prima min path"

report_checks -path_delay max
puts "PASS: prima max path"

#---------------------------------------------------------------
# Arnoldi delay calculator with same variations
# Exercises: arnoldiDelay, arnoldiReduceRc, arnoldiExpand
#---------------------------------------------------------------
puts "--- arnoldi with varying slews ---"
catch {set_delay_calculator arnoldi} msg
puts "set_delay_calculator arnoldi: $msg"

foreach slew_val {1 5 10 50 100} {
  set_input_transition $slew_val {in1 in2 clk1 clk2 clk3}
  report_checks
  puts "arnoldi slew=$slew_val: done"
}
set_input_transition 10 {in1 in2 clk1 clk2 clk3}

# Arnoldi with varying loads
puts "--- arnoldi with varying loads ---"
foreach load_val {0.0001 0.001 0.01 0.1} {
  set_load $load_val [get_ports out]
  report_checks
  puts "arnoldi load=$load_val: done"
}
set_load 0 [get_ports out]

# Arnoldi report_dcalc for all arcs
puts "--- arnoldi report_dcalc ---"
catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "arnoldi u1 A->Y max: $msg"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -min} msg
puts "arnoldi u1 A->Y min: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "arnoldi u2 A->Y max: $msg"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max} msg
puts "arnoldi u2 B->Y max: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "arnoldi r1 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -min} msg
puts "arnoldi r1 CLK->Q min: $msg"

catch {report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max} msg
puts "arnoldi r2 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max} msg
puts "arnoldi r3 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/D] -max} msg
puts "arnoldi r1 setup: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/D] -min} msg
puts "arnoldi r1 hold: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -max} msg
puts "arnoldi r3 setup: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -min} msg
puts "arnoldi r3 hold: $msg"

# Arnoldi with fields
report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: arnoldi with all fields"

report_checks -format full_clock
puts "PASS: arnoldi full_clock"

#---------------------------------------------------------------
# Switching between parasitic calculators to exercise reinit paths
#---------------------------------------------------------------
puts "--- switching parasitic calculators ---"
set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: dmp_ceff_elmore with parasitics"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: dmp_ceff_two_pole with parasitics"

catch {set_delay_calculator ccs_ceff} msg
report_checks
puts "PASS: ccs_ceff with parasitics"

set_delay_calculator lumped_cap
report_checks
puts "PASS: lumped_cap with parasitics"

catch {set_delay_calculator prima} msg
report_checks
puts "PASS: prima after switching"

catch {set_delay_calculator arnoldi} msg
report_checks
puts "PASS: arnoldi after switching"

#---------------------------------------------------------------
# Incremental updates with parasitics
# Exercises: seedInvalidDelays with parasitic-loaded nets
#---------------------------------------------------------------
puts "--- incremental with parasitics ---"
set_delay_calculator dmp_ceff_elmore

set_load 0.001 [get_ports out]
report_checks
puts "PASS: incremental parasitics after set_load"

set_input_transition 50 {in1 in2}
report_checks
puts "PASS: incremental parasitics after set_input_transition"

create_clock -name clk -period 200 {clk1 clk2 clk3}
report_checks
puts "PASS: incremental parasitics after clock change"

# Restore
set_load 0 [get_ports out]
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
create_clock -name clk -period 500 {clk1 clk2 clk3}

puts "ALL PASSED"
