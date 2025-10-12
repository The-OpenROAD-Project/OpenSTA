
module counter(clk, reset, in, out);
  input clk;
  output out;
  input reset;
  input in;
  wire mid;

  parameter PARAM1=1;
  parameter PARAM2="test";

  specify
    specparam SPARAM1=2;
    specparam SPARAM2="test2";
  endspecify

  defparam _1415_.PARAM2 = 1;


endmodule
