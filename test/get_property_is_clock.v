module top (in, clk, out);
   input in, clk;
   output out;
   block1 b1 (.in(in), .clk(clk), .out(b1out));
   macro1 bb1 (.A(b1out), .Y(out));
endmodule

module block1 (in, clk, out);
   input in, clk;
   output out;
   DFFHQx4_ASAP7_75t_R r1 (.D(in), .CLK(clk), .Q(out));
endmodule
