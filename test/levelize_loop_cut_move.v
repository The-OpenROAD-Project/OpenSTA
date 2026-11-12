module top (in0, in1, n6);
  input in0, in1;
  output n6;
  wire n2, n4;
  AND2x2_ASAP7_75t_R g2 (.A(in1), .B(n6), .Y(n2));
  AND2x2_ASAP7_75t_R g4 (.A(n4), .Y(n4));
  AND2x2_ASAP7_75t_R g6 (.A(n2), .B(in0), .Y(n6));
endmodule
