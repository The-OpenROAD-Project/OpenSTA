// b passes only the falling edge of clk.
//  r: rising-edge flop behind b, never sees the edge it launches on
//  f: falling-edge flop behind b, clocked
//  c: rising-edge flop on clk, clocked
module top (clk, d, q1, q2, q3);
  input clk, d;
  output q1, q2, q3;
  wire fclk;
  FALLBUF b (.A(clk), .Z(fclk));
  DFFHQNx1_ASAP7_75t_R r (.CLK(fclk), .D(d), .QN(q1));
  DFFLQNx1_ASAP7_75t_R f (.CLK(fclk), .D(d), .QN(q2));
  DFFHQNx1_ASAP7_75t_R c (.CLK(clk), .D(d), .QN(q3));
endmodule
