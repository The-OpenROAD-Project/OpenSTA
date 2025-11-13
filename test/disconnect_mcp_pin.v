module top (clk, clkout, data_in, data_out);
  input clk;
  output clkout;
  input [1:0] data_in;
  output [1:0] data_out;

  // Anchor buffers on the source-synchronous interface IOs
  BUFx2_ASAP7_75t_R clkbuf0 (.A(clk), .Y(clkout));
  BUFx2_ASAP7_75t_R u0 (.A(data_in[0]), .Y(data_out[0]));
  BUFx2_ASAP7_75t_R u1 (.A(data_in[1]), .Y(data_out[1]));
endmodule // top
