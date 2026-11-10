module generated_clock_timing (
    input wire CLK_IN,
    input wire data_in,
    output wire out
);

  wire CLK_OUT;
  CLK_GEN clk_gen (
    .CLK_IN(CLK_IN),
    .CLK_OUT(CLK_OUT)
  );
  
  DFFHQNx1_ASAP7_75t_R ff_inst (
    .D(data_in),
    .CLK(CLK_OUT),
    .QN(out)
  );

endmodule
