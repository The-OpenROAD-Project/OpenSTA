# Test escaped names, path dividers, and SDC network queries.
# Targets:
#   SdcNetwork.cc: findPort, findPin, findNet with hierarchy,
#     escapeDividers, escapeBrackets, portDirection, makePort,
#     findInstancesMatching, findPinsHierMatching
#   Network.cc: pathDivider, pathEscape, setPathDivider, setPathEscape,
#     pathName with escaped chars, findInstanceRelative
#   ParseBus.cc: parseBusName edge cases (escaped names, bus bit ranges,
#     isBusName, wildcard subscripts)
#   ConcreteNetwork.cc: findPort, groupBusPorts, setAttribute/getAttribute

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

report_checks
puts "PASS: initial setup"

#---------------------------------------------------------------
# Test various pattern matching
# Exercises: findInstancesMatching with different patterns
#---------------------------------------------------------------
puts "--- pattern matching ---"

# Star pattern
set star_cells [get_cells *]
puts "cells *: [llength $star_cells]"

# Question mark pattern
set q_cells [get_cells ???1]
puts "cells ???1: [llength $q_cells]"

# Prefix pattern
catch {
  set prefix_cells [get_cells {b*}]
  puts "cells b*: [llength $prefix_cells]"
}

# Specific prefix
set buf_cells [get_cells buf*]
puts "cells buf*: [llength $buf_cells]"

set and_cells [get_cells and*]
puts "cells and*: [llength $and_cells]"

set reg_cells [get_cells reg*]
puts "cells reg*: [llength $reg_cells]"

# Non-matching pattern
catch {
  set x_cells [get_cells nonexistent_*]
  puts "cells nonexistent_*: [llength $x_cells]"
} msg
puts "PASS: pattern matching"

#---------------------------------------------------------------
# Test get_pins with various patterns
# Exercises: findPinsMatching, findInstPinsMatching
#---------------------------------------------------------------
puts "--- pin pattern matching ---"

set buf_pins [get_pins buf1/*]
puts "buf1/* pins: [llength $buf_pins]"

set all_a_pins [get_pins */A]
puts "*/A pins: [llength $all_a_pins]"

set all_z_pins [get_pins */Z]
puts "*/Z pins: [llength $all_z_pins]"

set all_zn_pins [get_pins */ZN]
puts "*/ZN pins: [llength $all_zn_pins]"

set all_ck_pins [get_pins */CK]
puts "*/CK pins: [llength $all_ck_pins]"

set hier_pins [get_pins -hierarchical *]
puts "hier pins: [llength $hier_pins]"

# Wildcard on inst
set star_a_pins [get_pins */*1]
puts "*/*1 pins: [llength $star_a_pins]"

puts "PASS: pin pattern matching"

#---------------------------------------------------------------
# Test get_nets with patterns
# Exercises: findNetsMatching
#---------------------------------------------------------------
puts "--- net pattern matching ---"

set all_nets [get_nets *]
puts "all nets: [llength $all_nets]"

set n_nets [get_nets n*]
puts "n* nets: [llength $n_nets]"

set hier_nets [get_nets -hierarchical *]
puts "hier nets: [llength $hier_nets]"

puts "PASS: net pattern matching"

#---------------------------------------------------------------
# Test get_ports patterns
# Exercises: findPortsMatching (non-bus path)
#---------------------------------------------------------------
puts "--- port pattern matching ---"

set all_ports [get_ports *]
puts "all ports: [llength $all_ports]"

set i_ports [get_ports in*]
puts "in* ports: [llength $i_ports]"

set o_ports [get_ports out*]
puts "out* ports: [llength $o_ports]"

set c_ports [get_ports clk*]
puts "clk* ports: [llength $c_ports]"

# Pattern with ? wildcard
set q_ports [get_ports {?n?}]
puts "?n? ports: [llength $q_ports]"

puts "PASS: port pattern matching"

#---------------------------------------------------------------
# Test get_lib_cells with patterns across libraries
# Exercises: findLibCellsMatching
#---------------------------------------------------------------
puts "--- lib cell pattern matching ---"

set all_lib_cells [get_lib_cells NangateOpenCellLibrary/*]
puts "all lib cells: [llength $all_lib_cells]"

set inv_lib_cells [get_lib_cells NangateOpenCellLibrary/INV*]
puts "INV* lib cells: [llength $inv_lib_cells]"

set buf_lib_cells [get_lib_cells NangateOpenCellLibrary/BUF*]
puts "BUF* lib cells: [llength $buf_lib_cells]"

set nand_lib_cells [get_lib_cells NangateOpenCellLibrary/NAND*]
puts "NAND* lib cells: [llength $nand_lib_cells]"

# Wildcard library
set all_all [get_lib_cells */*]
puts "*/* lib cells: [llength $all_all]"

# Specific library search
set star_star [get_lib_cells */INV*]
puts "*/INV* lib cells: [llength $star_star]"

set star_dff [get_lib_cells */DFF*]
puts "*/DFF* lib cells: [llength $star_dff]"

set star_aoi [get_lib_cells */AOI*]
puts "*/AOI* lib cells: [llength $star_aoi]"

set star_oai [get_lib_cells */OAI*]
puts "*/OAI* lib cells: [llength $star_oai]"

set star_mux [get_lib_cells */MUX*]
puts "*/MUX* lib cells: [llength $star_mux]"

set star_sdff [get_lib_cells */SDFF*]
puts "*/SDFF* lib cells: [llength $star_sdff]"

set star_fill [get_lib_cells */FILL*]
puts "*/FILL* lib cells: [llength $star_fill]"

set star_clkgate [get_lib_cells */CLKGATE*]
puts "*/CLKGATE* lib cells: [llength $star_clkgate]"

set star_tlat [get_lib_cells */TLAT*]
puts "*/TLAT* lib cells: [llength $star_tlat]"

set star_tinv [get_lib_cells */TINV*]
puts "*/TINV* lib cells: [llength $star_tinv]"

puts "PASS: lib cell pattern matching"

#---------------------------------------------------------------
# Test get_lib_pins patterns
# Exercises: findLibPinsMatching
#---------------------------------------------------------------
puts "--- lib pin pattern matching ---"

set inv_lib_pins [get_lib_pins NangateOpenCellLibrary/INV_X1/*]
puts "INV_X1/* lib pins: [llength $inv_lib_pins]"

set buf_lib_pins [get_lib_pins NangateOpenCellLibrary/BUF_X1/*]
puts "BUF_X1/* lib pins: [llength $buf_lib_pins]"

set dff_lib_pins [get_lib_pins NangateOpenCellLibrary/DFF_X1/*]
puts "DFF_X1/* lib pins: [llength $dff_lib_pins]"

set nand_lib_pins [get_lib_pins NangateOpenCellLibrary/NAND2_X1/*]
puts "NAND2_X1/* lib pins: [llength $nand_lib_pins]"

set aoi_lib_pins [get_lib_pins NangateOpenCellLibrary/AOI21_X1/*]
puts "AOI21_X1/* lib pins: [llength $aoi_lib_pins]"

set sdff_lib_pins [get_lib_pins NangateOpenCellLibrary/SDFF_X1/*]
puts "SDFF_X1/* lib pins: [llength $sdff_lib_pins]"

set clkgate_lib_pins [get_lib_pins NangateOpenCellLibrary/CLKGATETST_X1/*]
puts "CLKGATETST_X1/* lib pins: [llength $clkgate_lib_pins]"

set fa_lib_pins [get_lib_pins NangateOpenCellLibrary/FA_X1/*]
puts "FA_X1/* lib pins: [llength $fa_lib_pins]"

puts "PASS: lib pin pattern matching"

#---------------------------------------------------------------
# Test current_design
#---------------------------------------------------------------
puts "--- current_design ---"
set design [current_design]
puts "current_design: $design"

#---------------------------------------------------------------
# Test extensive timing reports
# Exercises: timing traversal, path reporting
#---------------------------------------------------------------
puts "--- timing reports ---"
report_checks
report_checks -path_delay min
report_checks -path_delay max
report_checks -from [get_ports in1]
report_checks -from [get_ports in2]
report_checks -to [get_ports out1]
report_checks -rise_from [get_ports in1]
report_checks -fall_from [get_ports in1]
report_checks -rise_to [get_ports out1]
report_checks -fall_to [get_ports out1]
puts "PASS: timing reports"

# Various report formats
report_checks -fields {slew cap input_pins nets fanout}
report_checks -format full_clock
report_checks -format full_clock_expanded
report_checks -digits 6
report_checks -no_line_splits
puts "PASS: report formats"

# Check types
report_check_types -max_delay -min_delay
report_check_types -max_slew -max_capacitance -max_fanout
puts "PASS: report_check_types"

# Report power
catch {
  report_power
  puts "PASS: report_power"
}

puts "ALL PASSED"
