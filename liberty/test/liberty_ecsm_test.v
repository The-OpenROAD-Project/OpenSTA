module liberty_ecsm_test (
  input clk,
  input in1,
  output out1
);

  wire n1;

  ECSM1 u1 (
    .A(in1),
    .Z(n1)
  );

  ECSM2 u2 (
    .A(n1),
    .Z(out1)
  );

endmodule
