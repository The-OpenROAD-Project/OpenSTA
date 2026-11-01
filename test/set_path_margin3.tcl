# set_path_margin -from and -to
source set_path_margin_common.tcl

set_path_margin -setup 5.0 -from [get_pins r1/CK] -to [get_pins r3/D]
# Should see path margin.
report_path_margin -path_delay max -from [get_pins r1/CK] -to [get_pins r3/D]
# Should not see path margin.
report_path_margin -path_delay max -from [get_pins r2/CK] -to [get_pins r3/D]
