module top (input in, input clk, output out);

  wire w1, w2, w3, w4;

  DFFHQx4_ASAP7_75t_R r1 (.D(in), .CLK(clk), .Q(w1));

  BUFx2_ASAP7_75t_R u2 (.A(w1), .Y(w2));
  BUFx2_ASAP7_75t_R u3 (.A(w2), .Y(w3));
  BUFx2_ASAP7_75t_R u4 (.A(w3), .Y(w4));
  
  DFFHQx4_ASAP7_75t_R r2 (.D(w2), .CLK(clk), .Q(out));
  DFFHQx4_ASAP7_75t_R r3 (.D(w3), .CLK(clk), .Q(out));
  DFFHQx4_ASAP7_75t_R r4 (.D(w4), .CLK(clk), .Q(out));

endmodule
