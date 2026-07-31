// Minimal repro for the write_path_spice non-unate side-input tie bug.
// One xor2: the timed path enters pin B, the side input A comes from an
// unconstrained port, so STA knows no constant for it and write_path_spice
// must pick a tie that matches the arc it sensitized.
module repro (input a, input s, output x);
  sky130_fd_sc_hd__xor2_1 x0 (.A(s), .B(a), .X(x));
endmodule
