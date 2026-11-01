// ICGs with constant enables.
//  g1: ENA=1 SE=0, always on
//  g2: ENA=0 SE=0, always off
module top (clk, d1, d2, q1, q2);
  input clk, d1, d2;
  output q1, q2;
  wire hi, lo, gclk1, gclk2;
  TIEHIx1_ASAP7_75t_R th (.H(hi));
  TIELOx1_ASAP7_75t_R tl (.L(lo));
  ICGx1_ASAP7_75t_R g1 (.CLK(clk), .ENA(hi), .SE(lo), .GCLK(gclk1));
  ICGx1_ASAP7_75t_R g2 (.CLK(clk), .ENA(lo), .SE(lo), .GCLK(gclk2));
  DFFHQNx1_ASAP7_75t_R r1 (.CLK(gclk1), .D(d1), .QN(q1));
  DFFHQNx1_ASAP7_75t_R r2 (.CLK(gclk2), .D(d2), .QN(q2));
endmodule
