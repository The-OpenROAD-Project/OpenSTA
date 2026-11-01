# Two .blib libraries bound to scenes by library name. The expected output
# is identical to the same script reading the ../examples/asap7_small_*.lib.gz
# text libraries directly.
source helpers.tcl

set ff_blib [make_result_file asap7_small_ff.blib]
set ss_blib [make_result_file asap7_small_ss.blib]
write_liberty_binary ../examples/asap7_small_ff.lib.gz $ff_blib
write_liberty_binary ../examples/asap7_small_ss.lib.gz $ss_blib
read_liberty $ff_blib
read_liberty $ss_blib
read_verilog ../examples/reg1_asap7.v
link_design top
read_sdc ../examples/mcmm2_mode1.sdc

define_scene ff -liberty asap7_small_ff
define_scene ss -liberty asap7_small_ss

report_checks -path_delay min_max -scenes {ff ss} -digits 3
