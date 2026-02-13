// Verilog design exercising preprocessor-like macro lines,
// parameter declarations, parameter overrides (#(...) syntax),
// defparam statements, and parameter expressions.
// The lexer skips lines starting with ` (ifdef/endif/define/etc.)
// Targets: VerilogLex.ll macro line skip, VerilogParse.yy parameter,
//   defparam, parameter_values (#(...)) in instance declarations.

`define ENABLE_BUF 1
`ifdef ENABLE_BUF
`endif

`ifndef DISABLE_INV
`define WIDTH 4
`else
`endif

module param_sub (input A, input B, output Y);
  parameter DELAY = 1;
  parameter MODE = "fast";
  AND2_X1 g1 (.A1(A), .A2(B), .ZN(Y));
endmodule

module verilog_preproc_param (clk, d1, d2, d3, d4,
                              q1, q2, q3, q4);
  input clk;
  input d1, d2, d3, d4;
  output q1, q2, q3, q4;

  parameter TOP_WIDTH = 8;
  parameter [7:0] TOP_MASK = 8'hFF;
  parameter TOP_STR = "default";
  parameter TOP_EXPR = 2 * 3 + 1;

  wire n1, n2, n3, n4, n5, n6;

`ifdef SOME_FEATURE
  // This block is skipped by the lexer
`else
  // This block is also skipped
`endif

  // Instance with parameter override using #(...)
  param_sub #(2) ps1 (.A(d1), .B(d2), .Y(n1));
  param_sub #(3) ps2 (.A(d3), .B(d4), .Y(n2));

  // Instance with parameter expression override
  param_sub #(1 + 1) ps3 (.A(d1), .B(d3), .Y(n3));

  // defparam statements
  defparam ps1.DELAY = 5;
  defparam ps2.DELAY = 10, ps2.MODE = "turbo";

  BUF_X1 buf1 (.A(n1), .Z(n4));
  INV_X1 inv1 (.A(n2), .ZN(n5));
  OR2_X1 or1 (.A1(n3), .A2(n4), .ZN(n6));

  DFF_X1 reg1 (.D(n4), .CK(clk), .Q(q1));
  DFF_X1 reg2 (.D(n5), .CK(clk), .Q(q2));
  DFF_X1 reg3 (.D(n6), .CK(clk), .Q(q3));
  DFF_X1 reg4 (.D(n1), .CK(clk), .Q(q4));
endmodule
