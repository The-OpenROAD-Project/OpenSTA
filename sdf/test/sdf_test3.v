module sdf_test3 (clk, d, en, q, q_inv);
  input clk, d, en;
  output q, q_inv;
  wire n1, n2, n3, n4;

  BUF_X1 buf1 (.A(d), .Z(n1));
  INV_X1 inv1 (.A(n1), .ZN(n2));
  AND2_X1 and1 (.A1(n2), .A2(en), .ZN(n3));
  OR2_X1 or1 (.A1(n1), .A2(n2), .ZN(n4));
  DFF_X1 reg1 (.D(n3), .CK(clk), .Q(q), .QN(q_inv));
endmodule
