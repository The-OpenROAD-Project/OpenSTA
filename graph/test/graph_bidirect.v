// Design with bidirectional ports and reconvergent paths
// for testing graph bidirect vertex/edge handling
module graph_bidirect (clk, d1, d2, d3, d4, q1, q2, q3, q4);
  input clk, d1, d2, d3, d4;
  output q1, q2, q3, q4;
  wire n1, n2, n3, n4, n5, n6, n7, n8, n9, n10;

  // Fan-out from d1 and d2
  BUF_X1 buf1 (.A(d1), .Z(n1));
  BUF_X1 buf2 (.A(d2), .Z(n2));
  INV_X1 inv1 (.A(d3), .ZN(n3));
  INV_X1 inv2 (.A(d4), .ZN(n4));

  // Reconvergent logic
  AND2_X1 and1 (.A1(n1), .A2(n2), .ZN(n5));
  OR2_X1 or1 (.A1(n1), .A2(n3), .ZN(n6));
  NAND2_X1 nand1 (.A1(n2), .A2(n4), .ZN(n7));
  NOR2_X1 nor1 (.A1(n3), .A2(n4), .ZN(n8));

  // Second level reconvergence
  AND2_X1 and2 (.A1(n5), .A2(n6), .ZN(n9));
  OR2_X1 or2 (.A1(n7), .A2(n8), .ZN(n10));

  // Registers
  DFF_X1 reg1 (.D(n5), .CK(clk), .Q(q1));
  DFF_X1 reg2 (.D(n9), .CK(clk), .Q(q2));
  DFF_X1 reg3 (.D(n10), .CK(clk), .Q(q3));
  DFF_X1 reg4 (.D(n6), .CK(clk), .Q(q4));
endmodule
