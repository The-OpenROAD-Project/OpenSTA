module search_crpr_data_checks (clk1, clk2, in1, in2, out1, out2);
  input clk1, clk2, in1, in2;
  output out1, out2;
  wire n1, n2, n3, n4, n5, n6, clk1_buf1, clk1_buf2, clk2_buf;

  // Clock tree 1 with reconvergence
  CLKBUF_X1 ck1buf1 (.A(clk1), .Z(clk1_buf1));
  CLKBUF_X1 ck1buf2 (.A(clk1_buf1), .Z(clk1_buf2));

  // Clock tree 2
  CLKBUF_X1 ck2buf (.A(clk2), .Z(clk2_buf));

  // Combinational logic
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));

  // Same-domain register pair (for CRPR)
  DFF_X1 reg1 (.D(n2), .CK(clk1_buf1), .Q(n3));
  BUF_X1 buf2 (.A(n3), .Z(n4));
  DFF_X1 reg2 (.D(n4), .CK(clk1_buf2), .Q(n5));

  // Cross-domain register
  DFF_X1 reg3 (.D(n5), .CK(clk2_buf), .Q(n6));

  BUF_X1 buf3 (.A(n5), .Z(out1));
  BUF_X1 buf4 (.A(n6), .Z(out2));
endmodule
