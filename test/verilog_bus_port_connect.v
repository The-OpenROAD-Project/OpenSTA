// A hierarchical instance with a bus port.  A bus port has members and no pin
// index, so connecting a net to the bus port itself has no pin to make.
module top (a, y);
  input [1:0] a;
  output y;
  sub u (.b(a), .r(y));
endmodule

module sub (b, r);
  input [1:0] b;
  output r;
  INV_X1 g (.A(b[0]), .ZN(r));
endmodule
