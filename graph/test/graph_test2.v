module graph_test2 (clk, d1, d2, en, q1, q2);
  input clk, d1, d2, en;
  output q1, q2;
  wire n1, n2, n3, n4, n5, n6;

  BUF_X1 buf1 (.A(d1), .Z(n1));
  BUF_X2 buf2 (.A(d2), .Z(n2));
  INV_X1 inv1 (.A(n1), .ZN(n3));
  AND2_X1 and1 (.A1(n3), .A2(en), .ZN(n4));
  OR2_X1 or1 (.A1(n2), .A2(n4), .ZN(n5));
  BUF_X1 buf3 (.A(n5), .Z(n6));
  DFF_X1 reg1 (.D(n4), .CK(clk), .Q(q1));
  DFF_X1 reg2 (.D(n6), .CK(clk), .Q(q2));
endmodule
