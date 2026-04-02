# Test write_verilog should omit bias pins the same way it omits power/ground.

source ../../test/helpers.tcl

puts "--- write_verilog bias pins ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
read_verilog verilog_bias_pins.v
link_design top

set outfile [make_result_file verilog_bias_pins_out.v]
write_verilog $outfile

set outfile_pwr [make_result_file verilog_bias_pins_pwr.v]
write_verilog -include_pwr_gnd $outfile_pwr

set combined_out [make_result_file verilog_bias_pins_combined.v]
set out_stream [open $combined_out w]
foreach file [list $outfile $outfile_pwr] {
  set in_stream [open $file r]
  puts -nonewline $out_stream [read $in_stream]
  close $in_stream
}
close $out_stream

diff_files verilog_bias_pins_out.vok $combined_out
