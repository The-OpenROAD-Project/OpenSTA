module top (clk, d, rn, sn, q);
  input clk, d, rn, sn;
  output q;
  DFFASRHQNx1_ASAP7_75t_R r1 (.CLK(clk), .D(d), .RESETN(rn), .SETN(sn), .QN(q));
endmodule
