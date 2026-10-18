// blk has a register-driven output and three constant ones: tied low,
// tied high, and an AND of a register with a tie-low cell, a constant
// only constant propagation finds. top instances blk's timing model
// and puts a register on each of its outputs.
module blk (clk, d, q, z0, z1, zand);
  input clk, d;
  output q, z0, z1, zand;
  wire qn, lo, hi;

  DFFHQNx1_ASAP7_75t_R r (.CLK(clk), .D(d), .QN(qn));
  BUFx2_ASAP7_75t_R bq (.A(qn), .Y(q));
  TIELOx1_ASAP7_75t_R tl (.L(lo));
  TIEHIx1_ASAP7_75t_R th (.H(hi));
  BUFx2_ASAP7_75t_R b0 (.A(lo), .Y(z0));
  BUFx2_ASAP7_75t_R b1 (.A(hi), .Y(z1));
  AND2x2_ASAP7_75t_R a (.A(qn), .B(lo), .Y(zand));
endmodule

module top (clk, d);
  input clk, d;
  wire q, z0, z1, zand;

  blk_model u (.clk(clk), .d(d), .q(q), .z0(z0), .z1(z1), .zand(zand));
  DFFHQNx1_ASAP7_75t_R rq (.CLK(clk), .D(q));
  DFFHQNx1_ASAP7_75t_R r0 (.CLK(clk), .D(z0));
  DFFHQNx1_ASAP7_75t_R r1 (.CLK(clk), .D(z1));
  DFFHQNx1_ASAP7_75t_R rz (.CLK(clk), .D(zand));
endmodule
