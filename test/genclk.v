module generated_clock (

  // Test basic test cases
  input wire CLK_IN_1,
  output wire slow_clk_int,
  output wire fast_clk_int,
  output wire slow_clk_out2,
  output wire fast_clk_out2,

  // Test liberty-defined nested clock output
  output wire slow_clk_out3,
  output wire fast_clk_out3,

  // Test edges/shifts
  input wire CLK_IN_2,
  output wire CLK_OUT_2,
  
  // Test TCL edges/shifts command
  output wire CLK_OUT_3
);

  // Give one more level of hierarchy to test names
  second_hierarchy u_second_hierarchy (
    .clk_in(CLK_IN_1),
    .slow_clk_out(slow_clk_int),
    .fast_clk_out(fast_clk_int),
    .slow_clk_out2(slow_clk_out2),
    .fast_clk_out2(fast_clk_out2),
    .slow_clk_out3(slow_clk_out3),
    .fast_clk_out3(fast_clk_out3)
  );

  CLK_EDGE_SHIFT clk_edge_shift (
    .CLK_IN(CLK_IN_2),
    .CLK_OUT(CLK_OUT_2)
  );

endmodule

module second_hierarchy (
  input wire clk_in,
  output wire slow_clk_out,
  output wire fast_clk_out,
  output wire slow_clk_out2,
  output wire fast_clk_out2,
  output wire slow_clk_out3,
  output wire fast_clk_out3
);

  CLK_GEN clk_gen (
    .CLK_IN(clk_in),
    .CLK_OUT_DIV(slow_clk_out),
    .CLK_OUT_MUL(fast_clk_out)
  );

  CLK_GEN clk_gen2 (
    .CLK_IN(slow_clk_out),
    .CLK_OUT_DIV(slow_clk_out2),
    .CLK_OUT_MUL(fast_clk_out2)
  );

  CLK_GEN clk_gen3 (
    .CLK_IN(slow_clk_out2),
    .CLK_OUT_DIV(slow_clk_out3),
    .CLK_OUT_MUL(fast_clk_out3)
  );

endmodule
