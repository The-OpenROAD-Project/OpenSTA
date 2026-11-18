// Four registers capture in through a buffer each and drive an output
// through a buffer each.
module sdc_delete_exception_insts (clk, in, out1, out2, out3, out4);
  input clk, in;
  output out1, out2, out3, out4;
  wire d1, d2, d3, d4, q1, q2, q3, q4;

  BUFx2_ASAP7_75t_R ib1 (.A(in), .Y(d1));
  BUFx2_ASAP7_75t_R ib2 (.A(in), .Y(d2));
  BUFx2_ASAP7_75t_R ib3 (.A(in), .Y(d3));
  BUFx2_ASAP7_75t_R ib4 (.A(in), .Y(d4));
  DFFHQx4_ASAP7_75t_R r1 (.D(d1), .CLK(clk), .Q(q1));
  DFFHQx4_ASAP7_75t_R r2 (.D(d2), .CLK(clk), .Q(q2));
  DFFHQx4_ASAP7_75t_R r3 (.D(d3), .CLK(clk), .Q(q3));
  DFFHQx4_ASAP7_75t_R r4 (.D(d4), .CLK(clk), .Q(q4));
  BUFx2_ASAP7_75t_R ob1 (.A(q1), .Y(out1));
  BUFx2_ASAP7_75t_R ob2 (.A(q2), .Y(out2));
  BUFx2_ASAP7_75t_R ob3 (.A(q3), .Y(out3));
  BUFx2_ASAP7_75t_R ob4 (.A(q4), .Y(out4));
endmodule
