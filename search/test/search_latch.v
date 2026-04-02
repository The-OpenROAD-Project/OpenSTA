module search_latch (clk, in1, in2, out1, out2);
  input clk, in1, in2;
  output out1, out2;
  wire n1, n2, n3, n4, n5;

  // Combinational logic
  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));

  // Latch: DLH_X1 has D (data) and G (gate/enable)
  DLH_X1 latch1 (.D(n2), .G(clk), .Q(n3));

  // Another latch
  DLH_X1 latch2 (.D(n3), .G(clk), .Q(n4));

  // Regular flip-flop for cross-domain
  DFF_X1 reg1 (.D(n3), .CK(clk), .Q(n5));

  BUF_X1 buf2 (.A(n4), .Z(out1));
  BUF_X1 buf3 (.A(n5), .Z(out2));
endmodule
