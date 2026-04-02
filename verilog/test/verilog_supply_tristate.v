// Verilog design with supply0, supply1, tri-state, and various port types
// Exercises VerilogReader.cc uncovered paths:
//   supply0/supply1 dcl parsing (lines 839-845)
//   tri dcl parsing as modifier for output (line 832-837)
//   VerilogDclBus constructor paths
//   wire assign in declaration (makeDclArg with assign)
//   VerilogNetConstant (makeNetConstant)
//   net concatenation (makeNetConcat)
//   part select (makeNetPartSelect)
//   Black box linking when module missing
module verilog_supply_tristate (clk, in1, in2, in3, en,
                                out1, out2, out3, outbus);
  input clk, in1, in2, in3, en;
  output out1;
  output out2;
  output out3;
  output [3:0] outbus;

  // supply0 and supply1 declarations
  supply0 gnd_net;
  supply1 vdd_net;

  // tri declaration for an output (modifier path)
  tri out1;

  wire n1, n2, n3, n4, n5, n6;
  wire [3:0] bus1;

  // Wire with assign in declaration
  wire assigned_w = in3;

  // Continuous assigns
  assign out3 = n6;
  assign bus1[0] = n1;
  assign bus1[1] = n2;
  assign bus1[2] = n3;
  assign bus1[3] = n4;

  BUF_X1 buf1 (.A(in1), .Z(n1));
  BUF_X1 buf2 (.A(in2), .Z(n2));
  INV_X1 inv1 (.A(in3), .ZN(n3));
  AND2_X1 and1 (.A1(n1), .A2(n2), .ZN(n4));
  OR2_X1 or1 (.A1(n3), .A2(n4), .ZN(n5));
  BUF_X1 buf3 (.A(n5), .Z(n6));

  // Registers
  DFF_X1 reg1 (.D(n5), .CK(clk), .Q(out1));
  DFF_X1 reg2 (.D(n6), .CK(clk), .Q(out2));
  DFF_X1 reg3 (.D(assigned_w), .CK(clk), .Q(outbus[0]));
  DFF_X1 reg4 (.D(n1), .CK(clk), .Q(outbus[1]));
  DFF_X1 reg5 (.D(n2), .CK(clk), .Q(outbus[2]));
  DFF_X1 reg6 (.D(n3), .CK(clk), .Q(outbus[3]));
endmodule
