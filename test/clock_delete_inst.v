module clkgen (clk_in, clk_out);
  input clk_in;
  output clk_out;

  BUFx2_ASAP7_75t_R b1 (.A(clk_in), .Y(clk_out));
endmodule // clkgen

module top (in1, in2, clk1, clk2, out1, out2);
  input in1, in2, clk1, clk2;
  output out1, out2;
  wire gclk1, gclk2;

  clkgen u1 (.clk_in(clk1), .clk_out(gclk1));
  BUFx2_ASAP7_75t_R b2 (.A(clk2), .Y(gclk2));
  DFFHQx4_ASAP7_75t_R r1 (.D(in1), .CLK(gclk1), .Q(out1));
  DFFHQx4_ASAP7_75t_R r2 (.D(in2), .CLK(gclk2), .Q(out2));
endmodule // top
