module sdf_test1 (clk, d, q);
  input clk, d;
  output q;
  wire n1;

  BUF_X1 buf1 (.A(d), .Z(n1));
  DFF_X1 reg1 (.D(n1), .CK(clk), .Q(q));
endmodule
