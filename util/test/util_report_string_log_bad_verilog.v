module bad_design (input a, output b);
  NONEXISTENT_CELL u1 (.A(a), .Z(b));
endmodule
