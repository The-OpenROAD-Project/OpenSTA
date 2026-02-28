// Comprehensive Verilog test for VerilogReader coverage
// Exercises: part-select in port connections, constants, assigns

// Sub-module with 4-bit bus input and output
module sub4 (input [3:0] d, output [3:0] q);
  BUF_X1 b0 (.A(d[0]), .Z(q[0]));
  BUF_X1 b1 (.A(d[1]), .Z(q[1]));
  BUF_X1 b2 (.A(d[2]), .Z(q[2]));
  BUF_X1 b3 (.A(d[3]), .Z(q[3]));
endmodule

module verilog_coverage_test (
  input clk,
  input [7:0] data_in,
  input [3:0] ctrl,
  output [7:0] data_out,
  output valid
);

  wire [7:0] w1;
  wire [3:0] lo_result;
  wire [3:0] hi_result;
  wire n1;

  // Buffer data_in to w1
  BUF_X1 b0 (.A(data_in[0]), .Z(w1[0]));
  BUF_X1 b1 (.A(data_in[1]), .Z(w1[1]));
  BUF_X1 b2 (.A(data_in[2]), .Z(w1[2]));
  BUF_X1 b3 (.A(data_in[3]), .Z(w1[3]));
  BUF_X1 b4 (.A(data_in[4]), .Z(w1[4]));
  BUF_X1 b5 (.A(data_in[5]), .Z(w1[5]));
  BUF_X1 b6 (.A(data_in[6]), .Z(w1[6]));
  BUF_X1 b7 (.A(data_in[7]), .Z(w1[7]));

  // Part-select in port connection: triggers makeNetPartSelect
  sub4 lo_proc (
    .d(w1[3:0]),
    .q(lo_result)
  );

  // Concatenation in port connection
  sub4 hi_proc (
    .d({w1[7], w1[6], w1[5], w1[4]}),
    .q(hi_result)
  );

  // Assign statements: triggers VerilogAssign constructor
  assign data_out[0] = lo_result[0];
  assign data_out[1] = lo_result[1];
  assign data_out[2] = lo_result[2];
  assign data_out[3] = lo_result[3];
  assign data_out[4] = hi_result[0];
  assign data_out[5] = hi_result[1];
  assign data_out[6] = hi_result[2];
  assign data_out[7] = hi_result[3];

  // Constants in port connections: triggers makeNetConstant, parseConstant10
  AND2_X1 and_const (.A1(lo_result[0]), .A2(1'b1), .ZN(n1));

  // Decimal constant: triggers parseConstant10
  // Note: 1'd1 is a 1-bit decimal constant
  AND2_X1 and_dec (.A1(lo_result[1]), .A2(1'd1), .ZN(data_out[7]));

  // Valid output
  OR2_X1 or_valid (.A1(n1), .A2(hi_result[0]), .ZN(valid));

endmodule
