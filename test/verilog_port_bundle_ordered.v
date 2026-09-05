// An explicit named header port (IEEE 1364-2005 A.1.4,
// port ::= . port_identifier ( [ port_expression ] )) reached by an ordered
// connection list.  The header entry .px(m) is modelled as a bundle port px
// plus a member port m; a bundle port gets no pin index, and the ordered
// binding walks the port list, so slot 1 lands on the bundle.
module top (a, y);
 input a;
 output y;
 sub u (a, y);
endmodule

module sub (.px(m), .py(r));
 input m;
 output r;
 INV_X1 g (.A(m), .ZN(r));
endmodule
