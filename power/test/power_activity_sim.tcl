# simulate_power_activity on combinational logic between sources with
# formula activities. Toggles per cycle, clock period 6ns:
#   rc/X = x ^ buf(x) is 0, and mx/X = x ? (x | y) : (x & y) is x (0.5);
#     the formulas treat the inputs as independent.
#   r/Q keeps its formula activity (0.45, not the simulated 0.5 of its D)
#     and rx/X = r_q ^ !r_q is constant 1.
#   The clock gate and the clock buffer cb keep their formula activities.
#   l1/l2 is a combinational loop, evaluated in netlist order (warning).
source ../../test/helpers.tcl
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set verilog [make_result_file power_activity_sim.v]
set stream [open $verilog w]
puts $stream {module top (clk, x, y, en, lp, z, m, r_q, g_q, c_q, l);
  input clk, x, y, en, lp;
  output z, m, r_q, g_q, c_q, l;
  wire xb, a, o, r_n, r_x, gclk, cclk, l2y;
  sky130_fd_sc_hd__buf_1 rb (.A(x), .X(xb));
  sky130_fd_sc_hd__xor2_1 rc (.A(x), .B(xb), .X(z));
  sky130_fd_sc_hd__and2_1 ma (.A(x), .B(y), .X(a));
  sky130_fd_sc_hd__or2_1 mo (.A(x), .B(y), .X(o));
  sky130_fd_sc_hd__mux2_1 mx (.A0(a), .A1(o), .S(x), .X(m));
  sky130_fd_sc_hd__dfxtp_1 r (.CLK(clk), .D(m), .Q(r_q));
  sky130_fd_sc_hd__inv_1 ri (.A(r_q), .Y(r_n));
  sky130_fd_sc_hd__xor2_1 rx (.A(r_q), .B(r_n), .X(r_x));
  sky130_fd_sc_hd__dlclkp_1 icg (.CLK(clk), .GATE(en), .GCLK(gclk));
  sky130_fd_sc_hd__dfxtp_1 g (.CLK(gclk), .D(r_x), .Q(g_q));
  sky130_fd_sc_hd__clkbuf_1 cb (.A(clk), .X(cclk));
  sky130_fd_sc_hd__dfxtp_1 c (.CLK(cclk), .D(x), .Q(c_q));
  sky130_fd_sc_hd__nand2_1 l1 (.A(lp), .B(l2y), .Y(l));
  sky130_fd_sc_hd__nand2_1 l2 (.A(l), .B(lp), .Y(l2y));
endmodule}
close $stream
read_verilog $verilog
link_design top
create_clock -name clk -period 6 [get_ports clk]
set_input_delay 0 -clock clk [delete_from_list [all_inputs] [get_ports clk]]
set_power_activity -input_ports [get_ports {x lp}] -activity 0.5
set_power_activity -input_ports [get_ports y] -activity 0.1
set_power_activity -input_ports [get_ports en] -activity 0.1 -duty 0.2

proc show { pins } {
  foreach pin $pins {
    lassign [get_property [get_pins $pin] activity] density duty origin
    puts [format "%-6s toggles/cycle %.3f duty %.3f %s" \
            $pin [expr {$density * 6e-9}] $duty $origin]
  }
}
set pins {rb/X rc/X mx/X r/Q rx/X icg/GCLK cb/X l1/Y}

puts "Formulas."
show $pins
simulate_power_activity -cycles 4000 -lanes 256 -seed 1
show $pins
report_activity_annotation

puts "Annotations on a cell output and on a load pin are kept."
set_power_activity -pins [get_pins rc/X] -activity 0.2 -duty 0.5
set_power_activity -pins [get_pins mx/S] -activity 0 -duty 0
simulate_power_activity -cycles 4000 -lanes 256 -seed 1
show {rc/X mx/S mx/X}
unset_power_activity -pins [get_pins {rc/X mx/S}]

puts "An input change removes the annotations (the formulas are used)."
set_power_activity -input_ports [get_ports y] -activity 0.3
show mx/X
set_power_activity -input_ports [get_ports y] -activity 0.1

puts "Simulation mode, then formulas (the default)."
set_power_activity_mode simulation
show mx/X
set_power_activity_mode vectorless
show mx/X

puts "No clock."
delete_clock [all_clocks]
simulate_power_activity -cycles 4000 -lanes 256 -seed 1
