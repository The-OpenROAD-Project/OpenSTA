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

report_checks -path_delay min

#---------------------------------------------------------------
# Read SPEF (large sky130 SPEF)
#---------------------------------------------------------------
puts "--- read_spef GCD ---"
read_spef ../../examples/gcd_sky130hd.spef

#---------------------------------------------------------------
# Report with parasitics
#---------------------------------------------------------------
puts "--- timing with SPEF ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

report_checks -sort_by_slack

#---------------------------------------------------------------
# Report parasitic annotation
#---------------------------------------------------------------
puts "--- parasitic annotation ---"
report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Report nets
#---------------------------------------------------------------
puts "--- report_net ---"
report_net clk
puts "report_net clk: done"

# Sample some nets from the design
set some_nets [get_nets *]
set net_count [llength $some_nets]
puts "total nets: $net_count"

# Report a few specific nets
foreach net_name {clk reset req_val resp_val} {
  report_net $net_name
  puts "report_net $net_name: done"
}

report_net -digits 6 clk
puts "report_net -digits 6 clk: done"

#---------------------------------------------------------------
# Try different delay calculators
#---------------------------------------------------------------
puts "--- arnoldi ---"
set_delay_calculator arnoldi
report_checks

report_checks -path_delay min

puts "--- lumped_cap ---"
set_delay_calculator lumped_cap
report_checks

puts "--- dmp_ceff_two_pole ---"
set_delay_calculator dmp_ceff_two_pole
report_checks

# Back to default
set_delay_calculator dmp_ceff_elmore
report_checks

#---------------------------------------------------------------
# Annotated delay/check reporting
#---------------------------------------------------------------
puts "--- annotated delay ---"
report_annotated_delay -cell
puts "annotated -cell: done"

report_annotated_delay -net
puts "annotated -net: done"

report_annotated_delay -cell -net
puts "annotated -cell -net: done"

report_annotated_delay -from_in_ports
puts "annotated -from_in_ports: done"

report_annotated_delay -to_out_ports
puts "annotated -to_out_ports: done"

report_annotated_delay -report_annotated
puts "annotated -report_annotated: done"

report_annotated_delay -report_unannotated
puts "annotated -report_unannotated: done"
