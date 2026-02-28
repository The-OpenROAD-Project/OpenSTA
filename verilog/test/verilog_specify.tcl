# Test verilog reader with specify blocks, parameters, and defparams
# Targets: VerilogReader.cc (parameter/specify/defparam paths)
#   VerilogLex.ll (lexer paths for specify, parameter, defparam keywords)
#   VerilogParse.yy (parser paths for specify blocks)

source ../../test/helpers.tcl

proc assert_file_nonempty {path} {
  if {![file exists $path] || [file size $path] <= 0} {
    error "expected non-empty file: $path"
  }
}

proc assert_file_contains {path token} {
  set in [open $path r]
  set text [read $in]
  close $in
  if {[string first $token $text] < 0} {
    error "expected '$token' in $path"
  }
}

#---------------------------------------------------------------
# Test: Read verilog with specify blocks and parameters
#---------------------------------------------------------------
puts "--- read verilog with specify/parameter ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../test/verilog_specify.v
link_design counter

set cells [get_cells *]
puts "cells: [llength $cells]"
if {[llength $cells] != 0} {
  error "unexpected cell count in specify test: [llength $cells]"
}

set nets [get_nets *]
puts "nets: [llength $nets]"
if {[llength $nets] < 4} {
  error "unexpected net count in specify test: [llength $nets]"
}

set ports [get_ports *]
puts "ports: [llength $ports]"
if {[llength $ports] != 4} {
  error "unexpected port count in specify test: [llength $ports]"
}

set src_in [open ../../test/verilog_specify.v r]
set src_text [read $src_in]
close $src_in
foreach token {specify parameter defparam} {
  if {[string first $token $src_text] < 0} {
    error "missing expected token '$token' in source verilog_specify.v"
  }
}

#---------------------------------------------------------------
# Write and verify
#---------------------------------------------------------------
puts "--- write_verilog ---"
set outfile [make_result_file verilog_specify_out.v]
write_verilog $outfile
assert_file_nonempty $outfile
assert_file_contains $outfile "module counter"
assert_file_contains $outfile "input clk;"
assert_file_contains $outfile "output out;"
assert_file_contains $outfile "endmodule"
