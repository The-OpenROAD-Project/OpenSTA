module top (in1, in2, in3, in6, in7, in8, in9, in10, out1, out2, out3);
 input in1, in2, in3, in6, in7, in8, in9, in10;
 output out1, out2, out3;

 // victim
 AND3x4_ASAP7_75t_R u1 (.A(in1),
    .B(in2),
    .C(in3),
    .Y(u1y));

 // victim load
 AND3x1_ASAP7_75t_R l1 (.A(u1y),
    .B(),
    .C(),
    .Y(out1));

 // victim load
 AND2x2_ASAP7_75t_R l2 (.A(u1y),
    .B(),
    .Y());

 // victim load
 AND3x1_ASAP7_75t_R l3 (.A(),
    .B(u1y),
    .C(),
    .Y());

 // victim load
 AND3x2_ASAP7_75t_R l4 (.A(),
    .B(u1y),
    .C(),
    .Y());

 // a1 aggressor
 AND3x4_ASAP7_75t_R a1 (.A(in6),
    .B(in7),
    .C(in8),
    .Y(a1y));
 AND3x4_ASAP7_75t_R l5 (.A(),
    .B(a2y),
    .C(a1y),
    .Y());
 AND2x4_ASAP7_75t_R l6 (.A(a1y),
    .B(),
    .Y());
 AND2x4_ASAP7_75t_R l7 (.A(a1y),
    .B(a2y),
    .Y());

 // a2 aggressor
 NOR2x2_ASAP7_75t_R a2 (.A(in9),
    .B(in10),
    .Y(a2y));
 AND2x4_ASAP7_75t_R l8 (.A(),
    .B(a2y),
    .Y());
 NAND2x1_ASAP7_75t_R l9 (.A(),
    .B(a2y),
    .Y());

endmodule
