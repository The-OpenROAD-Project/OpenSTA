module top (clk, d, de, q, qb, qn);
  input clk, d, de;
  output q, qb, qn;
  EDFF r (.CLK(clk), .D(d), .DE(de), .Q(q));
  EDFF_QN rb (.CLK(clk), .D(d), .DE(de), .Q(qb), .Q_N(qn));
endmodule
