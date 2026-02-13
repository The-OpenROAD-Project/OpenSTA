# Test SDC network querying and bus range operations for coverage improvement.
# Targets: SdcNetwork.cc (findPin, findNet, findInstance, busName,
#   escapeDividers, escapeBrackets, portDirection, findPortsMatching,
#   portBitCount, fromIndex, toIndex, hasMember, memberIterator,
#   findBusBit, isBus, isBundle, name, groupBusPorts)
# ParseBus.cc (parseBusName with range [0:7], subscript [*],
#   simple [0], isBusName, escapeChars)
# ConcreteNetwork.cc (findPort, findBusBit, makeBusPort, groupBusPorts,
#   busName, isBus, isBundle, fromIndex, toIndex, portBitCount,
#   hasMember, findMember, memberIterator, size)
# Network.cc (findPortsMatching with bus ranges, busIndexInRange,
#   hasMembers, setPathDivider, setPathEscape)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../verilog/test/verilog_complex_bus_test.v
link_design verilog_complex_bus_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_a[*]}]
set_input_delay -clock clk 0 [get_ports {data_b[*]}]
set_output_delay -clock clk 0 [get_ports {result[*]}]
set_output_delay -clock clk 0 [get_ports carry]
set_output_delay -clock clk 0 [get_ports overflow]
set_input_transition 0.1 [get_ports {data_a[*] data_b[*] clk}]

#---------------------------------------------------------------
# Test bus range queries [from:to]
# Exercises: parseBusName is_range path, findPortsMatching range path
#---------------------------------------------------------------
puts "--- bus range queries ---"

# Query using range notation
catch {
  set range_ports [get_ports {data_a[0:3]}]
  puts "data_a\[0:3\] ports: [llength $range_ports]"
} msg
if {[string match "*Error*" $msg]} {
  puts "data_a\[0:3\]: $msg"
}

catch {
  set range_ports2 [get_ports {data_a[4:7]}]
  puts "data_a\[4:7\] ports: [llength $range_ports2]"
} msg
if {[string match "*Error*" $msg]} {
  puts "data_a\[4:7\]: $msg"
}

catch {
  set range_ports3 [get_ports {result[0:3]}]
  puts "result\[0:3\] ports: [llength $range_ports3]"
} msg
if {[string match "*Error*" $msg]} {
  puts "result\[0:3\]: $msg"
}

# Reverse range (exercises from > to swap in findPortsMatching)
catch {
  set rev_range [get_ports {data_b[7:0]}]
  puts "data_b\[7:0\] ports: [llength $rev_range]"
} msg
if {[string match "*Error*" $msg]} {
  puts "data_b\[7:0\]: $msg"
}

#---------------------------------------------------------------
# Test wildcard subscript queries
# Exercises: parseBusName subscript_wild path
#---------------------------------------------------------------
puts "--- wildcard subscript queries ---"
set wild_a [get_ports {data_a[*]}]
puts "data_a\[*\] ports: [llength $wild_a]"

set wild_b [get_ports {data_b[*]}]
puts "data_b\[*\] ports: [llength $wild_b]"

set wild_r [get_ports {result[*]}]
puts "result\[*\] ports: [llength $wild_r]"

#---------------------------------------------------------------
# Test individual bit queries
# Exercises: parseBusName simple subscript path
#---------------------------------------------------------------
puts "--- individual bit queries ---"
foreach bus {data_a data_b result} {
  foreach i {0 1 3 5 7} {
    catch {
      set p [get_ports "${bus}\[$i\]"]
      set dir [get_property $p direction]
      set fn [get_full_name $p]
      puts "${bus}\[$i\]: dir=$dir name=$fn"
    } msg
  }
}

#---------------------------------------------------------------
# Test non-bus scalar port queries
# Exercises: parseBusName returns is_bus=false
#---------------------------------------------------------------
puts "--- scalar port queries ---"
set clk_p [get_ports clk]
puts "clk: [get_full_name $clk_p] dir=[get_property $clk_p direction]"

set carry_p [get_ports carry]
puts "carry: [get_full_name $carry_p] dir=[get_property $carry_p direction]"

set ovfl_p [get_ports overflow]
puts "overflow: [get_full_name $ovfl_p] dir=[get_property $ovfl_p direction]"

#---------------------------------------------------------------
# Test get_ports with glob patterns on bus ports
# Exercises: findPortsMatching with non-bus patterns
#---------------------------------------------------------------
puts "--- glob patterns on bus ports ---"
set data_ports [get_ports data*]
puts "data* ports: [llength $data_ports]"

set all_ports [get_ports *]
puts "all ports: [llength $all_ports]"

set star_result [get_ports result*]
puts "result* ports: [llength $star_result]"

set question_ports [get_ports {?arry}]
puts "?arry ports: [llength $question_ports]"

#---------------------------------------------------------------
# Test get_pins with bus-style patterns across hierarchy
# Exercises: findPinsMatching, findInstPinsMatching
#---------------------------------------------------------------
puts "--- pin queries on bus design ---"
set all_pins [get_pins */*]
puts "all flat pins: [llength $all_pins]"

set hier_pins [get_pins -hierarchical *]
puts "hierarchical pins: [llength $hier_pins]"

# Pin patterns matching specific ports
set a_pins [get_pins */A]
puts "*/A pins: [llength $a_pins]"

set z_pins [get_pins */Z]
puts "*/Z pins: [llength $z_pins]"

set zn_pins [get_pins */ZN]
puts "*/ZN pins: [llength $zn_pins]"

set ck_pins [get_pins */CK]
puts "*/CK pins: [llength $ck_pins]"

set d_pins [get_pins */D]
puts "*/D pins: [llength $d_pins]"

set q_pins [get_pins */Q]
puts "*/Q pins: [llength $q_pins]"

# Pins on specific instances by pattern
set buf_a_pins [get_pins buf_a*/*]
puts "buf_a* pins: [llength $buf_a_pins]"

set and_pins [get_pins and*/*]
puts "and* pins: [llength $and_pins]"

set reg_pins [get_pins reg*/*]
puts "reg* pins: [llength $reg_pins]"

#---------------------------------------------------------------
# Test get_nets with bus patterns
# Exercises: findNetsMatching with bus names
#---------------------------------------------------------------
puts "--- net queries on bus design ---"
set all_nets [get_nets *]
puts "all nets: [llength $all_nets]"

set stage1_nets [get_nets stage1*]
puts "stage1* nets: [llength $stage1_nets]"

set stage2_nets [get_nets stage2*]
puts "stage2* nets: [llength $stage2_nets]"

set hier_nets [get_nets -hierarchical *]
puts "hierarchical nets: [llength $hier_nets]"

#---------------------------------------------------------------
# Test get_cells with patterns in bus design
# Exercises: findInstancesMatching patterns
#---------------------------------------------------------------
puts "--- cell queries on bus design ---"
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

# Filter on ref_name
set buf_x1_cells [get_cells -filter "ref_name == BUF_X1" *]
puts "BUF_X1 cells: [llength $buf_x1_cells]"

set and2_cells [get_cells -filter "ref_name == AND2_X1" *]
puts "AND2_X1 cells: [llength $and2_cells]"

set dff_cells [get_cells -filter "ref_name == DFF_X1" *]
puts "DFF_X1 cells: [llength $dff_cells]"

#---------------------------------------------------------------
# Test report_net on bus element nets
# Exercises: net pin iteration on bus-connected nets
#---------------------------------------------------------------
puts "--- report_net on bus nets ---"
foreach idx {0 3 7} {
  catch {
    report_net "stage1\[$idx\]"
    puts "report_net stage1\[$idx\]: done"
  } msg
  catch {
    report_net "stage2\[$idx\]"
    puts "report_net stage2\[$idx\]: done"
  } msg
}

# Non-bus internal nets
catch {report_net internal_carry} msg
puts "report_net internal_carry: done"
catch {report_net internal_overflow} msg
puts "report_net internal_overflow: done"

#---------------------------------------------------------------
# Test report_instance on various cells
# Exercises: instancePinIterator, cell property queries
#---------------------------------------------------------------
puts "--- report_instance on bus cells ---"
foreach inst {buf_a0 buf_a7 and0 and7 reg0 reg7 or_carry and_ovfl buf_carry buf_ovfl} {
  catch {report_instance $inst} msg
  puts "report_instance $inst: done"
}

#---------------------------------------------------------------
# Test liberty library queries with patterns
# Exercises: findCellsMatching on liberty libraries
#---------------------------------------------------------------
puts "--- liberty library queries ---"
set libs [get_libs *]
puts "libraries: [llength $libs]"

set all_lib_cells [get_lib_cells */*]
puts "all lib cells: [llength $all_lib_cells]"

set nand_lib [get_lib_cells */NAND*]
puts "NAND* lib cells: [llength $nand_lib]"

set nor_lib [get_lib_cells */NOR*]
puts "NOR* lib cells: [llength $nor_lib]"

set mux_lib [get_lib_cells */MUX*]
puts "MUX* lib cells: [llength $mux_lib]"

set dff_lib [get_lib_cells */DFF*]
puts "DFF* lib cells: [llength $dff_lib]"

set aoi_lib [get_lib_cells */AOI*]
puts "AOI* lib cells: [llength $aoi_lib]"

set oai_lib [get_lib_cells */OAI*]
puts "OAI* lib cells: [llength $oai_lib]"

#---------------------------------------------------------------
# Test all_registers with bus design
#---------------------------------------------------------------
puts "--- registers in bus design ---"
set regs [all_registers]
puts "all_registers: [llength $regs]"

set reg_data [all_registers -data_pins]
puts "register data_pins: [llength $reg_data]"

set reg_clk [all_registers -clock_pins]
puts "register clock_pins: [llength $reg_clk]"

set reg_out [all_registers -output_pins]
puts "register output_pins: [llength $reg_out]"

#---------------------------------------------------------------
# Test timing with bus paths
# Exercises: full timing traversal through bus nets
#---------------------------------------------------------------
puts "--- timing analysis ---"
report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: min path"

report_checks -from [get_ports {data_a[0]}] -to [get_ports {result[0]}]
puts "PASS: data_a[0]->result[0]"

report_checks -from [get_ports {data_a[7]}] -to [get_ports {result[7]}]
puts "PASS: data_a[7]->result[7]"

report_checks -from [get_ports {data_a[7]}] -to [get_ports carry]
puts "PASS: data_a[7]->carry"

report_checks -from [get_ports {data_b[6]}] -to [get_ports overflow]
puts "PASS: data_b[6]->overflow"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report_checks with fields"

report_checks -endpoint_count 5
puts "PASS: report_checks endpoint_count 5"

report_checks -group_count 3
puts "PASS: report_checks group_count 3"

puts "ALL PASSED"
