# Read design and query network objects
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_test1.v
link_design network_test1

# Query cells
set cells [get_cells *]
puts "Cells: [llength $cells]"
if { [llength $cells] != 3 } {
  puts "FAIL: expected 3 cells"
  exit 1
}
puts "PASS: cell count correct"

# Query nets
set nets [get_nets *]
puts "Nets: [llength $nets]"
if { [llength $nets] == 0 } {
  puts "FAIL: no nets found"
  exit 1
}
puts "PASS: nets found"

# Query pins
set pins [get_pins buf1/*]
if { [llength $pins] == 0 } {
  puts "FAIL: no pins found on buf1"
  exit 1
}
puts "PASS: pins found on buf1"

# Query ports
set ports [get_ports *]
puts "Ports: [llength $ports]"
if { [llength $ports] != 4 } {
  puts "FAIL: expected 4 ports"
  exit 1
}
puts "PASS: port count correct"

puts "ALL PASSED"
