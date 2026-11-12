module top (in, se, out);
  input in, se;
  output out;
  COND_BUF u1 (.A(in), .SE(se), .Z(out));
endmodule
