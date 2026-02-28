// Design with multi-driver nets and various gate types
// for testing GraphDelayCalc multi-driver net handling
module dcalc_multidriver_test (clk, in1, in2, in3, in4, sel, out1, out2, out3);
  input clk, in1, in2, in3, in4, sel;
  output out1, out2, out3;
  wire n1, n2, n3, n4, n5, n6, n7, n8, n9;

  // Chain path 1
  BUF_X1 buf1 (.A(in1), .Z(n1));
  INV_X1 inv1 (.A(n1), .ZN(n2));
  BUF_X2 buf2 (.A(n2), .Z(n3));

  // Chain path 2
  BUF_X4 buf3 (.A(in2), .Z(n4));
  AND2_X1 and1 (.A1(n4), .A2(in3), .ZN(n5));

  // Merging paths
  OR2_X1 or1 (.A1(n3), .A2(n5), .ZN(n6));
  NAND2_X1 nand1 (.A1(n6), .A2(sel), .ZN(n7));
  NOR2_X1 nor1 (.A1(n6), .A2(in4), .ZN(n8));

  // Output stage with registers
  DFF_X1 reg1 (.D(n7), .CK(clk), .Q(out1));
  DFF_X1 reg2 (.D(n8), .CK(clk), .Q(out2));
  BUF_X1 buf_out (.A(n6), .Z(out3));
endmodule
