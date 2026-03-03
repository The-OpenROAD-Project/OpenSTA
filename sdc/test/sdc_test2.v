module sdc_test2 (clk1, clk2, in1, in2, in3, out1, out2);
  input clk1, clk2, in1, in2, in3;
  output out1, out2;
  wire n1, n2, n3, n4, n5, n6, n7;

  BUF_X1 buf1 (.A(in1), .Z(n1));
  INV_X1 inv1 (.A(in2), .ZN(n2));
  AND2_X1 and1 (.A1(n1), .A2(n2), .ZN(n3));
  OR2_X1 or1 (.A1(n1), .A2(in3), .ZN(n4));
  NAND2_X1 nand1 (.A1(n3), .A2(n4), .ZN(n5));
  NOR2_X1 nor1 (.A1(n3), .A2(n4), .ZN(n6));

  DFF_X1 reg1 (.D(n5), .CK(clk1), .Q(n7));
  DFF_X1 reg2 (.D(n6), .CK(clk1), .Q(out1));
  DFF_X1 reg3 (.D(n7), .CK(clk2), .Q(out2));
endmodule
