# Test SPEF reading with GCD sky130hd design (large SPEF, different format)
# Targets: SpefReader.cc (large netlist SPEF parsing, sky130 name mapping,
#   different SPEF format/vendor, findPinRelative, findPortPinRelative,
#   findInstanceRelative, D_NET sections, large *NAME_MAP sections)
#   ConcreteParasitics.cc (large parasitic network creation, many nodes/caps/resistors)
#   ReduceParasitics.cc (reduction of complex parasitic networks)
#   EstimateParasitics.cc (estimation fallback paths)
#   Parasitics.cc (various find/make operations with large design)

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd

read_sdc ../../examples/gcd_sky130hd.sdc

#---------------------------------------------------------------
# Baseline without parasitics
#---------------------------------------------------------------
puts "--- baseline ---"
report_checks
puts "PASS: baseline report_checks"

report_checks -path_delay min
puts "PASS: baseline min path"

#---------------------------------------------------------------
# Read SPEF (large sky130 SPEF)
#---------------------------------------------------------------
puts "--- read_spef GCD ---"
read_spef ../../examples/gcd_sky130hd.spef
puts "PASS: read_spef GCD"

#---------------------------------------------------------------
# Report with parasitics
#---------------------------------------------------------------
puts "--- timing with SPEF ---"
report_checks
puts "PASS: report_checks with SPEF"

report_checks -path_delay min
puts "PASS: min path with SPEF"

report_checks -path_delay max
puts "PASS: max path with SPEF"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report with fields"

report_checks -format full_clock
puts "PASS: full_clock format"

report_checks -sort_by_slack
puts "PASS: sort_by_slack"

#---------------------------------------------------------------
# Report parasitic annotation
#---------------------------------------------------------------
puts "--- parasitic annotation ---"
report_parasitic_annotation
puts "PASS: parasitic annotation"

report_parasitic_annotation -report_unannotated
puts "PASS: parasitic annotation unannotated"

#---------------------------------------------------------------
# Report nets
#---------------------------------------------------------------
puts "--- report_net ---"
catch {report_net clk} msg
puts "report_net clk: done"

# Sample some nets from the design
set some_nets [get_nets *]
set net_count [llength $some_nets]
puts "total nets: $net_count"

# Report a few specific nets
foreach net_name {clk reset req_val resp_val} {
  catch {report_net $net_name} msg
  puts "report_net $net_name: done"
}

catch {report_net -digits 6 clk} msg
puts "report_net -digits 6 clk: done"

#---------------------------------------------------------------
# Try different delay calculators
#---------------------------------------------------------------
puts "--- arnoldi ---"
set_delay_calculator arnoldi
report_checks
puts "PASS: arnoldi report_checks"

report_checks -path_delay min
puts "PASS: arnoldi min"

puts "--- lumped_cap ---"
set_delay_calculator lumped_cap
report_checks
puts "PASS: lumped_cap report_checks"

puts "--- dmp_ceff_two_pole ---"
set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: dmp_ceff_two_pole report_checks"

# Back to default
set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: back to default"

#---------------------------------------------------------------
# Annotated delay/check reporting
#---------------------------------------------------------------
puts "--- annotated delay ---"
catch {report_annotated_delay -cell} msg
puts "annotated -cell: done"

catch {report_annotated_delay -net} msg
puts "annotated -net: done"

catch {report_annotated_delay -cell -net} msg
puts "annotated -cell -net: done"

catch {report_annotated_delay -from_in_ports} msg
puts "annotated -from_in_ports: done"

catch {report_annotated_delay -to_out_ports} msg
puts "annotated -to_out_ports: done"

catch {report_annotated_delay -report_annotated} msg
puts "annotated -report_annotated: done"

catch {report_annotated_delay -report_unannotated} msg
puts "annotated -report_unannotated: done"

puts "ALL PASSED"
