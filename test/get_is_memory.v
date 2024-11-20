module get_is_memory (
	input           CLK,
	input           CEN,
	input           GWEN,
	input   [7:0]   WEN,
	input   [6:0]   A,
	input   [7:0]   D,
	output  [7:0]   Q
);

	wire CEN_buf;
	wire GWEN_reg;

	BUFx2_ASAP7_75t_R buf_inst (
		.A(CEN),
		.Y(CEN_buf)
	);

	DFFHQx4_ASAP7_75t_R dff_inst (
		.CLK(CLK),
		.D(GWEN),
		.Q(GWEN_reg)
	);

	gf180mcu_fd_ip_sram__sram128x8m8wm1 sram_inst (
		.CLK(CLK),
		.CEN(CEN_buf),
		.GWEN(GWEN_reg),
		.WEN(WEN),
		.A(A),
		.D(D),
		.Q(Q)
	);

endmodule
