module sdc_test1 (clk, in1, out1);
  input clk, in1;
  output out1;
  wire n1;

  BUF_X1 buf1 (.A(in1), .Z(n1));
  DFF_X1 reg1 (.D(n1), .CK(clk), .Q(out1));
endmodule
