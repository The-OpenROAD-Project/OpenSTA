module verilog_bus_test (clk, data_in, data_out, sel, enable);
  input clk;
  input [3:0] data_in;
  output [3:0] data_out;
  input sel;
  input enable;
  wire [3:0] n1;
  wire [3:0] n2;
  wire n3;

  BUF_X1 buf0 (.A(data_in[0]), .Z(n1[0]));
  BUF_X1 buf1 (.A(data_in[1]), .Z(n1[1]));
  BUF_X1 buf2 (.A(data_in[2]), .Z(n1[2]));
  BUF_X1 buf3 (.A(data_in[3]), .Z(n1[3]));

  AND2_X1 and0 (.A1(n1[0]), .A2(enable), .ZN(n2[0]));
  AND2_X1 and1 (.A1(n1[1]), .A2(enable), .ZN(n2[1]));
  AND2_X1 and2 (.A1(n1[2]), .A2(enable), .ZN(n2[2]));
  AND2_X1 and3 (.A1(n1[3]), .A2(enable), .ZN(n2[3]));

  DFF_X1 reg0 (.D(n2[0]), .CK(clk), .Q(data_out[0]));
  DFF_X1 reg1 (.D(n2[1]), .CK(clk), .Q(data_out[1]));
  DFF_X1 reg2 (.D(n2[2]), .CK(clk), .Q(data_out[2]));
  DFF_X1 reg3 (.D(n2[3]), .CK(clk), .Q(data_out[3]));
endmodule
