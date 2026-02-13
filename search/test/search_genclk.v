module search_genclk (clk, in1, in2, out1, out2);
  input clk, in1, in2;
  output out1, out2;
  wire n1, n2, n3, n4, n5, div_clk, clk_buf;

  // Clock buffer
  CLKBUF_X1 clkbuf (.A(clk), .Z(clk_buf));

  // Clock divider: DFF with inverted Q feedback to D
  // Q toggles every clock edge -> divide by 2
  DFF_X1 div_reg (.D(n5), .CK(clk_buf), .Q(div_clk), .QN(n5));

  // Combinational logic path 1
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));

  // Register on main clock domain
  DFF_X1 reg1 (.D(n2), .CK(clk_buf), .Q(n3));

  // Register on divided clock domain
  DFF_X1 reg2 (.D(n3), .CK(div_clk), .Q(n4));

  BUF_X1 buf2 (.A(n3), .Z(out1));
  BUF_X1 buf3 (.A(n4), .Z(out2));
endmodule
