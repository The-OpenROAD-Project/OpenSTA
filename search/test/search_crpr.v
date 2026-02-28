module search_crpr (clk, in1, in2, out1);
  input clk, in1, in2;
  output out1;
  wire n1, n2, n3, n4, clk_buf1, clk_buf2;

  // Clock tree with reconvergence
  CLKBUF_X1 ckbuf1 (.A(clk), .Z(clk_buf1));
  CLKBUF_X1 ckbuf2 (.A(clk_buf1), .Z(clk_buf2));

  // Combinational logic
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));

  // Two registers sharing reconvergent clock tree
  DFF_X1 reg1 (.D(n2), .CK(clk_buf1), .Q(n3));
  BUF_X1 buf2 (.A(n3), .Z(n4));
  DFF_X1 reg2 (.D(n4), .CK(clk_buf2), .Q(out1));
endmodule
