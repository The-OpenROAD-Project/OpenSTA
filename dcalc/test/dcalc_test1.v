module dcalc_test1 (clk, in1, out1);
  input clk, in1;
  output out1;
  wire n1, n2;

  BUF_X1 buf1 (.A(in1), .Z(n1));
  INV_X1 inv1 (.A(n1), .ZN(n2));
  DFF_X1 reg1 (.D(n2), .CK(clk), .Q(out1));
endmodule
