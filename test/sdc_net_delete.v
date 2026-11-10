module sdc_net_delete (clk, in1, out1, out2);
  input clk, in1;
  output out1, out2;
  wire n1, n2, n3, n4;

  DFFHQx4_ASAP7_75t_R reg1 (.D(in1), .CLK(clk), .Q(n1));
  BUFx2_ASAP7_75t_R buf1 (.A(n1), .Y(n2));
  BUFx2_ASAP7_75t_R buf2 (.A(n2), .Y(n3));
  DFFHQx4_ASAP7_75t_R reg2 (.D(n3), .CLK(clk), .Q(out1));
  BUFx2_ASAP7_75t_R buf3 (.A(n1), .Y(n4));
  BUFx2_ASAP7_75t_R buf4 (.A(n4), .Y(out2));
endmodule
