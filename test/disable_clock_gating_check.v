module top (clk, en1, en2, d1, d2, q1, q2);
  input clk;
  input en1;
  input en2;
  input d1;
  input d2;
  output q1;
  output q2;
  wire gclk1;
  wire gclk2;

  AND2 cg1 (.A(clk), .B(en1), .Z(gclk1));
  AND2 cg2 (.A(clk), .B(en2), .Z(gclk2));
  DFF  ff1 (.CK(gclk1), .D(d1), .Q(q1));
  DFF  ff2 (.CK(gclk2), .D(d2), .Q(q2));
endmodule
