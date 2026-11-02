module top (a, b, y);
  input a;
  input b;
  output y;
  wor [1:0] w = a;
  assign w = a;
  assign w = b;
  BUFx2_ASAP7_75t_R g (.A(w), .Y(y));
endmodule
