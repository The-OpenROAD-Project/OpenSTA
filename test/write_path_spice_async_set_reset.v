// Minimal repro for the write_path_spice asynchronous set/reset tie bug.
// One flop with both an active low set (SET_B) and an active low reset
// (RESET_B).  Both come from unconstrained ports, so STA knows no constant for
// them and write_path_spice must tie them to their deasserted level for a
// CLK -> Q path deck.
module repro (input clk, input d, input nset, input nreset, output q);
  sky130_fd_sc_hd__dfbbp_1 r0 (.CLK(clk), .D(d), .RESET_B(nreset), .SET_B(nset),
                               .Q(q));
endmodule
