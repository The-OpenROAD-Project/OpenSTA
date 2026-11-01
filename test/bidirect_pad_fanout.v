module top (in1, in2, tie_out, tie_q0, tie_q1, sig_q0, sig_q1, ff_q1);
  input in1, in2;
  output tie_out, tie_q0, tie_q1, sig_q0, sig_q1, ff_q1;
  wire sig, clk_pad, ff_q0;

  // Constant net: tie cell + output port + passive pads + loads.
  TIELO tie (.L(tie_out));
  PAD_INOUT tie_pad0 (.PAD(tie_out));
  PAD_INOUT tie_pad1 (.PAD(tie_out));
  PAD_INOUT tie_pad2 (.PAD(tie_out));
  BUFx2_ASAP7_75t_R tie_load0 (.A(tie_out), .Y(tie_q0));
  BUFx2_ASAP7_75t_R tie_load1 (.A(tie_out), .Y(tie_q1));

  // Signal net: real driver + passive pads + loads.
  BUFx2_ASAP7_75t_R sig_drvr (.A(in1), .Y(sig));
  PAD_INOUT sig_pad0 (.PAD(sig));
  PAD_INOUT sig_pad1 (.PAD(sig));
  PAD_INOUT sig_pad2 (.PAD(sig));
  BUFx2_ASAP7_75t_R sig_load0 (.A(sig), .Y(sig_q0));
  BUFx2_ASAP7_75t_R sig_load1 (.A(sig), .Y(sig_q1));

  // Pad only net: clock defined on a pad.
  PAD_INOUT clk_pad0 (.PAD(clk_pad));
  PAD_INOUT clk_pad1 (.PAD(clk_pad));
  PAD_INOUT clk_pad2 (.PAD(clk_pad));
  DFFHQx4_ASAP7_75t_R ff0 (.D(in2), .CLK(clk_pad), .Q(ff_q0));
  DFFHQx4_ASAP7_75t_R ff1 (.D(ff_q0), .CLK(clk_pad), .Q(ff_q1));
endmodule
