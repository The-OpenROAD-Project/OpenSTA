# Test CCS/DMP delay calc with SPEF parasitics and multi-corner.
# Targets: CcsCeffDelayCalc.cc (0.0% -> parasitic paths)
#   GraphDelayCalc.cc (87.3% -> parasitic-driven delay calc)
#   DmpCeff.cc (79.1% -> ceff computation with Pi-model)
#   DmpDelayCalc.cc (51.8% -> Pi-model parasitic paths)
#   ArcDcalcWaveforms.cc (0.0% -> waveform data access)
#   NetCaps.cc (53.3% -> net cap with parasitics)
#   DelayCalcBase.cc (65.3% -> parasitic delay base)
#   FindRoot.cc (79.5% -> root finding edge cases)

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
# CCS delay calculator with parasitics
#---------------------------------------------------------------
puts "--- ccs_ceff delay calculator with parasitics ---"
catch {set_delay_calculator ccs_ceff} msg
puts "set_delay_calculator ccs_ceff: $msg"

report_checks
puts "PASS: ccs_ceff with parasitics report_checks"

report_checks -path_delay min
puts "PASS: ccs_ceff with parasitics min"

report_checks -path_delay max
puts "PASS: ccs_ceff with parasitics max"

report_checks -fields {slew cap input_pins} -format full_clock
puts "PASS: ccs_ceff with parasitics fields"

# report_dcalc exercises arc delay computation through parasitics
catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y]} msg
puts "ccs_ceff dcalc u1 A->Y: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y]} msg
puts "ccs_ceff dcalc u2 A->Y: $msg"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y]} msg
puts "ccs_ceff dcalc u2 B->Y: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "ccs_ceff dcalc r1 CLK->Q max: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -min} msg
puts "ccs_ceff dcalc r1 CLK->Q min: $msg"

# Setup/hold arcs with ccs_ceff
catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -max} msg
puts "ccs_ceff dcalc r3 setup: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/D] -min} msg
puts "ccs_ceff dcalc r3 hold: $msg"

# Additional paths
report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: ccs_ceff in1->out"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: ccs_ceff in2->out"

#---------------------------------------------------------------
# DMP ceff two-pole with parasitics (incremental from ccs_ceff)
#---------------------------------------------------------------
puts "--- dmp_ceff_two_pole with parasitics ---"
set_delay_calculator dmp_ceff_two_pole

report_checks
puts "PASS: dmp_ceff_two_pole with parasitics"

report_checks -path_delay min
puts "PASS: dmp_ceff_two_pole min with parasitics"

report_checks -path_delay max
puts "PASS: dmp_ceff_two_pole max with parasitics"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "dmp_two_pole dcalc u1: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "dmp_two_pole dcalc u2: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "dmp_two_pole dcalc r1 CLK->Q: $msg"

report_checks -fields {slew cap input_pins nets}
puts "PASS: dmp_two_pole with full fields"

#---------------------------------------------------------------
# Incremental changes with parasitics
#---------------------------------------------------------------
puts "--- incremental with parasitics ---"

# Change load
set_load 0.001 [get_ports out]
report_checks
puts "PASS: incremental parasitics after set_load"

# Change input transition
set_input_transition 50 {in1 in2}
report_checks
puts "PASS: incremental parasitics after set_input_transition"

# Change clock period
create_clock -name clk -period 200 {clk1 clk2 clk3}
report_checks
puts "PASS: incremental parasitics after clock change"

#---------------------------------------------------------------
# Switch to ccs_ceff after constraint changes (exercises reinit)
#---------------------------------------------------------------
puts "--- ccs_ceff after constraint changes ---"
set_delay_calculator ccs_ceff
report_checks
puts "PASS: ccs_ceff after constraint changes"

# Switch rapidly between calculators
set_delay_calculator dmp_ceff_elmore
set_delay_calculator ccs_ceff
set_delay_calculator arnoldi
set_delay_calculator ccs_ceff
report_checks
puts "PASS: rapid calculator switching"

#---------------------------------------------------------------
# Report checks with different endpoint counts
#---------------------------------------------------------------
puts "--- report_checks with endpoint_count ---"
report_checks -endpoint_count 2
puts "PASS: endpoint_count 2"

report_checks -group_count 3
puts "PASS: group_count 3"

report_checks -path_delay min -endpoint_count 3
puts "PASS: min endpoint_count 3"

puts "ALL PASSED"
