module power_activity_cases (clk, a, b, en, se, d, g, x, y, q1, q2, q3, q4, q5, z1, z2, w);
  input clk, a, b, en, se, d, g;
  output x, y, q1, q2, q3, q4, q5, z1, z2, w;
  wire gclk, clk_div, bb1, bb2;

  XOR2xp5_ASAP7_75t_R u_xor (.A(a), .B(b), .Y(x));
  AND2x2_ASAP7_75t_R u_and (.A(a), .B(b), .Y(y));
  ICGx1_ASAP7_75t_R u_icg (.CLK(clk), .ENA(en), .SE(se), .GCLK(gclk));
  DFFHQx4_ASAP7_75t_R r_gated (.D(d), .CLK(gclk), .Q(q1));
  DFFHQx4_ASAP7_75t_R r_reg (.D(d), .CLK(clk), .Q(q2));
  DFFHQx4_ASAP7_75t_R r_glitch (.D(g), .CLK(clk), .Q(q3));
  BUFx2_ASAP7_75t_R u_buf (.A(clk), .Y(clk_div));
  DFFHQx4_ASAP7_75t_R r_gen (.D(d), .CLK(clk_div), .Q(q4));
  DFF_NIQ r_niq (.D(d), .CK(clk), .QN(q5));
  BLACKBOX u_bb1 (.A(a), .Z(bb1));
  INVx1_ASAP7_75t_R u_inv1 (.A(bb1), .Y(z1));
  BLACKBOX u_bb2 (.A(a), .Z(bb2));
  INVx1_ASAP7_75t_R u_inv2 (.A(bb2), .Y(z2));
  AND2_WHEN u_when (.A(a), .B(b), .Z(w));
endmodule
