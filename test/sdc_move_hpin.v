module hsub (in, out);
  input in;
  output out;
  BUFx2_ASAP7_75t_R u1 (.A(in), .Y(out));
endmodule

module sdc_move_hpin (clk, in1, in2, out1, out2);
  input clk, in1, in2;
  output out1, out2;
  wire n1, n2, n3, n4;
  DFFHQx4_ASAP7_75t_R r1 (.D(in1), .CLK(clk), .Q(n1));
  hsub h1 (.in(n1), .out(n2));
  DFFHQx4_ASAP7_75t_R r2 (.D(n2), .CLK(clk), .Q(out1));
  DFFHQx4_ASAP7_75t_R r3 (.D(in2), .CLK(clk), .Q(n3));
  hsub h2 (.in(n3), .out(n4));
  DFFHQx4_ASAP7_75t_R r4 (.D(n4), .CLK(clk), .Q(out2));
endmodule
