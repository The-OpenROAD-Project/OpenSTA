// An explicit named header port (IEEE 1364-2005 A.1.4) reached by a *named*
// connection links, because the lookup is by name and finds the member port.
// The bundle port px is still in the cell port list with no pin index, so
// looking a pin up by that name must not index pins_[-1].
module top (a, y);
 input a;
 output y;
 sub u (.px(a), .py(y));
endmodule

module sub (.px(m), .py(r));
 input m;
 output r;
 INV_X1 g (.A(m), .ZN(r));
endmodule
