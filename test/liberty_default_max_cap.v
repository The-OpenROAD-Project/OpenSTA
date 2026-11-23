module top(in, out);
  input in;
  output out;
  BUF u1 (.A(in), .Y(out));
endmodule
