# SWIG Seq* leak if %exception returns past %typemap(freearg).
# tclListSeqPtr allocates a non-empty Seq, then FilterExpr throws.
# Empty Tcl lists do not allocate (tclListSeqPtr returns nullptr).
# Run under -fsanitize=address or ./regression -valgrind.
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
create_clock -name vclk -period 1000
set_input_delay -clock clk 0 {in1 in2}

# Direct wrapper that leaked in get_filter valgrind (filter_insts).
puts {sta::filter_insts {name ~= *r1*} [get_cells *]}
catch {sta::filter_insts {name ~= *r1*} [get_cells *]} result
puts $result

puts {[get_cells -filter {name ~= *r1*} *]}
catch {get_cells -filter {name ~= *r1*} *} result
puts $result
puts {[get_pins -filter {name ~= *CLK*} *]}
catch {get_pins -filter {name ~= *CLK*} *} result
puts $result
puts {[get_ports -filter {name ~= clk*} *]}
catch {get_ports -filter {name ~= clk*} *} result
puts $result
puts {[get_nets -filter {name ~= *q*} *]}
catch {get_nets -filter {name ~= *q*} *} result
puts $result
puts {[get_clocks -filter {name ~= clk*} *]}
catch {get_clocks -filter {name ~= clk*} *} result
puts $result
puts {[get_lib_cells -filter {name ~= BUF*} *]}
catch {get_lib_cells -filter {name ~= BUF*} *} result
puts $result
puts {[get_lib_pins -filter {name ~= A} BUFx2_ASAP7_75t_R/*]}
catch {get_lib_pins -filter {name ~= A} BUFx2_ASAP7_75t_R/*} result
puts $result
puts {[get_libs -filter {name ~= asap*} *]}
catch {get_libs -filter {name ~= asap*} *} result
puts $result
puts {[get_timing_edges -of_objects u1 -filter {from_pin ~= *}]}
catch {get_timing_edges -of_objects u1 -filter {from_pin ~= *}} result
puts $result

puts {sta::filter_path_ends {name ~= x} [find_timing_paths]}
catch {sta::filter_path_ends {name ~= x} [find_timing_paths]} result
puts $result
