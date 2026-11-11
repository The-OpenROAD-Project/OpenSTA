module top (clk, d, q, clk_out);
  input clk, d;
  output q, clk_out;
  DFF_X1 r1 (.D(d), .CK(clk), .Q(q));
  BUF_X1 u1 (.A(clk), .Z(clk_out));
endmodule
