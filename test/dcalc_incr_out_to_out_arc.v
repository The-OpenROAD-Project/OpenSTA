module top (b, con);
  input b;
  output con;
  FAx1_ASAP7_75t_R fa (.B(b), .CON(con));
endmodule
