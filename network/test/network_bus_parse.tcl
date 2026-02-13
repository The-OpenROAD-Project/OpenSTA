# Test bus/bundle name parsing and querying
# Targets: ParseBus.cc (parseBusName with range, subscript_wild, escapeChars)
#   ConcreteNetwork.cc (makeBusPort, groupBusPorts, isBus, isBundle, busName,
#     findBusBit, fromIndex, toIndex, size, portBitCount, findMember,
#     memberIterator, hasMember)
#   Network.cc (busIndexInRange, hasMember, findPortsMatching)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../verilog/test/verilog_complex_bus_test.v
link_design verilog_complex_bus_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_a[*]}]
set_input_delay -clock clk 0 [get_ports {data_b[*]}]
set_output_delay -clock clk 0 [get_ports {result[*]}]
set_output_delay -clock clk 0 [get_ports carry]
set_output_delay -clock clk 0 [get_ports overflow]

#---------------------------------------------------------------
# Test bus port queries (exercises isBus, fromIndex, toIndex, size)
#---------------------------------------------------------------
puts "--- bus port queries ---"

# Query all ports
set all_ports [get_ports *]
puts "total ports: [llength $all_ports]"

# Query bus ports by pattern
set data_a_ports [get_ports data_a*]
puts "data_a* ports: [llength $data_a_ports]"

set data_b_ports [get_ports data_b*]
puts "data_b* ports: [llength $data_b_ports]"

set result_ports [get_ports result*]
puts "result* ports: [llength $result_ports]"

#---------------------------------------------------------------
# Test individual bus bit queries
# Exercises ParseBus.cc parseBusName variants
#---------------------------------------------------------------
puts "--- individual bus bit queries ---"
foreach i {0 1 2 3 4 5 6 7} {
  catch {
    set p [get_ports "data_a\[$i\]"]
    set dir [get_property $p direction]
    puts "data_a\[$i\] direction: $dir"
  } msg
}

foreach i {0 1 2 3 4 5 6 7} {
  catch {
    set p [get_ports "result\[$i\]"]
    set dir [get_property $p direction]
    puts "result\[$i\] direction: $dir"
  } msg
}

#---------------------------------------------------------------
# Test wildcard bus subscript queries (exercises subscript_wild path)
#---------------------------------------------------------------
puts "--- wildcard bus subscript ---"
set wild_a [get_ports {data_a[*]}]
puts "data_a\[*\] ports: [llength $wild_a]"

set wild_b [get_ports {data_b[*]}]
puts "data_b\[*\] ports: [llength $wild_b]"

set wild_r [get_ports {result[*]}]
puts "result\[*\] ports: [llength $wild_r]"

#---------------------------------------------------------------
# Test get_pins with bus-style patterns
# Exercises parseBusName in pin query context
#---------------------------------------------------------------
puts "--- bus-style pin queries ---"
set all_pins [get_pins */*]
puts "all pins: [llength $all_pins]"

set buf_a_pins [get_pins buf_a*/*]
puts "buf_a* pins: [llength $buf_a_pins]"

set and_pins [get_pins and*/*]
puts "and* pins: [llength $and_pins]"

set reg_pins [get_pins reg*/*]
puts "reg* pins: [llength $reg_pins]"

#---------------------------------------------------------------
# Test get_nets with bus patterns
# Exercises parseBusName in net query context
#---------------------------------------------------------------
puts "--- bus-style net queries ---"
set all_nets [get_nets *]
puts "all nets: [llength $all_nets]"

set stage1_nets [get_nets stage1*]
puts "stage1* nets: [llength $stage1_nets]"

set stage2_nets [get_nets stage2*]
puts "stage2* nets: [llength $stage2_nets]"

#---------------------------------------------------------------
# Test get_cells with various patterns (exercises findChildrenMatching)
#---------------------------------------------------------------
puts "--- cell pattern queries ---"
set all_cells [get_cells *]
puts "total cells: [llength $all_cells]"

set buf_cells [get_cells buf*]
puts "buf* cells: [llength $buf_cells]"

set and_cells [get_cells and*]
puts "and* cells: [llength $and_cells]"

set reg_cells [get_cells reg*]
puts "reg* cells: [llength $reg_cells]"

set or_cells [get_cells or*]
puts "or* cells: [llength $or_cells]"

#---------------------------------------------------------------
# Test hierarchical queries on bus design
#---------------------------------------------------------------
puts "--- hierarchical queries ---"
set hier_cells [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier_cells]"

set hier_nets [get_nets -hierarchical *]
puts "hierarchical nets: [llength $hier_nets]"

set hier_pins [get_pins -hierarchical *]
puts "hierarchical pins: [llength $hier_pins]"

#---------------------------------------------------------------
# Test report_net on bus nets
#---------------------------------------------------------------
puts "--- report_net on bus nets ---"
foreach net {stage1[0] stage1[7] stage2[0] stage2[7]} {
  catch {report_net $net} msg
  puts "report_net $net: done"
}

#---------------------------------------------------------------
# Test report_instance on cells in bus design
#---------------------------------------------------------------
puts "--- report_instance on cells ---"
foreach inst {buf_a0 and0 reg0 or_carry buf_carry} {
  report_instance $inst
  puts "report_instance $inst: done"
}

#---------------------------------------------------------------
# Test get_lib_cells with patterns
# Exercises findPortsMatching on liberty cells
#---------------------------------------------------------------
puts "--- lib cell queries ---"
set buf_lib [get_lib_cells NangateOpenCellLibrary/BUF*]
puts "BUF* lib cells: [llength $buf_lib]"

set and_lib [get_lib_cells NangateOpenCellLibrary/AND*]
puts "AND* lib cells: [llength $and_lib]"

set or_lib [get_lib_cells NangateOpenCellLibrary/OR*]
puts "OR* lib cells: [llength $or_lib]"

set inv_lib [get_lib_cells NangateOpenCellLibrary/INV*]
puts "INV* lib cells: [llength $inv_lib]"

set dff_lib [get_lib_cells NangateOpenCellLibrary/DFF*]
puts "DFF* lib cells: [llength $dff_lib]"

#---------------------------------------------------------------
# Timing checks to exercise bus timing paths
#---------------------------------------------------------------
puts "--- timing analysis ---"
report_checks
report_checks -path_delay min
report_checks -from [get_ports {data_a[0]}] -to [get_ports {result[0]}]
report_checks -fields {slew cap input_pins nets fanout}

puts "ALL PASSED"
