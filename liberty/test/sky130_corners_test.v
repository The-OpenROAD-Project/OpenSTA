// Small sky130hd test design for multi-corner analysis.
module sky130_corners_test (clk, in1, in2, out1, out2);
  input clk, in1, in2;
  output out1, out2;
  wire n1, n2, n3, n4, n5, clk_buf;

  sky130_fd_sc_hd__clkbuf_1 ckbuf (.A(clk), .X(clk_buf));

  sky130_fd_sc_hd__and2_1 and1 (.A(in1), .B(in2), .X(n1));
  sky130_fd_sc_hd__or2_1  or1  (.A(n1),  .B(in1), .X(n2));
  sky130_fd_sc_hd__buf_1  buf1 (.A(n2),  .X(n3));
  sky130_fd_sc_hd__inv_1  inv1 (.A(n3),  .Y(n4));

  sky130_fd_sc_hd__dfxtp_1 reg1 (.D(n4), .CLK(clk_buf), .Q(n5));
  sky130_fd_sc_hd__buf_1   buf2 (.A(n5), .X(out1));

  sky130_fd_sc_hd__dfxtp_1 reg2 (.D(n5), .CLK(clk_buf), .Q(out2));
endmodule
