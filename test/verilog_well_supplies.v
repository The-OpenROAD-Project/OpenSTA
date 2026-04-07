module top (
    output y,
    input  a
);
  supply1 VPWR;
  supply0 VGND;
  supply1 VPB;
  supply0 VNB;
  sky130_fd_sc_hd__buf_1 u1 (
      .X(y),
      .A(a),
      .VPWR(VPWR),
      .VGND(VGND),
      .VPB(VPB),
      .VNB(VNB)
  );
endmodule
