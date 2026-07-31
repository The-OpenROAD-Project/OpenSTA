`timescale 1ps/1ps

module top (
    input  wire A,
    input  wire clk,
    output wire Y
);

    INVx2_ASAP7_75t_R u_inv (
        .A(A),
        .Y(Y)
    );

endmodule
