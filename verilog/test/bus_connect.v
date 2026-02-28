// Verilog with bus port bit and part select connections
// This triggers makeNetNamedPortRefBit and makeNetNamedPortRefPart
module bus_mod (input [3:0] data_in, output [3:0] data_out);
  wire [3:0] w;
  // Named connections with bus references
  BUFx2_ASAP7_75t_R u0 (.A(data_in[0]), .Y(w[0]));
  BUFx2_ASAP7_75t_R u1 (.A(data_in[1]), .Y(w[1]));
  BUFx2_ASAP7_75t_R u2 (.A(data_in[2]), .Y(w[2]));
  BUFx2_ASAP7_75t_R u3 (.A(data_in[3]), .Y(w[3]));
  BUFx2_ASAP7_75t_R u4 (.A(w[0]), .Y(data_out[0]));
  BUFx2_ASAP7_75t_R u5 (.A(w[1]), .Y(data_out[1]));
  BUFx2_ASAP7_75t_R u6 (.A(w[2]), .Y(data_out[2]));
  BUFx2_ASAP7_75t_R u7 (.A(w[3]), .Y(data_out[3]));
endmodule
