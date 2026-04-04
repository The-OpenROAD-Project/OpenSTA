# Test that '.' is allowed in filter predicate values.
# Targets: FilterExpr::lex (FilterObjects.cc)
source ../../test/helpers.tcl

read_liberty ../../test/asap7_small.lib.gz
read_verilog ../../test/reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}

# Filter with dot in glob pattern value (no cells have literal '.' so no match)
puts {[get_cells -filter {name =~ r.*} *]}
report_object_full_names [get_cells -filter {name =~ r.*} *]

# Verify filter_expr_to_postfix parses dot-containing values
puts [sta::filter_expr_to_postfix "name =~ tcdm_master_.*req_.*_i" 0]
