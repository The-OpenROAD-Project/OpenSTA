# Test pattern matching functions for coverage improvement
# Targets: Network.cc uncovered functions:
#   findNetsMatching (line 914, hit=0), findNetsMatchingLinear (line 977, hit=0),
#   findPinsMatching (line 992, hit=0), findInstPinsMatching (line 1066, hit=0),
#   findInstancesMatching (line 724, hit=0), findInstancesMatching1 (line 744, hit=0),
#   findNetLinear (line 898, hit=0), findPinLinear (line 839, hit=0),
#   connectedNets(Pin) (line 580, hit=0),
#   pathNameLess(Instance) (line 287, hit=0), pathNameLess(Net) (line 486, hit=0),
#   pathNameCmp(Net) (line 493, hit=0),
#   isInside(Instance) (line 344, hit=0), isInside(Net) (line 416, hit=0),
#   isInside(Pin, Instance) (line 448, hit=0), isInside(Pin, Pin) (line 441, hit=0),
#   instanceCount (line 1098, 1111, hit=0), pinCount (line 1117, 1137, hit=0),
#   netCount (line 1143, 1163, hit=0), leafInstanceCount (line 1169, hit=0),
#   leafPinCount (line 1182, hit=0)
# Also targets: SdcNetwork.cc (findNet, busName)
#   NetworkCmp.cc (InstancePathNameLess, NetPathNameLess, sortByPathName for Instances/Nets)

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 10 {in1 in2 clk}

# Build timing graph
report_checks

#---------------------------------------------------------------
# Test get_cells with various wildcard patterns
# Exercises findInstancesMatching, findChildrenMatching
#---------------------------------------------------------------
puts "--- instance pattern matching ---"

# Exact match
set buf_exact [get_cells buf1]
puts "buf1 exact: [get_full_name $buf_exact]"

# Wildcard patterns
set buf_wild [get_cells buf*]
puts "buf* matches: [llength $buf_wild]"

set and_wild [get_cells and*]
puts "and* matches: [llength $and_wild]"

set reg_wild [get_cells reg*]
puts "reg* matches: [llength $reg_wild]"

set all_wild [get_cells *1]
puts "*1 matches: [llength $all_wild]"

set question_wild [get_cells ?uf1]
puts "?uf1 matches: [llength $question_wild]"

# Hierarchical instance queries
set hier_all [get_cells -hierarchical *]
puts "hier * cells: [llength $hier_all]"

set hier_buf [get_cells -hierarchical buf*]
puts "hier buf* cells: [llength $hier_buf]"

set hier_reg [get_cells -hierarchical reg*]
puts "hier reg* cells: [llength $hier_reg]"

#---------------------------------------------------------------
# Test get_nets with various wildcard patterns
# Exercises findNetsMatching, findNetsHierMatching
#---------------------------------------------------------------
puts "--- net pattern matching ---"

# Exact match
set n1_exact [get_nets n1]
puts "n1 exact: [get_full_name $n1_exact]"

# Wildcard patterns
set n_wild [get_nets n*]
puts "n* matches: [llength $n_wild]"

set all_nets [get_nets *]
puts "* nets: [llength $all_nets]"

# Hierarchical net queries
set hier_n [get_nets -hierarchical n*]
puts "hier n* nets: [llength $hier_n]"

set hier_all_nets [get_nets -hierarchical *]
puts "hier * nets: [llength $hier_all_nets]"

#---------------------------------------------------------------
# Test get_pins with various wildcard patterns
# Exercises findPinsMatching, findPinsHierMatching, findInstPinsMatching
#---------------------------------------------------------------
puts "--- pin pattern matching ---"

# Instance/port patterns
set buf1_all [get_pins buf1/*]
puts "buf1/* matches: [llength $buf1_all]"

set all_A [get_pins */A]
puts "*/A matches: [llength $all_A]"

set all_Z [get_pins */Z]
puts "*/Z matches: [llength $all_Z]"

set all_ZN [get_pins */ZN]
puts "*/ZN matches: [llength $all_ZN]"

set all_CK [get_pins */CK]
puts "*/CK matches: [llength $all_CK]"

set all_D [get_pins */D]
puts "*/D matches: [llength $all_D]"

set all_Q [get_pins */Q]
puts "*/Q matches: [llength $all_Q]"

# Hierarchical pin queries
set hier_all_pins [get_pins -hierarchical *]
puts "hier * pins: [llength $hier_all_pins]"

set hier_A_pins [get_pins -hierarchical *A*]
puts "hier *A* pins: [llength $hier_A_pins]"

#---------------------------------------------------------------
# Test get_ports with wildcard patterns
# Exercises findPortsMatching
#---------------------------------------------------------------
puts "--- port pattern matching ---"

set all_ports [get_ports *]
puts "* ports: [llength $all_ports]"

set in_ports [get_ports in*]
puts "in* ports: [llength $in_ports]"

set out_ports [get_ports out*]
puts "out* ports: [llength $out_ports]"

set clk_ports [get_ports clk*]
puts "clk* ports: [llength $clk_ports]"

# Filter expressions on ports
set input_ports [get_ports -filter "direction == input"]
puts "input ports: [llength $input_ports]"

set output_ports [get_ports -filter "direction == output"]
puts "output ports: [llength $output_ports]"

#---------------------------------------------------------------
# Test get_cells with filter expressions
# Exercises various filter paths in pattern matching
#---------------------------------------------------------------
puts "--- cell filter expressions ---"

set buf_ref [get_cells -filter "ref_name == BUF_X1" *]
puts "ref_name==BUF_X1: [llength $buf_ref]"

set and_ref [get_cells -filter "ref_name == AND2_X1" *]
puts "ref_name==AND2_X1: [llength $and_ref]"

set dff_ref [get_cells -filter "ref_name == DFF_X1" *]
puts "ref_name==DFF_X1: [llength $dff_ref]"

set x1_ref [get_cells -filter "ref_name =~ *X1" *]
puts "ref_name=~*X1: [llength $x1_ref]"

#---------------------------------------------------------------
# Test all_inputs / all_outputs / all_clocks / all_registers
# Exercises collection-based pattern matching
#---------------------------------------------------------------
puts "--- collection queries ---"

set inputs [all_inputs]
puts "all_inputs: [llength $inputs]"

set outputs [all_outputs]
puts "all_outputs: [llength $outputs]"

set clocks [all_clocks]
puts "all_clocks: [llength $clocks]"

set regs [all_registers]
puts "all_registers: [llength $regs]"

set reg_data [all_registers -data_pins]
puts "register data_pins: [llength $reg_data]"

set reg_clk [all_registers -clock_pins]
puts "register clock_pins: [llength $reg_clk]"

set reg_out [all_registers -output_pins]
puts "register output_pins: [llength $reg_out]"

#---------------------------------------------------------------
# Test get_lib_cells with patterns
# Exercises findCellsMatching
#---------------------------------------------------------------
puts "--- lib cell pattern matching ---"

set all_lib_cells [get_lib_cells */*]
puts "all lib cells: [llength $all_lib_cells]"

set buf_libs [get_lib_cells */BUF*]
puts "BUF* lib cells: [llength $buf_libs]"

set and_libs [get_lib_cells */AND*]
puts "AND* lib cells: [llength $and_libs]"

set dff_libs [get_lib_cells */DFF*]
puts "DFF* lib cells: [llength $dff_libs]"

#---------------------------------------------------------------
# Test report_checks with various from/to patterns
# Exercises full timing path traversal with pattern matching
#---------------------------------------------------------------
puts "--- report_checks with patterns ---"
report_checks -from [get_ports in1]
report_checks -from [get_ports in2]
report_checks -to [get_ports out1]
report_checks -from [get_pins buf1/A] -to [get_pins reg1/D]
report_checks -through [get_pins and1/ZN]
