// Verilog with assign statement
// This triggers VerilogStmt::isAssign, VerilogAssign
module assign_mod (input in1, input in2, output out1);
  wire a, b;
  assign a = in1;
  assign b = in2;
  AND2x2_ASAP7_75t_R u1 (.A(a), .B(b), .Y(out1));
endmodule
