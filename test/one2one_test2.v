// Liberty file test: one-to-one mapping with mismatched bit widths
// Should generate warning but still create timing arcs between bits with same index
module one2one_test2 (
  input wire [19:0] a,
  output wire [31:0] y
);

  or_20_to_32 partial_wide_or_cell (
    .A(a),
    .B(20'b0),
    .Y(y)
  );

endmodule