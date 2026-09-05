// An unsized literal in a port connection.  IEEE 1364-2005 3.5.1 makes the
// size of a number optional, so 'b0 is a legal number.  The size field is the
// empty string, which reaches the unguarded conversion in
// VerilogNetConstant::parseConstant.
module top (a, y);
  input a;
  output y;
  OR2_X1 g (.A1(a), .A2('b0), .ZN(y));
endmodule
