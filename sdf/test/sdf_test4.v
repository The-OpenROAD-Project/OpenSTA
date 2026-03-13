module sdf_test4 (clk, d1, d2, en, q1, q2, q3);
  input clk, d1, d2, en;
  output q1, q2, q3;
  wire n1, n2, n3, n4, n5, n6, n7;

  BUF_X1 buf1 (.A(d1), .Z(n1));
  BUF_X2 buf2 (.A(d2), .Z(n2));
  INV_X1 inv1 (.A(n1), .ZN(n3));
  AND2_X1 and1 (.A1(n1), .A2(n2), .ZN(n4));
  OR2_X1 or1 (.A1(n3), .A2(n4), .ZN(n5));
  NAND2_X1 nand1 (.A1(n5), .A2(en), .ZN(n6));
  NOR2_X1 nor1 (.A1(n3), .A2(n6), .ZN(n7));
  DFF_X1 reg1 (.D(n5), .CK(clk), .Q(q1), .QN());
  DFF_X1 reg2 (.D(n6), .CK(clk), .Q(q2), .QN());
  DFF_X1 reg3 (.D(n7), .CK(clk), .Q(q3), .QN());
endmodule
