module sub (in, out);
  input in;
  output out;
  BUFx2_ASAP7_75t_R u1 (.A(in), .Y(out));
endmodule

module top (a, z);
  input a;
  output z;
  wire n1;
  sub sub1 (.in(a), .out(n1));
  BUFx2_ASAP7_75t_R u2 (.A(n1), .Y(z));
endmodule
