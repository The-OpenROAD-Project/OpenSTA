module power_test1 (clk, in1, out1);
  input clk, in1;
  output out1;
  wire n1;

  sky130_fd_sc_hd__buf_1 buf1 (.A(in1), .X(n1));
  sky130_fd_sc_hd__dfxtp_1 reg1 (.D(n1), .CLK(clk), .Q(out1));
endmodule
