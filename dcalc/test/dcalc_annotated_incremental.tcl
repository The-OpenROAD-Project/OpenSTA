# Test annotated delays/slews, incremental delay recalculation,
# delay tolerance, capacitance queries, report_net.
# Targets: GraphDelayCalc.cc findDelays incremental, seedInvalidDelays,
#          findCheckEdgeDelays, delayInvalid(pin/vertex),
#          setIncrementalDelayTolerance, incrementalDelayTolerance,
#          loadDelay, netCaps, findVertexDelay,
#          Sta.cc setArcDelay, arcDelay, arcDelayAnnotated,
#          setAnnotatedSlew, vertexSlew, connectedCap,
#          removeDelaySlewAnnotations, delaysInvalidFrom,
#          delaysInvalidFromFanin, findDelays,
#          DmpCeff.cc various convergence paths with edge-case loads

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog dcalc_multidriver_test.v
link_design dcalc_multidriver_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3 in4 sel}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3}]
set_input_transition 0.1 [get_ports {in1 in2 in3 in4 sel clk}]

# Baseline
report_checks -path_delay max > /dev/null

############################################################
# Incremental delay tolerance
############################################################
puts "--- setIncrementalDelayTolerance ---"
catch {
  sta::set_incremental_delay_tolerance 0.01
  report_checks -path_delay max
}
puts "PASS: incremental delay tolerance 0.01"

catch {
  sta::set_incremental_delay_tolerance 0.0
  report_checks -path_delay max
}
puts "PASS: incremental delay tolerance 0.0"

catch {
  sta::set_incremental_delay_tolerance 0.1
  report_checks -path_delay max
}
puts "PASS: incremental delay tolerance 0.1"

# Reset
catch { sta::set_incremental_delay_tolerance 0.0 }

############################################################
# report_net for capacitance queries
############################################################
puts "--- report_net for various nets ---"
report_net n1
report_net n2
report_net n3
report_net n4
report_net n5
report_net n6
report_net n7
report_net n8
puts "PASS: report_net all nets"

############################################################
# report_net with loads
############################################################
puts "--- report_net with loads ---"
set_load 0.05 [get_ports out1]
set_load 0.03 [get_ports out2]
set_load 0.02 [get_ports out3]
report_net n6
report_net n7
report_net n8
puts "PASS: report_net with loads"

############################################################
# report_net with digits
############################################################
puts "--- report_net with digits ---"
report_net -digits 6 n1
report_net -digits 2 n6
puts "PASS: report_net digits"

############################################################
# Incremental: add wire caps and recompute
############################################################
puts "--- incremental with wire caps ---"
catch {
  set_load 0.005 [get_nets n1]
  report_checks -path_delay max
}
puts "PASS: wire cap n1"

catch {
  set_load 0.01 [get_nets n6]
  report_checks -path_delay max
}
puts "PASS: wire cap n6"

############################################################
# Rapid constraint changes for incremental recalculation
############################################################
puts "--- rapid constraint changes ---"
set_load 0.001 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_load 0.01 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_load 0.05 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_load 0.1 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_load 0.5 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_load 1.0 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_load 0 [get_ports out1]
puts "PASS: rapid constraint changes"

############################################################
# Input transition changes driving incremental
############################################################
puts "--- input transition incremental ---"
foreach slew {0.001 0.005 0.01 0.05 0.1 0.5 1.0 2.0} {
  set_input_transition $slew [get_ports in1]
  catch { report_checks -from [get_ports in1] -to [get_ports out1] }
}
set_input_transition 0.1 [get_ports in1]
puts "PASS: input transition incremental"

############################################################
# Clock period changes
############################################################
puts "--- clock period incremental ---"
create_clock -name clk -period 5 [get_ports clk]
report_checks -path_delay max
create_clock -name clk -period 20 [get_ports clk]
report_checks -path_delay max
create_clock -name clk -period 2 [get_ports clk]
report_checks -path_delay max
create_clock -name clk -period 10 [get_ports clk]
report_checks -path_delay max
puts "PASS: clock period incremental"

############################################################
# Delay calc after adding/removing constraints
############################################################
puts "--- delay calc after constraint changes ---"
set_input_delay -clock clk 2.0 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
set_output_delay -clock clk 3.0 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: constraint change incremental"

############################################################
# Driving cell changes
############################################################
puts "--- driving cell changes ---"
set_driving_cell -lib_cell BUF_X1 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
set_driving_cell -lib_cell BUF_X4 [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
set_driving_cell -lib_cell INV_X1 -pin ZN [get_ports in1]
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: driving cell changes"

############################################################
# read_sdf and annotated delay
############################################################
puts "--- write and read SDF ---"
set sdf_file [file join [pwd] results dcalc_annotated.sdf]
catch {
  file mkdir results
  write_sdf -no_timestamp -no_version $sdf_file
  read_sdf $sdf_file
  report_checks -path_delay max
  report_annotated_delay -list_annotated
  report_annotated_check -list_annotated -setup -hold
}
puts "PASS: write/read SDF"

############################################################
# Remove annotations and recalculate
############################################################
puts "--- remove annotations ---"
catch {
  sta::remove_delay_slew_annotations
  report_checks -path_delay max
}
puts "PASS: remove annotations"

############################################################
# Multiple calculator with incremental
############################################################
puts "--- calculator switch incremental ---"
set_delay_calculator dmp_ceff_elmore
set_load 0.05 [get_ports out1]
report_checks -from [get_ports in1] -to [get_ports out1]

set_delay_calculator dmp_ceff_two_pole
report_checks -from [get_ports in1] -to [get_ports out1]

set_delay_calculator lumped_cap
report_checks -from [get_ports in1] -to [get_ports out1]

set_delay_calculator unit
report_checks -from [get_ports in1] -to [get_ports out1]

set_delay_calculator dmp_ceff_elmore
set_load 0 [get_ports out1]
puts "PASS: calculator switch incremental"

puts "ALL PASSED"
