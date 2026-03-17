module top (in, clk1, clk2, out, out2);
   input in, clk1, clk2;
   output out, out2;
   block1 b1 (.in(in), .clk(clk1), .out(b1out), .out2(out2));
   block2 b2 (.in(b1out), .clk(clk2), .out(out));
endmodule // top

module block1 (in, clk, out, out2);
   input in, clk;
   output out, out2;
   BUFx2_ASAP7_75t_R u1 (.A(in), .Y(u1out));
   DFFHQx4_ASAP7_75t_R  r1 (.D(u1out), .CLK(clk), .Q(r1q));
   BUFx2_ASAP7_75t_R u2 (.A(r1q), .Y(out));
   BUFx2_ASAP7_75t_R u3 (.A(out), .Y(out2));
endmodule // block1

module block2 (in, clk, out, out2);
   input in, clk;
   output out, out2;
   BUFx2_ASAP7_75t_R u1 (.A(in), .Y(u1out));
   DFFHQx4_ASAP7_75t_R  r1 (.D(u1out), .CLK(clk), .Q(r1q));
   BUFx2_ASAP7_75t_R u2 (.A(r1q), .Y(out));
   BUFx2_ASAP7_75t_R u3 (.A(out), .Y(out2));
endmodule // block2
