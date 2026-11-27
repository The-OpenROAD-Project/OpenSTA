# Writes a flat verilog module with a configurable number of ports:
# scalar inputs in0..in<n-1> and outputs out0..out<n-1> joined by
# buffers, a bus d[7:0] -> q[7:0], escaped names containing brackets
# and a divider, and ports that differ only in case.
proc write_wide_ports_design { filename module n } {
  set stream [open $filename w]
  puts $stream "module $module ("
  puts -nonewline $stream "  clk, CLK, d, q, \\esc\[3\] , \\a/b , \\a/b_out "
  for { set i 0 } { $i < $n } { incr i } {
    puts -nonewline $stream ",\n  in$i, out$i"
  }
  puts $stream ");"
  puts $stream "  input clk, CLK, \\esc\[3\] , \\a/b ;"
  puts $stream "  output \\a/b_out ;"
  puts $stream "  input \[7:0\] d;"
  puts $stream "  output \[7:0\] q;"
  for { set i 0 } { $i < $n } { incr i } {
    puts $stream "  input in$i;"
    puts $stream "  output out$i;"
  }
  for { set i 0 } { $i < $n } { incr i } {
    puts $stream "  BUF_X1 b$i (.A(in$i), .Z(out$i));"
  }
  for { set i 0 } { $i < 8 } { incr i } {
    puts $stream "  BUF_X1 bd$i (.A(d\[$i\]), .Z(q\[$i\]));"
  }
  puts $stream "  AND2_X1 ab (.A1(\\esc\[3\] ), .A2(\\a/b ), .ZN(\\a/b_out ));"
  puts $stream "endmodule"
  close $stream
}
