module slash_port_test (
  \level1/level2/level3 ,
  \a/b ,
  \level1/Y ,
  out
);
  input \level1/level2/level3 ;
  input \a/b ;
  input \level1/Y ;
  output out;

  wire buf_out, buf_out2, and_out;

  // Instance named "level1" — collides with the prefix of port \level1/level2/level3
  BUFx2_ASAP7_75t_R level1 (.A(\level1/level2/level3 ), .Y(buf_out));
  // Instance named "a" — collides with the prefix of port \a/b
  BUFx2_ASAP7_75t_R a (.A(\a/b ), .Y(buf_out2));
  AND2x2_ASAP7_75t_R u3 (.A(buf_out), .B(buf_out2), .Y(and_out));
  BUFx2_ASAP7_75t_R u4 (.A(and_out), .Y(out));
endmodule
