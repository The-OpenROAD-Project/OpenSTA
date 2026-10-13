module top (a, c, z);
  input a, c;
  output z;
  wire cn, n1;

  // set_case_analysis 1 on port c makes net cn constant.
  BUF_X1  u0 (.A(c),  .Z(cn));
  // 1 is non-controlling for AND2, so n1 is NOT constant.
  AND2_X1 u1 (.A1(a), .A2(cn), .ZN(n1));
  // Consumes n1's slew, so slew corruption becomes a delay change.
  BUF_X1  u2 (.A(n1), .Z(z));
endmodule
