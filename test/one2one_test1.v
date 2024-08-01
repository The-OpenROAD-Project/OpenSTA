// Liberty file test: one-to-one mapping with mismatched bit widths
// Should generate warning but still create timing arcs between bits with same index
module one2one_test1 (
  input wire [31:0] a,
  output wire [19:0] y
);

  or_32_to_20 partial_wide_or_cell (
    .A(a),
    .B(32'b0),
    .Y(y)
  );

endmodule