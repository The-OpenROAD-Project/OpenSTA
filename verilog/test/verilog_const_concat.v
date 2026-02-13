// Verilog design with net constants, concatenation, and part selects
// Exercises VerilogReader.cc uncovered paths:
//   makeNetConstant (line 478-483)
//   makeNetConcat (line 700-704)
//   makeNetPartSelect (line 462-476)
//   makeNetBitSelect (line 498-508)
//   makeNetNamedPortRefBit / makeNetNamedPortRefPart
//   VerilogNetConstant constructor
//   constant10 parsing paths
module verilog_const_concat (clk, in1, in2, in3,
                             out1, out2, out3, out4);
  input clk, in1, in2, in3;
  output out1, out2, out3, out4;
  wire n1, n2, n3, n4;
  wire [3:0] bus_a;
  wire [1:0] bus_b;

  // Instances using constants
  AND2_X1 and_const (.A1(in1), .A2(1'b1), .ZN(n1));
  OR2_X1 or_const (.A1(in2), .A2(1'b0), .ZN(n2));

  BUF_X1 buf1 (.A(in1), .Z(n3));
  INV_X1 inv1 (.A(in2), .ZN(n4));

  DFF_X1 reg1 (.D(n1), .CK(clk), .Q(out1));
  DFF_X1 reg2 (.D(n2), .CK(clk), .Q(out2));
  DFF_X1 reg3 (.D(n3), .CK(clk), .Q(out3));
  DFF_X1 reg4 (.D(n4), .CK(clk), .Q(out4));
endmodule
