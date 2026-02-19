# Test delay calculation annotation, slew queries, and load capacitance
# calculations with SDF annotation and multiple delay engines.
# Targets:
#   GraphDelayCalc.cc: findDelays, loadCap, loadDelay, gateDelay,
#     annotateArcDelay, annotateSlew, incrementalDelaysValid,
#     setObserver, delayCalcObserver, seedRootSlews,
#     findVertexDelay, findCheckEdgeDelays, deleteVertexBefore,
#     levelChangedBefore, levelsChangedBefore, delayInvalid (pin/vertex)
#   DmpCeff.cc: dmpCeffElmore, dmpCeffTwoPole with annotated values,
#     Ceff iteration with various effective capacitance conditions,
#     dmpCeffDrvrPi, ceffPiElmore, dmpCeffIter convergence
#   Graph.cc: arcDelayAnnotated, wireDelayAnnotated, slew and delay
#     getters for all rise/fall combinations

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../test/dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [get_ports {in1 clk}]

#---------------------------------------------------------------
# Baseline timing with default calculator
#---------------------------------------------------------------
puts "--- baseline timing ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

#---------------------------------------------------------------
# report_dcalc for all arcs: exercises gateDelay, loadDelay paths
#---------------------------------------------------------------
puts "--- report_dcalc all arcs ---"

# BUF arc: rise and fall
report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -max
puts "buf1 A->Z max: done"

report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z] -min
puts "buf1 A->Z min: done"

# INV arc: rise and fall
report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -max
puts "inv1 A->ZN max: done"

report_dcalc -from [get_pins inv1/A] -to [get_pins inv1/ZN] -min
puts "inv1 A->ZN min: done"

# DFF CK->Q arc
report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -max
puts "reg1 CK->Q max: done"

report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/Q] -min
puts "reg1 CK->Q min: done"

# DFF setup and hold check arcs
report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -max
puts "reg1 setup max: done"

report_dcalc -from [get_pins reg1/CK] -to [get_pins reg1/D] -min
puts "reg1 hold min: done"

#---------------------------------------------------------------
# Exercise different delay calculators and check delay values
# Targets: all delay calculator engines, copy/reinit paths
#---------------------------------------------------------------
puts "--- delay calculator engines ---"

# Unit delay calculator
set_delay_calculator unit
report_checks

report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "unit buf1: done"

# Lumped capacitance calculator
set_delay_calculator lumped_cap
report_checks

report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "lumped_cap buf1: done"

# DMP Ceff Elmore
set_delay_calculator dmp_ceff_elmore
report_checks

report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "dmp_elmore buf1: done"

# DMP Ceff Two Pole
set_delay_calculator dmp_ceff_two_pole
report_checks

report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "dmp_two_pole buf1: done"

# CCS Ceff
set_delay_calculator ccs_ceff
report_checks

# Switch back to default
set_delay_calculator dmp_ceff_elmore

#---------------------------------------------------------------
# Vary load capacitance to stress DmpCeff iteration
# Targets: DmpCeff convergence with different Ceff/Cload ratios
#---------------------------------------------------------------
puts "--- load variation ---"

foreach load_val {0.00001 0.0001 0.001 0.005 0.01 0.05 0.1 0.5 1.0 5.0} {
  set_load $load_val [get_ports out1]
  report_checks -from [get_ports in1] -to [get_ports out1]
  puts "load=$load_val: done"
}
set_load 0 [get_ports out1]

#---------------------------------------------------------------
# Vary input transition to exercise table lookup paths
# Targets: DmpCeff table lookup, findRange, dmpCeffTableLookup
#---------------------------------------------------------------
puts "--- slew variation ---"

foreach slew_val {0.001 0.005 0.01 0.05 0.1 0.2 0.5 1.0 2.0} {
  set_input_transition $slew_val [get_ports in1]
  report_checks -from [get_ports in1] -to [get_ports out1]
  puts "slew=$slew_val: done"
}
set_input_transition 0.1 [get_ports in1]

#---------------------------------------------------------------
# Incremental delay recalculation
# Targets: incrementalDelaysValid, seedInvalidDelays, delayInvalid
#---------------------------------------------------------------
puts "--- incremental delay calc ---"

# Change clock period
create_clock -name clk -period 5 [get_ports clk]
report_checks

# Change input delay
set_input_delay -clock clk 2.0 [get_ports in1]
report_checks

# Change output delay
set_output_delay -clock clk 3.0 [get_ports out1]
report_checks

# Reset and recheck
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
report_checks

#---------------------------------------------------------------
# Report checks with various formatting to exercise reporting paths
#---------------------------------------------------------------
puts "--- report formatting ---"

report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

report_checks -format full_clock_expanded

report_checks -endpoint_count 3

report_checks -unconstrained

report_checks -sort_by_slack

#---------------------------------------------------------------
# report_check_types exercises check edge delay queries
#---------------------------------------------------------------
puts "--- report_check_types ---"

report_check_types -max_delay -verbose

report_check_types -min_delay -verbose

#---------------------------------------------------------------
# report_slews for all pins: exercises slew getters
# (rise/fall, min/max combinations in Graph.cc)
#---------------------------------------------------------------
puts "--- report_slews ---"
report_slews [get_ports in1]
report_slews [get_ports out1]
report_slews [get_pins buf1/A]
report_slews [get_pins buf1/Z]
report_slews [get_pins inv1/A]
report_slews [get_pins inv1/ZN]
report_slews [get_pins reg1/D]
report_slews [get_pins reg1/CK]
report_slews [get_pins reg1/Q]
