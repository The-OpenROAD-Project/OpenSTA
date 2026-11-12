# Driver slew merge ignores timing arcs whose when cond is disabled by constants.
read_liberty power_activity_cases.lib
read_verilog dcalc_cond_disabled_slew.v
link_design top
report_checks -from in -to out -unconstrained -fields slew
set_case_analysis 0 se
report_checks -from in -to out -unconstrained -fields slew
set_case_analysis 1 se
report_checks -from in -to out -unconstrained -fields slew
