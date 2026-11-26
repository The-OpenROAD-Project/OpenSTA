# Writes a wide-port Nangate45 netlist for write_timing_model tests.
#  in0           enables every register's mux and every feed-through output
#  in1..in<sel>  select inputs, each reaching 1/sel of the first stage
#  the rest      data inputs into the first stage
# A lane per register goes through <stages> butterfly stages, each
# lane NANDed with the lane 2^(s-1) away at stage s, so after s stages
# an input reaches about 2^s lanes: log2(regs) stages give every input
# a cone of every register. Half the outputs are combinational from
# the inputs and half are register outputs. With latch 1 the last input
# reaches the last output through a latch.
proc write_wide_model_design { filename inputs regs outputs sel stages latch } {
  set data_count [expr { $inputs - 1 - $sel }]
  set stream [open $filename w]
  set ports [list clk]
  for { set i 0 } { $i < $inputs } { incr i } {
    lappend ports "in$i"
  }
  for { set o 0 } { $o < $outputs } { incr o } {
    lappend ports "out$o"
  }
  puts $stream "module wide_model ([join $ports {, }]);"
  puts $stream "  input clk;"
  for { set i 0 } { $i < $inputs } { incr i } {
    puts $stream "  input in$i;"
  }
  for { set o 0 } { $o < $outputs } { incr o } {
    puts $stream "  output out$o;"
  }
  for { set r 0 } { $r < $regs } { incr r } {
    set data "in[expr { 1 + $sel + $r % $data_count }]"
    set select "in[expr { 1 + $r % $sel }]"
    puts $stream "  wire s0_$r, d$r, q$r;"
    puts $stream "  XOR2_X1 gx$r (.A($data), .B($select), .Z(s0_$r));"
  }
  for { set s 1 } { $s <= $stages } { incr s } {
    set stride [expr { 1 << ($s - 1) }]
    for { set r 0 } { $r < $regs } { incr r } {
      set p [expr { $s - 1 }]
      set partner [expr { ($r + $stride) % $regs }]
      puts $stream "  wire s${s}_$r;"
      puts $stream "  NAND2_X1 gs${s}_$r (.A1(s${p}_$r), .A2(s${p}_$partner), .ZN(s${s}_$r));"
    }
  }
  for { set r 0 } { $r < $regs } { incr r } {
    set prev_q "q[expr { ($r + $regs - 1) % $regs }]"
    puts $stream "  MUX2_X1 gm$r (.A($prev_q), .B(s${stages}_$r), .S(in0), .Z(d$r));"
    puts $stream "  DFF_X1 r$r (.D(d$r), .CK(clk), .Q(q$r));"
  }
  set comb [expr { $outputs / 2 }]
  for { set o 0 } { $o < $outputs } { incr o } {
    if { $latch && $o == $outputs - 1 } {
      puts $stream "  DLH_X1 lat (.D(in[expr { $inputs - 1 }]), .G(clk), .Q(out$o));"
    } elseif { $o < $comb } {
      set a "in[expr { 1 + $o % ($inputs - 1) }]"
      set b "in[expr { 1 + ($o * 7 + 3) % ($inputs - 1) }]"
      puts $stream "  wire c$o;"
      puts $stream "  NAND2_X1 gc$o (.A1($b), .A2(in0), .ZN(c$o));"
      puts $stream "  XOR2_X1 go$o (.A($a), .B(c$o), .Z(out$o));"
    } else {
      puts $stream "  BUF_X1 gq$o (.A(q[expr { $o % $regs }]), .Z(out$o));"
    }
  }
  puts $stream "endmodule"
  close $stream
}
