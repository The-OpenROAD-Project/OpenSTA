module sdf_test2 (clk, d, q, en, rst);
  input clk, d, en, rst;
  output q;
  wire n1, n2, n3;

  BUF_X1 buf1 (.A(d), .Z(n1));
  BUF_X2 buf2 (.A(n1), .Z(n2));
  AND2_X1 and1 (.A1(n2), .A2(en), .ZN(n3));
  DFF_X1 reg1 (.D(n3), .CK(clk), .Q(q), .QN());
endmodule
