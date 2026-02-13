// Verilog with constant connections
// This triggers VerilogNetConstant
module const_mod (input in1, output out1);
  wire w1;
  AND2x2_ASAP7_75t_R u1 (.A(in1), .B(1'b1), .Y(w1));
  BUFx2_ASAP7_75t_R u2 (.A(w1), .Y(out1));
endmodule
