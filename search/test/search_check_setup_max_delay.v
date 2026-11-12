// d -> r -> q is budgeted with set_max_delay; e -> u -> p has no
// constraint at all.
module top (clk, d, e, q, p);
  input clk, d, e;
  output q, p;

  DFFHQNx1_ASAP7_75t_R r (.CLK(clk), .D(d), .QN(q));
  DFFHQNx1_ASAP7_75t_R u (.CLK(clk), .D(e), .QN(p));
endmodule
