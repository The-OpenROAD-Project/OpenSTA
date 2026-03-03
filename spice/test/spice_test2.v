module spice_test2 (clk, in1, in2, out1, out2);
  input clk, in1, in2;
  output out1, out2;
  wire n1, n2, n3, n4;

  BUF_X1 buf1 (.A(in1), .Z(n1));
  INV_X1 inv1 (.A(in2), .ZN(n2));
  AND2_X1 and1 (.A1(n1), .A2(n2), .ZN(n3));
  OR2_X1 or1 (.A1(n1), .A2(n2), .ZN(n4));
  DFF_X1 reg1 (.D(n3), .CK(clk), .Q(out1));
  DFF_X1 reg2 (.D(n4), .CK(clk), .Q(out2));
endmodule
