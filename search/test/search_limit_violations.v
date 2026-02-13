module search_limit_violations (clk, in1, in2, in3, in4, out1, out2, out3);
  input clk, in1, in2, in3, in4;
  output out1, out2, out3;
  wire n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11;

  // Long combinational chain (creates high fanout and large delay)
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  OR2_X1 or1 (.A1(in3), .A2(in4), .ZN(n2));
  AND2_X1 and2 (.A1(n1), .A2(n2), .ZN(n3));
  BUF_X1 buf1 (.A(n3), .Z(n4));
  INV_X1 inv1 (.A(n4), .ZN(n5));
  BUF_X1 buf2 (.A(n5), .Z(n6));
  INV_X1 inv2 (.A(n6), .ZN(n7));

  // High fanout: n7 drives multiple loads
  BUF_X1 buf3 (.A(n7), .Z(n8));
  BUF_X1 buf4 (.A(n7), .Z(n9));
  BUF_X1 buf5 (.A(n7), .Z(n10));

  // Registers
  DFF_X1 reg1 (.D(n8), .CK(clk), .Q(n11));
  DFF_X1 reg2 (.D(n9), .CK(clk), .Q(out2));
  DFF_X1 reg3 (.D(n10), .CK(clk), .Q(out3));

  BUF_X1 buf6 (.A(n11), .Z(out1));
endmodule
