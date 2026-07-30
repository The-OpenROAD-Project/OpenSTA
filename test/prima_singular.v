module top (clk, in0, in1, out0, out1);
  input clk, in0, in1;
  output out0, out1;
  wire q0, q1, z0, z1;
  DFFHQx4_ASAP7_75t_R r0 (.D(in0), .CLK(clk), .Q(q0));
  BUFx2_ASAP7_75t_R u0 (.A(q0), .Y(z0));
  DFFHQx4_ASAP7_75t_R t0 (.D(z0), .CLK(clk), .Q(out0));
  DFFHQx4_ASAP7_75t_R r1 (.D(in1), .CLK(clk), .Q(q1));
  BUFx2_ASAP7_75t_R u1 (.A(q1), .Y(z1));
  DFFHQx4_ASAP7_75t_R t1 (.D(z1), .CLK(clk), .Q(out1));
endmodule
