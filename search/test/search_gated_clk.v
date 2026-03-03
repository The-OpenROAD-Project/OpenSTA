module search_gated_clk (clk, en, in1, out1);
  input clk, en, in1;
  output out1;
  wire n1, n2, gated_clk, n3;

  // Clock gating cell: AND gate gating the clock
  AND2_X1 clk_gate (.A1(clk), .A2(en), .ZN(gated_clk));

  // Logic
  BUF_X1 buf1 (.A(in1), .Z(n1));

  // Register on gated clock
  DFF_X1 reg1 (.D(n1), .CK(gated_clk), .Q(n2));

  BUF_X1 buf2 (.A(n2), .Z(out1));
endmodule
