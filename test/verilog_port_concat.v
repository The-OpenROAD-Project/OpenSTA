// A concatenation port_expression in the top module header, which IEEE
// 1364-2005 A.1.4 allows (port_expression ::= port_reference |
// { port_reference {, port_reference} }; the LRM's own example is
// module complex_ports ({c,d}, .e(f))).  No Port is made for the unnamed
// slot, so the port lookup in VerilogReader::linkNetwork finds nothing.
module top ({a}, z);
 input a;
 output z;
 INV_X1 g (.A(a), .ZN(z));
endmodule
