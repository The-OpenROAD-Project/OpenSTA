module test (
  input  clk1,
  input  clk2,
  input  clk3,
  input  test_en,
  output out
);

  wire sel;  
  DFFHQx4_ASAP7_75t_R r1 (
    .CLK(clk1),
    .D(test_en),
    .Q(sel)
  );

  CKMUX ckmux (
    .CLK1  (clk2),
    .CLK2  (clk3),
    .CLKSEL(sel),
    .CLKOUT(out)
  );

endmodule
