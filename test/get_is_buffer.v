module dut (
	input           A,
	output          Y
);

	sky130_fd_sc_hd__buf_2 buf_inst (
		.A(A),
		.X(Y)
	);

endmodule
