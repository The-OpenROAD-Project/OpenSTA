module sdc_move_constraints (clk, in1, in2, out1);
  input clk, in1, in2;
  output out1;
  wire n1, n2;

  BUFx2_ASAP7_75t_R buf1 (.A(in1), .Y(n1));
  DFFHQx4_ASAP7_75t_R reg1 (.D(n1), .CLK(clk), .Q(n2));
  BUFx2_ASAP7_75t_R buf2 (.A(n2), .Y(out1));
endmodule
