// Verilog with positional (ordered) instance pin connections
// This triggers VerilogReader::makeOrderedInstPins
module pos_top (input in1, input in2, input clk, output out);
  wire w1, w2;
  // Positional connections: A, Y (not .A(in1), .Y(w1))
  BUFx2_ASAP7_75t_R u1 (in1, w1);
  AND2x2_ASAP7_75t_R u2 (w1, in2, w2);
  BUFx2_ASAP7_75t_R u3 (w2, out);
endmodule
