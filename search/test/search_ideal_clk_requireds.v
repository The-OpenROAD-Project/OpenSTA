module search_ideal_clk_requireds (clk, in1, in2, out1, out2);
  input clk, in1, in2;
  output out1, out2;
  wire ck, q1, q2, e1, e2, en_b, gck1, gck2, q3, q4;
  CLKBUF_X1 ckbuf (.A(clk), .Z(ck));
  DFF_X1 r1 (.D(in1), .CK(ck), .Q(q1));
  DFF_X1 r2 (.D(in2), .CK(ck), .Q(q2));
  BUF_X1 u1 (.A(q1), .Z(e1));
  BUF_X1 u2 (.A(e1), .Z(en_b));
  CLKGATE_X1 icg (.CK(ck), .E(en_b), .GCK(gck1));
  BUF_X1 u3 (.A(q2), .Z(e2));
  AND2_X1 cg (.A1(ck), .A2(e2), .ZN(gck2));
  DFF_X1 r3 (.D(q1), .CK(gck1), .Q(q3));
  DFF_X1 r4 (.D(q2), .CK(gck2), .Q(q4));
  BUF_X1 o1 (.A(q3), .Z(out1));
  BUF_X1 o2 (.A(q4), .Z(out2));
endmodule
