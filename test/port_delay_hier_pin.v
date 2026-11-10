module sub (in, out1, out2);
  input in;
  output out1, out2;
  BUFx2_ASAP7_75t_R u1 (.A(in), .Y(out1));
  BUFx2_ASAP7_75t_R u2 (.A(in), .Y(out2));
endmodule

module top (a, z1, z2);
  input a;
  output z1, z2;
  sub sub1 (.in(a), .out1(z1), .out2(z2));
endmodule
