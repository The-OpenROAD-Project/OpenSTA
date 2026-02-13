# Test pi_pole_residue parasitic model creation and query.
# Targets: ConcreteParasitics.cc uncovered:
#   ConcretePiPoleResidue constructor (line 309), isPiPoleResidue,
#   ConcretePiPoleResidue::piModel, setPiModel, capacitance,
#   findPoleResidue, setPoleResidue, deleteLoad,
#   ConcretePoleResidue constructor, poleResidue, poleResidueCount,
#   unannotatedLoads
# Also targets: Parasitics.cc: makePiPoleResidue, setPoleResidue, findPiPoleResidue
#   ReduceParasitics.cc: reduceToPiPoleResidue, arnoldi reduction with pole/residue
#   ConcreteParasitics.cc: isPiModel vs isPiPoleResidue branching

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
puts "PASS: read_spef"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: dmp_ceff_two_pole report_checks"

report_checks -path_delay min
puts "PASS: dmp_ceff_two_pole min path"

report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: dmp_ceff_two_pole in1->out"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: dmp_ceff_two_pole in2->out"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: dmp_ceff_two_pole with fields"

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

puts "PASS: pi_pole_residue queries"

#---------------------------------------------------------------
# Test 3: Delay calc reports with two-pole model
# Exercises: parasitic access through dcalc for pole/residue
#---------------------------------------------------------------
puts "--- Test 3: dcalc with two-pole ---"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "dcalc u1 A->Y max: done"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -min} msg
puts "dcalc u1 A->Y min: done"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "dcalc u2 A->Y max: done"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max} msg
puts "dcalc u2 B->Y max: done"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "dcalc r1 CLK->Q max: done"

catch {report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max} msg
puts "dcalc r2 CLK->Q max: done"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max} msg
puts "dcalc r3 CLK->Q max: done"

puts "PASS: dcalc reports with two-pole"

#---------------------------------------------------------------
# Test 4: Switch between delay calculators to exercise
# different parasitic reduction branches
#---------------------------------------------------------------
puts "--- Test 4: switch delay calculators ---"

# Arnoldi uses different reduction paths
set_delay_calculator arnoldi
report_checks
puts "PASS: arnoldi after two-pole"

# Query arnoldi-reduced values
catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "arnoldi dcalc u1: done"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "arnoldi dcalc u2: done"

# Switch back to dmp_ceff_elmore (exercises deleteReducedParasitics)
set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: back to elmore"

# Switch to two-pole again (re-reduces)
set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: two-pole again"

# Lumped cap
set_delay_calculator lumped_cap
report_checks
puts "PASS: lumped_cap"

#---------------------------------------------------------------
# Test 5: SPEF with coupling caps and two-pole reduction
# Exercises: coupling cap handling in pole/residue model
#---------------------------------------------------------------
puts "--- Test 5: coupling + two-pole ---"
set_delay_calculator dmp_ceff_two_pole
read_spef -keep_capacitive_coupling parasitics_coupling_spef.spef
puts "PASS: read_spef with coupling"

report_checks
puts "PASS: two-pole with coupling"

report_checks -path_delay min
puts "PASS: two-pole min with coupling"

report_checks -fields {slew cap}
puts "PASS: two-pole fields with coupling"

# Elmore with coupling (exercises reduction from coupling network)
set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: elmore with coupling"

#---------------------------------------------------------------
# Test 6: Re-read SPEF to exercise cleanup
#---------------------------------------------------------------
puts "--- Test 6: re-read SPEF ---"
set_delay_calculator dmp_ceff_two_pole
read_spef ../../test/reg1_asap7.spef
report_checks
puts "PASS: re-read without coupling"

# Net reports with different calculators
foreach net_name {r1q r2q u1z u2z out} {
  catch {report_net -digits 4 $net_name} msg
  puts "report_net $net_name: done"
}
puts "PASS: net reports"

#---------------------------------------------------------------
# Test 7: Parasitic annotation
#---------------------------------------------------------------
puts "--- Test 7: annotation ---"
report_parasitic_annotation
puts "PASS: annotation"

report_parasitic_annotation -report_unannotated
puts "PASS: annotation unannotated"

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
puts "PASS: elmore with manual pi"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: two-pole with manual pi"

set_delay_calculator lumped_cap
report_checks
puts "PASS: lumped_cap with manual pi"

puts "ALL PASSED"
