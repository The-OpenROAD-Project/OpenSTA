# unset_case_analysis propagates activity through the pin again.
source ../../test/helpers.tcl
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
set verilog [make_result_file power_unset_case.v]
set stream [open $verilog w]
puts $stream {module top (clk, a, b, y);
  input clk, a, b;
  output y;
  wire n1;
  sky130_fd_sc_hd__and2_1 g1 (.A(a), .B(b), .X(n1));
  sky130_fd_sc_hd__buf_1 g2 (.A(n1), .X(y));
endmodule}
close $stream
read_verilog $verilog
link_design top
create_clock -name clk -period 1 [get_ports clk]
set_power_activity -input_ports [get_ports {a b}] -activity 0.5 -duty 0.5

proc show { label } {
  lassign [get_property [get_pins g2/X] activity] density duty origin
  puts [format "%-8s g2/X transitions/cycle %.3f duty %.3f %s" \
          $label [expr {$density * 1e-9}] $duty $origin]
}
show fresh
set_case_analysis 0 [get_ports a]
show "a = 0"
unset_case_analysis [get_ports a]
show unset
