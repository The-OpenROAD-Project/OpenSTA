module search_test1 (clk, in1, in2, out1);
  input clk, in1, in2;
  output out1;
  wire n1, n2, n3;

  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));
  DFF_X1 reg1 (.D(n2), .CK(clk), .Q(n3));
  BUF_X1 buf2 (.A(n3), .Z(out1));
endmodule
