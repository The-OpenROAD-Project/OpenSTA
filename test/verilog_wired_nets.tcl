source helpers.tcl
read_liberty asap7_small.lib.gz

foreach net_type {wor wand} {
  foreach declaration [list "$net_type w;" "$net_type \[1:0\] w = a;"] {
    set verilog_file [make_result_file verilog_wired_nets.v]
    set stream [open $verilog_file w]
    puts $stream {module top (a, b, y);}
    puts $stream {  input a;}
    puts $stream {  input b;}
    puts $stream {  output y;}
    puts $stream "  $declaration"
    puts $stream {  assign w = a;}
    puts $stream {  assign w = b;}
    puts $stream {  BUFx2_ASAP7_75t_R g (.A(w), .Y(y));}
    puts $stream {endmodule}
    close $stream

    set rejected [catch {read_verilog $verilog_file} message]
    set expected "Error: 171 $verilog_file line 5, unsupported net type '$net_type'; resolve wired logic before static timing analysis"
    if {!$rejected || $message ne $expected} {
      error "$declaration: expected reader rejection, got '$message'"
    }
    puts "$declaration: rejected at declaration"
  }
}

if {![catch {link_design top} linked] && $linked} {
  error "rejected module remained linkable"
}
puts "rejected modules: not linkable"

# Ordinary assignments still preserve independent input connections.
set stream [open $verilog_file w]
puts $stream {module top (a, b, y, z);
  input a, b;
  output y, z;
  wire p, q;
  assign p = a;
  assign q = b;
  BUFx2_ASAP7_75t_R g1 (.A(p), .Y(y));
  BUFx2_ASAP7_75t_R g2 (.A(q), .Y(z));
endmodule}
close $stream
read_verilog $verilog_file
link_design top
if {[get_full_name [get_nets -of_objects [get_pins g1/A]]] ne "a"
    || [get_full_name [get_nets -of_objects [get_pins g2/A]]] ne "b"} {
  error "ordinary wire aliases lost independent input connections"
}
puts "ordinary wire aliases: independent inputs preserved"
