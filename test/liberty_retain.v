module top (in1, in2, out1, out2);
  input in1, in2;
  output out1, out2;

  buf_retain u1 (.A(in1), .Y(out1));
  buf_retain_no_slew u2 (.A(in2), .Y(out2));
endmodule
