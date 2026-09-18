module multi_sink (clk, \3out );
 input clk;
 output \3out ;
 wire \alu_adder_result_ex[0] ;
 // Names below are legal verilog only while escaped; they are all
 // alphanumeric but start with a digit.
 wire \54y ;
 wire [1:0] \7bus ;
 \hier_block \h1\x (.childclk(clk),  .\Y[2:1] ({ \alu_adder_result_ex[0] , \alu_adder_result_ex[0]  }) );
 BUFx2_ASAP7_75t_R \1inst  (.A(clk), .Y(\54y ));
 BUFx2_ASAP7_75t_R b2 (.A(\54y ), .Y(\7bus [0]));
 BUFx2_ASAP7_75t_R b3 (.A(\7bus [0]), .Y(\3out ));
endmodule // multi_sink

module hier_block (childclk, \Y[2:1] );
   input childclk;
   output [1:0] \Y[2:1] ;
   wire [1:0] \Y[2:1] ;
   BUFx2_ASAP7_75t_R \abuf_$100  (.A(childclk));
   BUFx2_ASAP7_75t_R \ff0/name (.A(childclk));
endmodule // hier_block1
