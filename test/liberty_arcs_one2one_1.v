// Liberty file test: one-to-one mapping with mismatched bit widths
// Should generate warning but still create timing arcs between bits with same index
module liberty_arcs_one2one_1 (
  input wire [7:0] a,
  output wire [3:0] y
);

  inv_8_to_4 partial_wide_inv_cell (
    .A(a),
    .Y(y)
  );

endmodule