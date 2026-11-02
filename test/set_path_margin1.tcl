# set_path_margin -to with -setup/-hold
source set_path_margin_common.tcl

set_path_margin -setup 0.50 -comment {tighten setup time} -to [get_pins r3/D]
report_path_margin -path_delay max -to [get_pins r3/D]
set_path_margin -hold 0.50 -comment {tighten hold time} -to [get_pins r3/D]
report_path_margin -path_delay min -to [get_pins r3/D]
set_path_margin -setup -67 -comment {loosen setup time} -to [get_pins r3/D]
report_path_margin -path_delay max -to [get_pins r3/D]
set_path_margin -hold -0.50 -comment {loosen hold time} -to [get_pins r3/D]
report_path_margin -path_delay min -to [get_pins r3/D]
