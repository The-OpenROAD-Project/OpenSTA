module power_activity (clk, a, b, en, se, d, g, x, y, q1, q2, q3, q4, q5, z1, z2, w);
  input clk, a, b, en, se, d, g;
  output x, y, q1, q2, q3, q4, q5, z1, z2, w;
  wire gclk, clk_div, bb1, bb2;

  XOR2_X1 u_xor (.A(a), .B(b), .Z(x));
  AND2_X1 u_and (.A1(a), .A2(b), .ZN(y));
  CLKGATETST_X1 u_icg (.CK(clk), .E(en), .SE(se), .GCK(gclk));
  DFF_X1 r_gated (.D(d), .CK(gclk), .Q(q1));
  DFF_X1 r_reg (.D(d), .CK(clk), .Q(q2));
  DFF_X1 r_glitch (.D(g), .CK(clk), .Q(q3));
  CLKBUF_X1 u_buf (.A(clk), .Z(clk_div));
  DFF_X1 r_gen (.D(d), .CK(clk_div), .Q(q4));
  DFF_NIQ r_niq (.D(d), .CK(clk), .QN(q5));
  BLACKBOX u_bb1 (.A(a), .Z(bb1));
  INV_X1 u_inv1 (.A(bb1), .ZN(z1));
  BLACKBOX u_bb2 (.A(a), .Z(bb2));
  INV_X1 u_inv2 (.A(bb2), .ZN(z2));
  AND2_WHEN u_when (.A(a), .B(b), .Z(w));
endmodule
