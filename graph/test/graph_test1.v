module graph_test1 (clk, d, q);
  input clk, d;
  output q;
  wire n1;

  DFF_X1 reg1 (.D(d), .CK(clk), .Q(n1));
  DFF_X1 reg2 (.D(n1), .CK(clk), .Q(q));
endmodule
