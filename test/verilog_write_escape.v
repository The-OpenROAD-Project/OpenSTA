module \multi_sink (clk);
 input clk;
 wire \alu_adder_result_ex[0] ;
 \hier_block \h1\x (.childclk(clk),  .\Y[2:1] ({ \alu_adder_result_ex[0] , \alu_adder_result_ex[0]  }) );
endmodule // multi_sink

module \hier_block (childclk, \Y[2:1] );
   input childclk; 
   output [1:0] \Y[2:1] ;
   wire [1:0] \Y[2:1] ;
   BUFx2_ASAP7_75t_R \abuf_$100  (.A(childclk));
   BUFx2_ASAP7_75t_R \ff0/name (.A(childclk));
endmodule // hier_block1
