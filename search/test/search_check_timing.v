module search_check_timing (clk, clk2, in1, in2, in3, in_unconst, out1, out2, out_unconst);
  input clk, clk2, in1, in2, in3, in_unconst;
  output out1, out2, out_unconst;
  wire n1, n2, n3, n4, n5, n6, n7;

  // Normal path
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));
  DFF_X1 reg1 (.D(n2), .CK(clk), .Q(n3));
  BUF_X1 buf2 (.A(n3), .Z(out1));

  // Register with second clock (multiple clocks scenario)
  DFF_X1 reg2 (.D(n3), .CK(clk2), .Q(n4));
  BUF_X1 buf3 (.A(n4), .Z(out2));

  // Unconstrained path (no set_output_delay on out_unconst)
  BUF_X1 buf4 (.A(in_unconst), .Z(n5));
  DFF_X1 reg3 (.D(n5), .CK(clk), .Q(n6));
  BUF_X1 buf5 (.A(n6), .Z(out_unconst));
endmodule
