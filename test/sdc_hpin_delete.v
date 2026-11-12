module sdc_hpin_sub (in, out);
  input in;
  output out;

  BUFx2_ASAP7_75t_R u1 (.A(in), .Y(out));
endmodule

module sdc_hpin_delete (clk, in1, in2, out1, out2);
  input clk, in1, in2;
  output out1, out2;
  wire clk1, n1;

  sdc_hpin_sub sub1 (.in(clk), .out(clk1));
  DFFHQx4_ASAP7_75t_R reg1 (.D(in1), .CLK(clk1), .Q(out1));
  BUFx2_ASAP7_75t_R buf1 (.A(in2), .Y(n1));
  AND2x2_ASAP7_75t_R and1 (.A(n1), .B(in1), .Y(out2));
endmodule
