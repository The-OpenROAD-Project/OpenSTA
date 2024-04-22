module top (in1, out1);
  input in1;
  output out1;

  INVx8_ASAP7_75t_R u1 (.A(in1), .Y(out1));
  INVx8_ASAP7_75t_R u2 (.A(in1), .Y(out1));
endmodule // top
