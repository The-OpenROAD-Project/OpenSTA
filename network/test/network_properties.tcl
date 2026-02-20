# Test network property queries and edge cases for coverage improvement
#---------------------------------------------------------------
# Use ASAP7 design which has bus ports for bus coverage
#---------------------------------------------------------------
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

#---------------------------------------------------------------
# Test extensive property querying on cells
#---------------------------------------------------------------
puts "--- cell property queries ---"
set all_cells [get_cells *]
puts "total cells: [llength $all_cells]"

foreach cell_name {u1 u2 r1 r2 r3} {
  set inst [get_cells $cell_name]
  set ref [get_property $inst ref_name]
  set lib_cell [get_property $inst liberty_cell]
  if { $lib_cell != "NULL" && $lib_cell ne "" } {
    set lib [get_name [$lib_cell liberty_library]]
  } else {
    set lib ""
  }
  set full [get_full_name $inst]
  puts "$cell_name: ref=$ref lib=$lib full=$full"
}

#---------------------------------------------------------------
# Test pin property queries in depth
#---------------------------------------------------------------
puts "--- pin direction / connectivity ---"
foreach pin_path {u1/A u1/Y u2/A u2/B u2/Y r1/CLK r1/D r1/Q} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  set net [$pin net]
  if { $net != "NULL" && $net ne "" } {
    set net_name [get_full_name $net]
  } else {
    set net_name ""
  }
  puts "$pin_path: dir=$dir net=$net_name"
}

#---------------------------------------------------------------
# Test net connectivity queries
#---------------------------------------------------------------
puts "--- net queries ---"
set all_nets [get_nets *]
puts "total nets: [llength $all_nets]"

foreach net_name {r1q r2q u1z u2z in1 in2 out clk1 clk2 clk3} {
  set net [get_nets $net_name]
  puts "net $net_name: [get_full_name $net]"
}

#---------------------------------------------------------------
# Test report_net for multiple nets (exercises network pin/term iterators)
#---------------------------------------------------------------
puts "--- report_net for various nets ---"
foreach net_name {r1q u1z u2z} {
  report_net $net_name
}

#---------------------------------------------------------------
# Test report_instance for leaf instances
#---------------------------------------------------------------
puts "--- report_instance ---"
foreach inst_name {u1 u2 r1 r2 r3} {
  report_instance $inst_name
}

#---------------------------------------------------------------
# Test get_cells with various filter expressions
#---------------------------------------------------------------
puts "--- filter expressions on cells ---"
set bufs [get_cells -filter "ref_name =~ BUFx*" *]
puts "BUFx* cells: [llength $bufs]"

set invs [get_cells -filter "ref_name =~ INVx*" *]
puts "INVx* cells: [llength $invs]"

set dffs [get_cells -filter "ref_name =~ DFFC*" *]
puts "DFFC* cells: [llength $dffs]"

#---------------------------------------------------------------
# Test get_pins matching patterns
#---------------------------------------------------------------
puts "--- pin pattern matching ---"
set clk_pins [get_pins */CLK]
puts "*/CLK pins: [llength $clk_pins]"

set d_pins [get_pins */D]
puts "*/D pins: [llength $d_pins]"

set q_pins [get_pins */Q]
puts "*/Q pins: [llength $q_pins]"

set y_pins [get_pins */Y]
puts "*/Y pins: [llength $y_pins]"

#---------------------------------------------------------------
# Test hierarchical and flat queries
#---------------------------------------------------------------
puts "--- hierarchical queries ---"
set hier_cells [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier_cells]"

set hier_nets [get_nets -hierarchical *]
puts "hierarchical nets: [llength $hier_nets]"

set hier_pins [get_pins -hierarchical *]
puts "hierarchical pins: [llength $hier_pins]"

#---------------------------------------------------------------
# Test port queries
#---------------------------------------------------------------
puts "--- port queries ---"
set all_ports [get_ports *]
puts "total ports: [llength $all_ports]"

foreach port_name {in1 in2 out clk1 clk2 clk3} {
  set p [get_ports $port_name]
  set dir [get_property $p direction]
  puts "port $port_name: direction=$dir"
}

#---------------------------------------------------------------
# Test liberty library iteration
#---------------------------------------------------------------
puts "--- liberty library queries ---"
set libs [get_libs *]
puts "libraries count: [llength $libs]"

foreach lib $libs {
  puts "lib: [get_name $lib]"
}

#---------------------------------------------------------------
# Test get_lib_cells with patterns
#---------------------------------------------------------------
puts "--- lib cell pattern queries ---"
set all_lib_cells [get_lib_cells */*]
puts "all lib cells: [llength $all_lib_cells]"

set buf_lib_cells [get_lib_cells */BUF*]
puts "BUF* lib cells: [llength $buf_lib_cells]"

set inv_lib_cells [get_lib_cells */INV*]
puts "INV* lib cells: [llength $inv_lib_cells]"

#---------------------------------------------------------------
# Test all_inputs / all_outputs / all_clocks / all_registers
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
puts "all_registers -data_pins: [llength $reg_data]"

set reg_clk [all_registers -clock_pins]
puts "all_registers -clock_pins: [llength $reg_clk]"

set reg_output [all_registers -output_pins]
puts "all_registers -output_pins: [llength $reg_output]"

#---------------------------------------------------------------
# Test report_checks to ensure timing graph is built
#---------------------------------------------------------------
puts "--- timing analysis ---"
report_checks
report_checks -path_delay min
report_checks -fields {slew cap input_pins nets fanout}
