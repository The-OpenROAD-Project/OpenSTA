module sdf_cond_pathpulse (clk, d1, d2, sel, q1, q2);
  input clk, d1, d2, sel;
  output q1, q2;
  wire n1, n2, n3, n4, n5;

  BUF_X1 buf1 (.A(d1), .Z(n1));
  INV_X1 inv1 (.A(d2), .ZN(n2));
  AND2_X1 and1 (.A1(n1), .A2(sel), .ZN(n3));
  OR2_X1 or1 (.A1(n2), .A2(n3), .ZN(n4));
  NAND2_X1 nand1 (.A1(n4), .A2(sel), .ZN(n5));
  DFF_X1 reg1 (.D(n4), .CK(clk), .Q(q1), .QN());
  DFF_X1 reg2 (.D(n5), .CK(clk), .Q(q2), .QN());
endmodule
