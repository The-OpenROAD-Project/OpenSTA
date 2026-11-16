module top (in1, in2, in3, in4, out);
  input in1, in2, in3, in4;
  output out;
  wire n1;
  A2O1A1Ixp33_ASAP7_75t_L u1 (.A1(in1), .A2(in2), .B(in3), .C(in4), .Y(n1));
  A2O1A1Ixp33_ASAP7_75t_L u2 (.A1(n1), .A2(in2), .B(in3), .C(in4), .Y(out));
endmodule
