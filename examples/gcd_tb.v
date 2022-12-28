`define FUNCTIONAL
`define UNIT_DELAY #1
`include "sky130_hd_primitives.v"
`include "sky130_hd.v"
`include "gcd_sky130hd.v"

`timescale 1 ns / 1 ps

module gcd_tb();
   parameter clk_period = 5.0;
   parameter clk_period2 = clk_period / 2.0;

   reg clk;
   reg [15:0] a;
   reg [15:0] b;
   wire [31:0] req_msg;
   reg req_val;
   reg resp_rdy;
   reg reset;

   wire req_rdy;
   wire [15:0] resp_msg;
   wire resp_val;

   // gcd inputs share the same bus port (stoopid)
   assign req_msg[15:0] = a;
   assign req_msg[31:16] = b;
   gcd gcd1(.clk(clk), .req_msg(req_msg), .req_rdy(req_rdy), .req_val(req_val),
            .reset(reset), .resp_msg(resp_msg), .resp_rdy(resp_rdy), .resp_val(resp_val));

   initial begin
      clk = 0;
      a = 0;
      b = 0;
      req_val = 0;
      resp_rdy = 0;
   end

   always
     #clk_period2 clk = ~clk;

   initial begin
      reset = 1;
      #clk_period reset = 0;

      #clk_period a = 5; b = 10; req_val = 1;
      #clk_period req_val = 0;
      #clk_period
      #clk_period
      #clk_period
      #clk_period resp_rdy = 1;
      #clk_period resp_rdy = 0;
      #clk_period

      #clk_period a = 15; b = 150; req_val = 1;
      #clk_period req_val = 0;
      #clk_period
      #clk_period
      #clk_period
      #clk_period
      #clk_period
      #clk_period
      #clk_period
      #clk_period
      #clk_period
      #clk_period
      #clk_period
      #clk_period resp_rdy = 1;
      #clk_period resp_rdy = 0;
      #clk_period

      $finish;
   end

   initial
     begin
        $dumpfile("gcd_sky130hd.vcd");
        $dumpvars(0, gcd_tb);
     end

endmodule
