// Verilog design with assign statements and continuous assignments
// Exercises VerilogReader.cc assign statement paths
module verilog_assign_test (clk, in1, in2, in3, out1, out2, out3);
  input clk, in1, in2, in3;
  output out1, out2, out3;
  wire n1, n2, n3, n4, n5;
  wire assigned_net;

  // Continuous assignment (assign statement)
  assign assigned_net = in3;

  BUF_X1 buf1 (.A(in1), .Z(n1));
  BUF_X1 buf2 (.A(in2), .Z(n2));
  AND2_X1 and1 (.A1(n1), .A2(n2), .ZN(n3));
  INV_X1 inv1 (.A(n3), .ZN(n4));
  OR2_X1 or1 (.A1(n4), .A2(assigned_net), .ZN(n5));
  DFF_X1 reg1 (.D(n3), .CK(clk), .Q(out1));
  DFF_X1 reg2 (.D(n5), .CK(clk), .Q(out2));
  DFF_X1 reg3 (.D(assigned_net), .CK(clk), .Q(out3));
endmodule
