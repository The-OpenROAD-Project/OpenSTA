module power_pin_delete (clk, in1, out1);
  input clk, in1;
  output out1;
  wire n1;

  BUFx2_ASAP7_75t_R buf1 (.A(in1), .Y(n1));
  DFFHQx4_ASAP7_75t_R reg1 (.D(n1), .CLK(clk), .Q(out1));
endmodule
