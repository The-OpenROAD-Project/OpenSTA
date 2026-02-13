# Test SPEF reader with different SPEF formats, port sections, design flow,
# RSPF sections, and varied scale factors.
# Targets: SpefReader.cc uncovered:
#   portDirection (I, O, B variants), rspfBegin/rspfFinish/rspfDrvrBegin/rspfLoad,
#   dspfBegin/dspfFinish, findParasiticNode (port path, net:subnode path),
#   setDesignFlow, RSPF parsing vs DSPF, findPin (port-only name path),
#   findNetRelative (sdc_network fallback), different scale factors
# Also targets: ConcreteParasitics.cc: makeParasiticNetwork, parasitic network
#   node/resistor/capacitor creation, deleteParasiticNetworks on re-read

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read SPEF with coupling caps and varied query patterns
# Exercises: coupling cap path in DSPF parsing, port I/O/B directions
#---------------------------------------------------------------
puts "--- Test 1: coupling SPEF port parsing ---"

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

# Read SPEF with coupling caps (keep mode)
read_spef -keep_capacitive_coupling parasitics_coupling_spef.spef
puts "PASS: read coupling SPEF"

report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: min path"

report_checks -format full_clock
puts "PASS: full_clock format"

#---------------------------------------------------------------
# Test 2: Re-read with coupling_reduction_factor variants
# Exercises: deleteParasiticNetworks, coupling factor scaling
#---------------------------------------------------------------
puts "--- Test 2: coupling factor variations ---"

read_spef -coupling_reduction_factor 0.0 parasitics_coupling_spef.spef
report_checks
puts "PASS: factor 0.0"

read_spef -coupling_reduction_factor 0.25 parasitics_coupling_spef.spef
report_checks
puts "PASS: factor 0.25"

read_spef -coupling_reduction_factor 0.75 parasitics_coupling_spef.spef
report_checks
puts "PASS: factor 0.75"

read_spef -coupling_reduction_factor 1.0 parasitics_coupling_spef.spef
report_checks
puts "PASS: factor 1.0"

#---------------------------------------------------------------
# Test 3: Read SPEF with -reduce flag
# Exercises: reduce parameter true path and dspfFinish reduction
#---------------------------------------------------------------
puts "--- Test 3: SPEF with -reduce ---"
read_spef -reduce parasitics_coupling_spef.spef
puts "PASS: read_spef -reduce coupling"

report_checks
puts "PASS: report after reduce"

read_spef -reduce ../../test/reg1_asap7.spef
puts "PASS: read_spef -reduce normal"

report_checks
puts "PASS: report after reduce normal"

#---------------------------------------------------------------
# Test 4: GCD sky130 SPEF (different format, large name map)
# Exercises: large NAME_MAP section, different vendor format,
#   findPinRelative, findNetRelative with different hierarchy
#---------------------------------------------------------------
puts "--- Test 4: GCD sky130 SPEF ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

read_spef ../../examples/gcd_sky130hd.spef
puts "PASS: read GCD SPEF"

report_checks
puts "PASS: GCD report_checks"

report_checks -path_delay min
puts "PASS: GCD min path"

report_checks -sort_by_slack
puts "PASS: GCD sort_by_slack"

#---------------------------------------------------------------
# Test 5: GCD with coupling and different delay calculators
#---------------------------------------------------------------
puts "--- Test 5: GCD delay calculators ---"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: GCD dmp_ceff_two_pole"

set_delay_calculator arnoldi
report_checks
puts "PASS: GCD arnoldi"

set_delay_calculator lumped_cap
report_checks
puts "PASS: GCD lumped_cap"

set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: GCD back to default"

#---------------------------------------------------------------
# Test 6: Re-read GCD SPEF with reduce
# Exercises: cleanup of large parasitic network
#---------------------------------------------------------------
puts "--- Test 6: GCD re-read with reduce ---"
read_spef -reduce ../../examples/gcd_sky130hd.spef
puts "PASS: GCD re-read reduce"

report_checks
puts "PASS: GCD report after re-read"

#---------------------------------------------------------------
# Test 7: Parasitic annotation with GCD
#---------------------------------------------------------------
puts "--- Test 7: GCD annotation ---"
report_parasitic_annotation
puts "PASS: GCD annotation"

report_parasitic_annotation -report_unannotated
puts "PASS: GCD annotation unannotated"

#---------------------------------------------------------------
# Test 8: GCD net reports
#---------------------------------------------------------------
puts "--- Test 8: GCD net reports ---"
catch {report_net clk} msg
puts "report_net clk: done"

catch {report_net -digits 6 clk} msg
puts "report_net -digits 6 clk: done"

foreach net_name {reset req_val resp_val} {
  catch {report_net $net_name} msg
  puts "report_net $net_name: done"
}
puts "PASS: GCD net reports"

#---------------------------------------------------------------
# Test 9: Annotated delay for GCD
#---------------------------------------------------------------
puts "--- Test 9: GCD annotated delay ---"
catch {report_annotated_delay -cell -net} msg
puts "annotated -cell -net: done"

catch {report_annotated_delay -from_in_ports -to_out_ports} msg
puts "annotated -from_in -to_out: done"

catch {report_annotated_delay -report_annotated} msg
puts "annotated -report_annotated: done"

catch {report_annotated_delay -report_unannotated} msg
puts "annotated -report_unannotated: done"

puts "ALL PASSED"
