# read_spef -path -reduce on a net connected to a hierarchical port: the
# network is made for the highest connected net (n1) and must be reduced and
# deleted on that net, not the local net (sub1/out). Regression for upstream
# becb3294, fixed in 7b261909.
read_liberty asap7_small.lib.gz
read_verilog spef_hier_reduce.v
link_design top
read_spef -path sub1 -reduce spef_hier_reduce.spef
report_net n1
puts "no crash"
