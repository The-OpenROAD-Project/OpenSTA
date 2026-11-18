// Two flops behind a clock buffer and a hard macro on the clock port
// itself. The flops' clock pins are the earliest clock sinks; the
// macro's registers, its pin arrival plus its own clock tree, the latest.
module clktree_macro_top (clk, in1, out1);
  input clk, in1;
  output out1;
  wire clk_buf, n1, n2;

  BUF_X1 ckbuf (.A(clk), .Z(clk_buf));
  DFF_X1 ff1 (.D(in1), .CK(clk_buf), .Q(n1));
  clktree_macro mac (.CLK(clk), .D(n1), .Q(n2));
  DFF_X1 ff2 (.D(n2), .CK(clk_buf), .Q(out1));
endmodule
