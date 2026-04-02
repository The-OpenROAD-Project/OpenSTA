module search_data_check_gated (clk, en, in1, in2, rst, out1, out2, out3);
  input clk, en, in1, in2, rst;
  output out1, out2, out3;
  wire n1, n2, n3, n4, n5, n6, n7, gated_clk;

  // Clock gating: AND gate
  AND2_X1 clk_gate (.A1(clk), .A2(en), .ZN(gated_clk));

  // Combinational logic
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));
  INV_X1 inv1 (.A(n2), .ZN(n3));

  // Register on gated clock with async reset
  DFFR_X1 reg1 (.D(n3), .CK(gated_clk), .RN(rst), .Q(n4), .QN(n5));

  // Second register on main clock
  DFFR_X1 reg2 (.D(n4), .CK(clk), .RN(rst), .Q(n6), .QN(n7));

  // Outputs
  BUF_X1 buf2 (.A(n4), .Z(out1));
  BUF_X1 buf3 (.A(n6), .Z(out2));
  BUF_X1 buf4 (.A(n5), .Z(out3));
endmodule
