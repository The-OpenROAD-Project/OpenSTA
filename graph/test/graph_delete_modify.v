// Design for testing graph delete/modify operations:
// makeVertex/deleteVertex through connect_pin/disconnect_pin/delete_instance,
// replace_cell with different pin counts, multi-fanout nets,
// and reconvergent paths that exercise edge deletion.
module graph_delete_modify (clk, d1, d2, d3, rst, q1, q2, q3, q4);
  input clk, d1, d2, d3, rst;
  output q1, q2, q3, q4;
  wire n1, n2, n3, n4, n5, n6, n7, n8;

  // Chain: d1 -> buf1 -> n1 -> and1 -> n5
  BUF_X1 buf1 (.A(d1), .Z(n1));
  // Chain: d2 -> buf2 -> n2 -> and1, or1 (multi-fanout)
  BUF_X1 buf2 (.A(d2), .Z(n2));
  // Chain: d3 -> inv1 -> n3 -> or1
  INV_X1 inv1 (.A(d3), .ZN(n3));

  AND2_X1 and1 (.A1(n1), .A2(n2), .ZN(n5));
  OR2_X1 or1 (.A1(n2), .A2(n3), .ZN(n6));

  // Second stage
  NAND2_X1 nand1 (.A1(n5), .A2(n6), .ZN(n7));
  NOR2_X1 nor1 (.A1(n5), .A2(n3), .ZN(n8));

  // Registers with reset
  DFF_X1 reg1 (.D(n5), .CK(clk), .Q(q1));
  DFF_X1 reg2 (.D(n7), .CK(clk), .Q(q2));
  DFF_X1 reg3 (.D(n8), .CK(clk), .Q(q3));
  DFF_X1 reg4 (.D(n6), .CK(clk), .Q(q4));
endmodule
