module search_fanin_fanout_nets (clk, in1, in2, in3, out1, out2, out3);
  input clk, in1, in2, in3;
  output out1, out2, out3;
  wire n1, n2, n3;

  AND2_X1 and1 (.A1(in1), .A2(in2), .ZN(n1));
  BUF_X1 buf1 (.A(n1), .Z(n2));
  INV_X1 inv1 (.A(in3), .ZN(n3));
  DFF_X1 reg1 (.D(n1), .CK(clk), .Q(out1));
  DFF_X1 reg2 (.D(n2), .CK(clk), .Q(out2));
  DFF_X1 reg3 (.D(n3), .CK(clk), .Q(out3));
endmodule
