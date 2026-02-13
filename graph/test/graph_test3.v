// Larger design for graph operations testing: more cell types, fan-in/fan-out,
// multiple clock domains, and reconvergent paths.
module graph_test3 (clk1, clk2, rst, d1, d2, d3, d4, q1, q2, q3);
  input clk1, clk2, rst, d1, d2, d3, d4;
  output q1, q2, q3;
  wire n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11, n12;

  // Input stage: buffers and inverters
  BUF_X1 buf1 (.A(d1), .Z(n1));
  BUF_X2 buf2 (.A(d2), .Z(n2));
  INV_X1 inv1 (.A(d3), .ZN(n3));
  INV_X2 inv2 (.A(d4), .ZN(n4));

  // Middle stage: logic gates
  AND2_X1 and1 (.A1(n1), .A2(n2), .ZN(n5));
  OR2_X1 or1 (.A1(n3), .A2(n4), .ZN(n6));
  NAND2_X1 nand1 (.A1(n5), .A2(n6), .ZN(n7));
  NOR2_X1 nor1 (.A1(n1), .A2(n3), .ZN(n8));

  // Reconvergent fan-out
  AND2_X2 and2 (.A1(n7), .A2(n8), .ZN(n9));
  OR2_X2 or2 (.A1(n7), .A2(n8), .ZN(n10));

  // Clock domain 1 registers
  DFF_X1 reg1 (.D(n9), .CK(clk1), .Q(n11));
  DFF_X1 reg2 (.D(n10), .CK(clk1), .Q(q1));

  // Clock domain 2 register (cross-domain)
  DFF_X1 reg3 (.D(n11), .CK(clk2), .Q(n12));
  BUF_X1 buf3 (.A(n12), .Z(q2));

  // Combinational output
  BUF_X4 buf4 (.A(n7), .Z(q3));
endmodule
