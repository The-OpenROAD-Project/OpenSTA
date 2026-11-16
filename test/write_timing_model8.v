// write_timing_model input -> output arcs.
//  in1 -> buf -> out1          combinational through a cell
//  in2 -> out2                 input wired straight to output
//  in3 -> buf -> out3 -> r1/D  output that also drives a flop
//  in4 -> buf -> io -> buf -> out4   path through a bidirect net
//  r1/Q -> out5                clock to output
module top (clk, in1, in2, in3, in4, out1, out2, out3, out4, out5, io);
  input clk, in1, in2, in3, in4;
  output out1, out2, out3, out4, out5;
  inout io;
  BUF_X1 b1 (.A(in1), .Z(out1));
  assign out2 = in2;
  BUF_X1 b3 (.A(in3), .Z(out3));
  DFF_X1 r1 (.D(out3), .CK(clk), .Q(out5));
  BUF_X1 b4 (.A(in4), .Z(io));
  BUF_X1 b5 (.A(io), .Z(out4));
endmodule
