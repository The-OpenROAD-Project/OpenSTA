module dut(fast_dut_Y, fast_dut_A);
  output fast_dut_Y;
  wire fast_dut_Y;
  input [63:0] fast_dut_A;
  wire [63:0] fast_dut_A;
  wire n_4069275;
  wire n_4069276;
  wire n_4069277;
  wire n_4069278;
  wire n_4069279;
  wire n_4069280;
  wire n_4069281;
  wire n_4069282;
  wire n_4069283;
  wire n_4069284;
  wire n_4069285;
  wire n_4069286;
  wire n_4069287;
  wire n_4069288;
  wire n_4069289;
  wire n_4069290;
  wire n_4069291;
  wire n_4069292;
  wire n_4069293;
  wire n_4069294;
  wire n_4069295;
  wire n_4069296;
  wire n_4069297;
  wire n_4069298;
  wire n_4069299;
  wire n_4069300;
  OAI21xp5_ASAP7_75t_SL g3393856 (
    .A1(n_4069287),
    .A2(n_4069300),
    .B(fast_dut_A[63]),
    .Y(fast_dut_Y)
  );
  NAND4xp75_ASAP7_75t_SL g3397213 (
    .A(n_4069277),
    .B(n_4069280),
    .C(n_4069283),
    .D(n_4069286),
    .Y(n_4069287)
  );
  NOR2x1_ASAP7_75t_SL g3397214 (
    .A(n_4069275),
    .B(n_4069276),
    .Y(n_4069277)
  );
  NAND4xp75_ASAP7_75t_SL g3397215 (
    .A(fast_dut_A[13]),
    .B(fast_dut_A[14]),
    .C(fast_dut_A[0]),
    .D(fast_dut_A[16]),
    .Y(n_4069275)
  );
  NAND3xp33_ASAP7_75t_SL g3397216 (
    .A(fast_dut_A[61]),
    .B(fast_dut_A[51]),
    .C(fast_dut_A[50]),
    .Y(n_4069276)
  );
  NOR2x1_ASAP7_75t_SL g3397217 (
    .A(n_4069278),
    .B(n_4069279),
    .Y(n_4069280)
  );
  NAND4xp75_ASAP7_75t_SL g3397218 (
    .A(fast_dut_A[45]),
    .B(fast_dut_A[44]),
    .C(fast_dut_A[47]),
    .D(fast_dut_A[46]),
    .Y(n_4069278)
  );
  NAND4xp75_ASAP7_75t_SL g3397219 (
    .A(fast_dut_A[49]),
    .B(fast_dut_A[48]),
    .C(fast_dut_A[53]),
    .D(fast_dut_A[52]),
    .Y(n_4069279)
  );
  NOR2x1_ASAP7_75t_SL g3397220 (
    .A(n_4069281),
    .B(n_4069282),
    .Y(n_4069283)
  );
  NAND4xp75_ASAP7_75t_SL g3397221 (
    .A(fast_dut_A[31]),
    .B(fast_dut_A[12]),
    .C(fast_dut_A[15]),
    .D(fast_dut_A[6]),
    .Y(n_4069281)
  );
  NAND4xp75_ASAP7_75t_SL g3397222 (
    .A(fast_dut_A[21]),
    .B(fast_dut_A[10]),
    .C(fast_dut_A[38]),
    .D(fast_dut_A[41]),
    .Y(n_4069282)
  );
  NOR2x1_ASAP7_75t_SL g3397223 (
    .A(n_4069284),
    .B(n_4069285),
    .Y(n_4069286)
  );
  NAND4xp75_ASAP7_75t_SL g3397224 (
    .A(fast_dut_A[17]),
    .B(fast_dut_A[29]),
    .C(fast_dut_A[24]),
    .D(fast_dut_A[23]),
    .Y(n_4069284)
  );
  NAND4xp75_ASAP7_75t_SL g3397225 (
    .A(fast_dut_A[37]),
    .B(fast_dut_A[36]),
    .C(fast_dut_A[35]),
    .D(fast_dut_A[33]),
    .Y(n_4069285)
  );
  NAND4xp75_ASAP7_75t_SL g3397226 (
    .A(n_4069290),
    .B(n_4069293),
    .C(n_4069296),
    .D(n_4069299),
    .Y(n_4069300)
  );
  NOR2x1_ASAP7_75t_SL g3397227 (
    .A(n_4069288),
    .B(n_4069289),
    .Y(n_4069290)
  );
  NAND4xp75_ASAP7_75t_SL g3397228 (
    .A(fast_dut_A[58]),
    .B(fast_dut_A[55]),
    .C(fast_dut_A[56]),
    .D(fast_dut_A[1]),
    .Y(n_4069288)
  );
  NAND4xp75_ASAP7_75t_SL g3397229 (
    .A(fast_dut_A[60]),
    .B(fast_dut_A[59]),
    .C(fast_dut_A[3]),
    .D(fast_dut_A[25]),
    .Y(n_4069289)
  );
  NOR2x1_ASAP7_75t_SL g3397230 (
    .A(n_4069291),
    .B(n_4069292),
    .Y(n_4069293)
  );
  NAND4xp75_ASAP7_75t_SL g3397231 (
    .A(fast_dut_A[54]),
    .B(fast_dut_A[57]),
    .C(fast_dut_A[11]),
    .D(fast_dut_A[62]),
    .Y(n_4069291)
  );
  NAND4xp75_ASAP7_75t_SL g3397232 (
    .A(fast_dut_A[20]),
    .B(fast_dut_A[22]),
    .C(fast_dut_A[19]),
    .D(fast_dut_A[28]),
    .Y(n_4069292)
  );
  NOR2x1_ASAP7_75t_SL g3397233 (
    .A(n_4069294),
    .B(n_4069295),
    .Y(n_4069296)
  );
  NAND4xp75_ASAP7_75t_SL g3397234 (
    .A(fast_dut_A[32]),
    .B(fast_dut_A[43]),
    .C(fast_dut_A[42]),
    .D(fast_dut_A[39]),
    .Y(n_4069294)
  );
  NAND4xp75_ASAP7_75t_SL g3397235 (
    .A(fast_dut_A[26]),
    .B(fast_dut_A[34]),
    .C(fast_dut_A[40]),
    .D(fast_dut_A[30]),
    .Y(n_4069295)
  );
  NOR2x1_ASAP7_75t_SL g3397236 (
    .A(n_4069297),
    .B(n_4069298),
    .Y(n_4069299)
  );
  NAND4xp75_ASAP7_75t_SL g3397237 (
    .A(fast_dut_A[27]),
    .B(fast_dut_A[2]),
    .C(fast_dut_A[9]),
    .D(fast_dut_A[4]),
    .Y(n_4069297)
  );
  NAND4xp75_ASAP7_75t_SL g3397238 (
    .A(fast_dut_A[5]),
    .B(fast_dut_A[8]),
    .C(fast_dut_A[18]),
    .D(fast_dut_A[7]),
    .Y(n_4069298)
  );
endmodule
