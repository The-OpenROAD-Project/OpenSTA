source verilog_input_alias_helpers.tcl

load_input_alias
set written_file [make_result_file verilog_input_alias_written.v]
write_verilog $written_file
set text [input_alias_file_bytes $written_file]
foreach alias {"assign internal_input = external_input;"
               "assign external_output = internal_output;"
               "assign external_io = internal_io;"} {
  if {[string first $alias $text] < 0} {
    error "missing or reversed alias: $alias"
  }
}
if {[string first "assign internal_io = external_io;" $text] >= 0} {
  error "inout alias changed direction"
}
puts "input/output/inout aliases: correct directions"

read_verilog $written_file
link_design alias_top
set starts [get_fanin -to [get_ports external_output] -startpoints_only]
if {[llength $starts] != 1
    || [get_full_name [lindex $starts 0]] ne "external_input"} {
  error "input connectivity lost after Verilog round trip"
}
puts "renamed input round trip: connected"
