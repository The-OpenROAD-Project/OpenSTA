// Verilog design exercising error-adjacent paths in VerilogReader.cc:
// - Bus expressions with partial bit ranges
// - Assign statements with concatenation
// - Module instances with varied port connection styles
// - Named and positional port connections
// - Escaped identifiers
module sub_mod (input A, input B, output Y);
  wire n1;
  AND2_X1 and_inner (.A1(A), .A2(B), .ZN(n1));
  BUF_X1 buf_inner (.A(n1), .Z(Y));
endmodule

module verilog_error_paths (clk, din, dout, sel, en,
                            bus_in, bus_out, flag);
  input clk;
  input [3:0] din;
  output [3:0] dout;
  input sel, en;
  input [7:0] bus_in;
  output [7:0] bus_out;
  output flag;

  wire [3:0] stage1;
  wire [3:0] stage2;
  wire [7:0] wide1;
  wire [7:0] wide2;
  wire w1, w2, w3;

  // Assign with concatenation of bus bits
  assign {stage1[3], stage1[2], stage1[1], stage1[0]} = din;

  // Simple assigns
  assign w1 = sel;
  assign flag = w3;

  // Buffer chain on low bits
  BUF_X1 buf0 (.A(stage1[0]), .Z(stage2[0]));
  BUF_X1 buf1 (.A(stage1[1]), .Z(stage2[1]));
  BUF_X2 buf2 (.A(stage1[2]), .Z(stage2[2]));
  BUF_X2 buf3 (.A(stage1[3]), .Z(stage2[3]));

  // AND gate with enable
  AND2_X1 and_en (.A1(stage2[0]), .A2(en), .ZN(w2));

  // OR gate
  OR2_X1 or_sel (.A1(w2), .A2(w1), .ZN(w3));

  // Sub-module instantiation (hierarchical)
  sub_mod sub1 (.A(stage2[1]), .B(stage2[2]), .Y(wide1[0]));
  sub_mod sub2 (.A(stage2[2]), .B(stage2[3]), .Y(wide1[1]));

  // Direct wiring for remaining bus bits
  BUF_X1 buf_w2 (.A(stage2[0]), .Z(wide1[2]));
  BUF_X1 buf_w3 (.A(stage2[1]), .Z(wide1[3]));
  BUF_X1 buf_w4 (.A(stage2[2]), .Z(wide1[4]));
  BUF_X1 buf_w5 (.A(stage2[3]), .Z(wide1[5]));
  INV_X1 inv_w6 (.A(stage2[0]), .ZN(wide1[6]));
  INV_X1 inv_w7 (.A(stage2[1]), .ZN(wide1[7]));

  // Wide bus through more gates
  AND2_X1 and_b0 (.A1(bus_in[0]), .A2(wide1[0]), .ZN(wide2[0]));
  AND2_X1 and_b1 (.A1(bus_in[1]), .A2(wide1[1]), .ZN(wide2[1]));
  AND2_X1 and_b2 (.A1(bus_in[2]), .A2(wide1[2]), .ZN(wide2[2]));
  AND2_X1 and_b3 (.A1(bus_in[3]), .A2(wide1[3]), .ZN(wide2[3]));
  OR2_X1 or_b4 (.A1(bus_in[4]), .A2(wide1[4]), .ZN(wide2[4]));
  OR2_X1 or_b5 (.A1(bus_in[5]), .A2(wide1[5]), .ZN(wide2[5]));
  OR2_X1 or_b6 (.A1(bus_in[6]), .A2(wide1[6]), .ZN(wide2[6]));
  OR2_X1 or_b7 (.A1(bus_in[7]), .A2(wide1[7]), .ZN(wide2[7]));

  // Output registers
  DFF_X1 reg0 (.D(stage2[0]), .CK(clk), .Q(dout[0]));
  DFF_X1 reg1 (.D(stage2[1]), .CK(clk), .Q(dout[1]));
  DFF_X1 reg2 (.D(stage2[2]), .CK(clk), .Q(dout[2]));
  DFF_X1 reg3 (.D(stage2[3]), .CK(clk), .Q(dout[3]));

  DFF_X1 reg_b0 (.D(wide2[0]), .CK(clk), .Q(bus_out[0]));
  DFF_X1 reg_b1 (.D(wide2[1]), .CK(clk), .Q(bus_out[1]));
  DFF_X1 reg_b2 (.D(wide2[2]), .CK(clk), .Q(bus_out[2]));
  DFF_X1 reg_b3 (.D(wide2[3]), .CK(clk), .Q(bus_out[3]));
  DFF_X1 reg_b4 (.D(wide2[4]), .CK(clk), .Q(bus_out[4]));
  DFF_X1 reg_b5 (.D(wide2[5]), .CK(clk), .Q(bus_out[5]));
  DFF_X1 reg_b6 (.D(wide2[6]), .CK(clk), .Q(bus_out[6]));
  DFF_X1 reg_b7 (.D(wide2[7]), .CK(clk), .Q(bus_out[7]));
endmodule
