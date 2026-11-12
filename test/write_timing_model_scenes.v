module top (clk, a, d, y, q);
  input clk, a, d;
  output y, q;
  wire n1;

  BUF_X1 u1 (.A(a), .Z(n1));
  BUF_X1 u2 (.A(n1), .Z(y));
  DFF_X1 r1 (.D(d), .CK(clk), .Q(q));
endmodule
