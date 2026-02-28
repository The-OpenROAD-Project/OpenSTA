module network_test1 (clk, in1, in2, out1);
  input clk, in1, in2;
  output out1;
  wire n1, n2;

  BUF_X1 buf1 (.A(in1), .Z(n1));
  AND2_X1 and1 (.A1(n1), .A2(in2), .ZN(n2));
  DFF_X1 reg1 (.D(n2), .CK(clk), .Q(out1));
endmodule
