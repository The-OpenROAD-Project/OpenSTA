# write_sdc on a wide register: exceptions over all registers, inputs and
# outputs are written as collections, so the .sdc does not grow with the
# design, and reading it back gives the same timing as the constraints
# it was written from.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib

# in[i] -> BUF -> r[i] -> INV -> out[i]
proc write_wide_design { file width } {
  set stream [open $file w]
  puts $stream "module wide$width (clk, in, out);"
  puts $stream "  input clk;"
  puts $stream "  input \[[expr $width - 1]:0\] in;"
  puts $stream "  output \[[expr $width - 1]:0\] out;"
  for { set i 0 } { $i < $width } { incr i } {
    puts $stream "  wire d$i, q$i;"
    puts $stream "  BUF_X1 b$i (.A(in\[$i\]), .Z(d$i));"
    puts $stream "  DFF_X1 r$i (.D(d$i), .CK(clk), .Q(q$i));"
    puts $stream "  INV_X1 i$i (.A(q$i), .ZN(out\[$i\]));"
  }
  puts $stream "endmodule"
  close $stream
}

proc read_wide_design { width } {
  set verilog [make_result_file sdc_write_collections_wide$width.v]
  write_wide_design $verilog $width
  read_verilog $verilog
  link_design wide$width
}

proc constrain { } {
  create_clock -name clk -period 2 [get_ports clk]
  set_max_delay 1.5 -from [all_inputs -no_clocks] -to [all_registers]
  set_max_delay 1.2 -from [all_registers] -to [all_outputs]
  group_path -name io -from [all_inputs] -to [all_outputs]
}

proc timing_report { } {
  sta::redirect_string_begin
  report_checks -path_delay min_max -group_path_count 1000 \
    -fields {slew cap input_pins}
  report_tns
  report_wns
  return [sta::redirect_string_end]
}

proc file_lines { file } {
  set stream [open $file r]
  set lines [llength [split [read -nonewline $stream] "\n"]]
  close $stream
  return $lines
}

proc file_contents { file } {
  set stream [open $file r]
  set contents [read $stream]
  close $stream
  return $contents
}

read_wide_design 64
constrain
set report [timing_report]
set sdc1 [make_result_file sdc_write_collections_wide1.sdc]
write_sdc -no_timestamp $sdc1
report_file $sdc1

read_wide_design 64
read_sdc $sdc1
puts "re-read timing equal: [expr { [timing_report] eq $report }]"
set sdc2 [make_result_file sdc_write_collections_wide2.sdc]
write_sdc -no_timestamp $sdc2
puts "re-written sdc equal: [expr { [file_contents $sdc1] eq [file_contents $sdc2] }]"

# Collections stay a fixed size; all registers but one are still written
# one object at a time.
foreach width {64 256 1024} {
  read_wide_design $width
  constrain
  set sdc [make_result_file sdc_write_collections_wide$width.sdc]
  write_sdc -no_timestamp $sdc
  set collection_lines [file_lines $sdc]
  set_false_path -from [lrange [all_registers] 1 end] -to [all_outputs]
  write_sdc -no_timestamp $sdc
  set list_lines [expr [file_lines $sdc] - $collection_lines]
  puts "width $width: $collection_lines lines with collections, +$list_lines lines for a register list"
}
