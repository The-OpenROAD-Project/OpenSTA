module top (a, b, y, z);
  input a, b;
  output y, z;
  wire p, q;
  assign p = a;
  assign q = b;
  BUFx2_ASAP7_75t_R g1 (.A(p), .Y(y));
  BUFx2_ASAP7_75t_R g2 (.A(q), .Y(z));
endmodule
