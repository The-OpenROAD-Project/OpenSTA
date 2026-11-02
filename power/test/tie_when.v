module top (clk, d, set_n, rst, q1, q2, q3, q4);
  input clk, d, set_n, rst;
  output q1, q2, q3, q4;
  wire one, zero;

  TIEHI tie (.Y(one));
  DFFS ff_tied (.CLK(clk), .D(d), .SETN(one), .QN(q1));     // SETN tied high by a tie cell
  DFFS ff_input (.CLK(clk), .D(d), .SETN(set_n), .QN(q2));  // SETN from a port held high
  TIELO tie_lo (.Y(zero));
  DFFR ff_tied_lo (.CLK(clk), .D(d), .RST(zero), .QN(q3));  // RST tied low by a tie cell
  DFFR ff_input_lo (.CLK(clk), .D(d), .RST(rst), .QN(q4));  // RST from a port held low
endmodule
