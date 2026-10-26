module child (input wire signed [1:0] a, output reg signed y);
  BUFx2_ASAP7_75t_R g (.A(a[1]), .Y(y));
endmodule

module top (a, y, z);
  input signed [1:0] a;
  wire signed [1:0] a;
  output signed y;
  wire signed y;
  output z;
  wire z;
  wire signed [1:0] t;
  child u (.a(a), .y(y));
  BUFx2_ASAP7_75t_R g (.A(a[0]), .Y(t[0]));
  BUFx2_ASAP7_75t_R h (.A(t[0]), .Y(z));
endmodule
