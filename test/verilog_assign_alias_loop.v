// Two continuous assigns aliasing p and q in both directions.  The first
// merges q into p; on the second both sides resolve to the same net, so
// mergeInto is called with net == this.  Reachable from real netlists after
// buffer removal or ECO rewiring, where one pass adds an alias and a later
// pass adds the reverse.
module top (a, y);
  input a;
  output y;
  wire p;
  wire q;
  BUF_X1 g0 (.A(a), .Z(p));
  assign q = p;
  assign p = q;
  INV_X1 g1 (.A(q), .ZN(y));
endmodule
