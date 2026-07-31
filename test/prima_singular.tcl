# Prima delay calc on degenerate parasitic networks (STA-1752 regression).
#
# Each buffer output net has both a floating (resistor-less) node and a
# zero-resistance short.  Either one used to make PrimaDelayCalc's conductance
# matrix G singular, raising STA-1752 "G matrix is singular".  Single threaded
# this surfaced as a Tcl error; multi threaded the error was thrown from a
# DispatchQueue worker and aborted with SIGABRT.  findNodeCount() now drops
# isolated nodes and merges shorted nodes, so the delay is computed correctly.
#
# This test runs single threaded and checks the reported path.  To exercise the
# historical multi-threaded crash path set STA_TEST_THREADS to the number of
# parallel buffers (2); the run must still complete without aborting.  (A BFS
# level is dispatched to workers only when its vertex count >= the thread count,
# so more threads than buffers runs inline on the main thread.)
read_liberty asap7_small.lib.gz
read_verilog prima_singular.v
link_design top
create_clock -name clk -period 500 clk
set_input_delay -clock clk 1 [list in0 in1]
set_input_transition 10 [list clk in0 in1]
set_propagated_clock clk
read_spef prima_singular.spef
sta::set_delay_calculator prima
if { [info exists ::env(STA_TEST_THREADS)] } {
  sta::set_thread_count $::env(STA_TEST_THREADS)
} else {
  sta::set_thread_count 1
}
report_checks -group_path_count 1
