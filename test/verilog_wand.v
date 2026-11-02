module top (a, b, y);
  input a;
  input b;
  output y;
  wand w;
  assign w = a;
  assign w = b;
  BUFx2_ASAP7_75t_R g (.A(w), .Y(y));
endmodule
