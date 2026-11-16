module generated_clock_edges_redefine (
  input wire CLK_IN,
  output wire CLK_MID,
  output wire CLK_OUT
);

  CLK_EDGE_GEN u_gen1 (
    .CLK_IN(CLK_IN),
    .CLK_OUT(CLK_MID)
  );

  CLK_EDGE_GEN u_gen2 (
    .CLK_IN(CLK_MID),
    .CLK_OUT(CLK_OUT)
  );

endmodule
