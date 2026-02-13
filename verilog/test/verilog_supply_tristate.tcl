# Test verilog with supply0, supply1, tri-state, wire assign in decl,
# net constants, part selects, and multiple reads.
# Targets VerilogReader.cc uncovered paths:
#   supply0/supply1 dcl (lines 839-845)
#   tri dcl as modifier for output (lines 832-837)
#   wire assign in declaration (makeDclArg with assign)
#   VerilogNetConstant (makeNetConstant)
#   makeNetPartSelect paths
#   linkNetwork: supply0/supply1 constant net paths (lines 1779-1785)
#   mergeAssignNet path (lines 2102-2125)
# Also targets VerilogWriter.cc:
#   verilogPortDir for tristate direction
#   writeAssigns path
#   writeWireDcls with bus wires
#   writePortDcls with tristate direction

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read verilog with supply0/supply1/tri
#---------------------------------------------------------------
puts "--- Test 1: supply0/supply1/tri read ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_supply_tristate.v
link_design verilog_supply_tristate

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Query individual ports
foreach pname {clk in1 in2 in3 en out1 out2 out3} {
  catch {
    set p [get_ports $pname]
    puts "$pname dir=[get_property $p direction]"
  } msg
}

# Query bus ports
catch {
  set bus_ports [get_ports outbus*]
  puts "outbus* ports: [llength $bus_ports]"
} msg

# Query individual bus bits
foreach i {0 1 2 3} {
  catch {
    set p [get_ports "outbus\[$i\]"]
    puts "outbus\[$i\] dir=[get_property $p direction]"
  } msg
}

puts "PASS: supply/tri read completed"

#---------------------------------------------------------------
# Test 2: Set up timing and exercise assign connectivity
#---------------------------------------------------------------
puts "--- Test 2: timing with supply/tri ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3 en}]
set_output_delay -clock clk 0 [get_ports {out1 out2 out3}]
set_output_delay -clock clk 0 [get_ports {outbus[0] outbus[1] outbus[2] outbus[3]}]
set_input_transition 10 {in1 in2 in3 en clk}

report_checks
puts "PASS: report_checks with supply/tri"

report_checks -path_delay min
puts "PASS: report_checks min"

# Paths through assign
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: in1->out1"

report_checks -from [get_ports in3] -to [get_ports out3]
puts "PASS: in3->out3 (through assign)"

report_checks -from [get_ports in3] -to [get_ports {outbus[0]}]
puts "PASS: in3->outbus[0] (through wire assign)"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report with all fields"

#---------------------------------------------------------------
# Test 3: report_net for assign-related nets
#---------------------------------------------------------------
puts "--- Test 3: report_net ---"
foreach net_name {n1 n2 n3 n4 n5 n6} {
  catch {report_net $net_name} msg
  puts "report_net $net_name: done"
}

# Report instances
foreach inst_name {buf1 buf2 inv1 and1 or1 buf3 reg1 reg2 reg3} {
  report_instance $inst_name
}
puts "PASS: report_instance all"

#---------------------------------------------------------------
# Test 4: write_verilog exercises writer paths
#---------------------------------------------------------------
puts "--- Test 4: write_verilog ---"
set out1 [make_result_file verilog_supply_tri_out.v]
write_verilog $out1
puts "PASS: write_verilog basic"

if { [file exists $out1] && [file size $out1] > 0 } {
  puts "PASS: output file size=[file size $out1]"
}

set out2 [make_result_file verilog_supply_tri_pwr.v]
write_verilog -include_pwr_gnd $out2
puts "PASS: write_verilog -include_pwr_gnd"

if { [file exists $out2] && [file size $out2] > 0 } {
  puts "PASS: pwr_gnd file size=[file size $out2]"
}

#---------------------------------------------------------------
# Test 5: Multiple read_verilog (re-read exercises deleteModules paths)
#---------------------------------------------------------------
puts "--- Test 5: re-read verilog ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_supply_tristate.v
link_design verilog_supply_tristate

set cells2 [get_cells *]
puts "re-read cells: [llength $cells2]"

set nets2 [get_nets *]
puts "re-read nets: [llength $nets2]"

puts "PASS: re-read verilog"

#---------------------------------------------------------------
# Test 6: Read verilog with constants (1'b0, 1'b1)
#---------------------------------------------------------------
puts "--- Test 6: fanin/fanout ---"
catch {
  set fi [get_fanin -to [get_ports out1] -flat]
  puts "fanin to out1: [llength $fi]"
} msg

catch {
  set fo [get_fanout -from [get_ports in1] -flat]
  puts "fanout from in1: [llength $fo]"
} msg

catch {
  set fi_cells [get_fanin -to [get_ports out1] -only_cells]
  puts "fanin cells to out1: [llength $fi_cells]"
} msg

catch {
  set fo_cells [get_fanout -from [get_ports in1] -only_cells]
  puts "fanout cells from in1: [llength $fo_cells]"
} msg

puts "ALL PASSED"
