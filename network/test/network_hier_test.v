// Hierarchical design for testing hierarchical network traversal,
// HpinDrvrLoad, and SdcNetwork functions.
module sub_block (input A, input B, output Y);
  wire n1;
  AND2_X1 and_gate (.A1(A), .A2(B), .ZN(n1));
  BUF_X1 buf_gate (.A(n1), .Z(Y));
endmodule

module network_hier_test (clk, in1, in2, in3, out1, out2);
  input clk, in1, in2, in3;
  output out1, out2;
  wire w1, w2, w3, w4, w5;

  BUF_X1 buf_in (.A(in1), .Z(w1));
  sub_block sub1 (.A(w1), .B(in2), .Y(w2));
  sub_block sub2 (.A(w2), .B(in3), .Y(w3));
  INV_X1 inv1 (.A(w3), .ZN(w4));
  DFF_X1 reg1 (.D(w4), .CK(clk), .Q(w5));
  BUF_X2 buf_out1 (.A(w5), .Z(out1));
  BUF_X1 buf_out2 (.A(w3), .Z(out2));
endmodule
