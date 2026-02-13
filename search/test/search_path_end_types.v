module search_path_end_types (clk, rst, in1, in2, out1, out2, out3);
  input clk, rst, in1, in2;
  output out1, out2, out3;
  wire n1, n2, n3, n4, n5, n6, n7, n8;

  // Combinational logic
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));

  // Register with async reset (for recovery/removal checks)
  DFFR_X1 reg1 (.D(n2), .CK(clk), .RN(rst), .Q(n3), .QN(n4));

  // Second register for reg-to-reg paths
  DFFR_X1 reg2 (.D(n3), .CK(clk), .RN(rst), .Q(n5), .QN(n6));

  // Output buffers
  BUF_X1 buf2 (.A(n3), .Z(out1));
  BUF_X1 buf3 (.A(n5), .Z(out2));
  BUF_X1 buf4 (.A(n4), .Z(out3));
endmodule
