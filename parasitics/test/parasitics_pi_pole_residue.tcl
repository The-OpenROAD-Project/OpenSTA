# Test pi_pole_residue parasitic model creation and query.

source ../../test/helpers.tcl

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
# Test 1: Read SPEF to create parasitic networks, then reduce
# to pi_pole_residue via dmp_ceff_two_pole delay calculator
# Exercises: ReduceParasitics.cc reduceToPiPoleResidue path
#---------------------------------------------------------------
puts "--- Test 1: SPEF with dmp_ceff_two_pole reduction ---"
read_spef ../../test/reg1_asap7.spef

set_delay_calculator dmp_ceff_two_pole
report_checks

report_checks -path_delay min

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

report_checks -fields {slew cap input_pins nets fanout}

#---------------------------------------------------------------
# Test 2: Query pi_pole_residue model values
# Exercises: findPiPoleResidue, poleResidueCount, poleResidue
#---------------------------------------------------------------
puts "--- Test 2: query pi_pole_residue ---"

catch {
  set ppr [sta::find_pi_pole_residue [get_pins u1/Y] "rise" "max"]
  puts "u1/Y rise max pi_pole_residue: $ppr"
} msg
puts "find_pi_pole_residue u1/Y rise max: done ($msg)"

catch {
  set ppr [sta::find_pi_pole_residue [get_pins u1/Y] "fall" "max"]
  puts "u1/Y fall max pi_pole_residue: $ppr"
} msg
puts "find_pi_pole_residue u1/Y fall max: done ($msg)"

catch {
  set ppr [sta::find_pi_pole_residue [get_pins u2/Y] "rise" "max"]
  puts "u2/Y rise max pi_pole_residue: $ppr"
} msg
puts "find_pi_pole_residue u2/Y rise max: done ($msg)"

catch {
  set ppr [sta::find_pi_pole_residue [get_pins r1/Q] "rise" "max"]
  puts "r1/Q rise max pi_pole_residue: $ppr"
} msg
puts "find_pi_pole_residue r1/Q rise max: done ($msg)"

catch {
  set ppr [sta::find_pi_pole_residue [get_pins r2/Q] "fall" "min"]
  puts "r2/Q fall min pi_pole_residue: $ppr"
} msg
puts "find_pi_pole_residue r2/Q fall min: done ($msg)"

#---------------------------------------------------------------
# Test 3: Delay calc reports with two-pole model
# Exercises: parasitic access through dcalc for pole/residue
#---------------------------------------------------------------
puts "--- Test 3: dcalc with two-pole ---"

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "dcalc u1 A->Y max: done"

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -min
puts "dcalc u1 A->Y min: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "dcalc u2 A->Y max: done"

report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max
puts "dcalc u2 B->Y max: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "dcalc r1 CLK->Q max: done"

report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max
puts "dcalc r2 CLK->Q max: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max
puts "dcalc r3 CLK->Q max: done"

#---------------------------------------------------------------
# Test 4: Switch between delay calculators to exercise
# different parasitic reduction branches
#---------------------------------------------------------------
puts "--- Test 4: switch delay calculators ---"

# Arnoldi uses different reduction paths
set_delay_calculator arnoldi
report_checks

# Query arnoldi-reduced values
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "arnoldi dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "arnoldi dcalc u2: done"

# Switch back to dmp_ceff_elmore (exercises deleteReducedParasitics)
set_delay_calculator dmp_ceff_elmore
report_checks

# Switch to two-pole again (re-reduces)
set_delay_calculator dmp_ceff_two_pole
report_checks

# Lumped cap
set_delay_calculator lumped_cap
report_checks

#---------------------------------------------------------------
# Test 5: SPEF with coupling caps and two-pole reduction
# Exercises: coupling cap handling in pole/residue model
#---------------------------------------------------------------
puts "--- Test 5: coupling + two-pole ---"
set_delay_calculator dmp_ceff_two_pole
read_spef -keep_capacitive_coupling parasitics_coupling_spef.spef

report_checks

report_checks -path_delay min

report_checks -fields {slew cap}

# Elmore with coupling (exercises reduction from coupling network)
set_delay_calculator dmp_ceff_elmore
report_checks

#---------------------------------------------------------------
# Test 6: Re-read SPEF to exercise cleanup
#---------------------------------------------------------------
puts "--- Test 6: re-read SPEF ---"
set_delay_calculator dmp_ceff_two_pole
read_spef ../../test/reg1_asap7.spef
report_checks

# Net reports with different calculators
foreach net_name {r1q r2q u1z u2z out} {
  report_net -digits 4 $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Test 7: Parasitic annotation
#---------------------------------------------------------------
puts "--- Test 7: annotation ---"
report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Test 8: Manual pi model then query isPiElmore vs isPiPoleResidue
# Exercises: the type-checking branches in ConcreteParasitics
#---------------------------------------------------------------
puts "--- Test 8: manual pi + elmore then query ---"
catch {sta::set_pi_model u1/Y 0.005 10.0 0.003} msg
puts "set_pi_model u1/Y: $msg"

catch {sta::set_elmore u1/Y u2/B 0.005} msg
puts "set_elmore u1/Y->u2/B: $msg"

# Run timing with different calculators to force re-reduction
set_delay_calculator dmp_ceff_elmore
report_checks

set_delay_calculator dmp_ceff_two_pole
report_checks

set_delay_calculator lumped_cap
report_checks
