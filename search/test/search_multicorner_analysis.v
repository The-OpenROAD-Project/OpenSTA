module search_multicorner_analysis (clk, in1, in2, in3, out1, out2);
  input clk, in1, in2, in3;
  output out1, out2;
  wire n1, n2, n3, n4, n5, n6, n7, clk_buf;

  // Clock buffer
  CLKBUF_X1 ckbuf (.A(clk), .Z(clk_buf));

  // Combinational logic chain (longer path for more interesting analysis)
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  OR2_X1 or1 (.A1(n1), .A2(in3), .ZN(n2));
  BUF_X1 buf1 (.A(n2), .Z(n3));
  INV_X1 inv1 (.A(n3), .ZN(n4));
  BUF_X2 buf2 (.A(n4), .Z(n5));

  // Register pair
  DFF_X1 reg1 (.D(n5), .CK(clk_buf), .Q(n6));
  BUF_X1 buf3 (.A(n6), .Z(n7));
  DFF_X1 reg2 (.D(n7), .CK(clk_buf), .Q(out1));

  // Direct output path
  BUF_X4 buf4 (.A(n6), .Z(out2));
endmodule
