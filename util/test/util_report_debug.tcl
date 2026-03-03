# Test Report/Debug formatting and output destination edge cases.
# Targets Report.cc, ReportTcl.cc, Debug.cc, StringUtil.cc, MinMax.cc, gzstream.

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/nangate45_typ.lib.gz
read_verilog ../../dcalc/test/dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

#---------------------------------------------------------------
# Report redirect to file + log simultaneously
#---------------------------------------------------------------
puts "--- redirect + log simultaneously ---"
set log_file [make_result_file "util_debug_log.txt"]
set redir_file [make_result_file "util_debug_redir.txt"]

log_begin $log_file
sta::redirect_file_begin $redir_file
report_checks
report_checks -path_delay min
report_units
sta::redirect_file_end
log_end

diff_files util_debug_redir.txtok $redir_file
diff_files util_debug_log.txtok $log_file

#---------------------------------------------------------------
# gzstream: Read/write gzipped liberty
#---------------------------------------------------------------
puts "--- gzipped liberty read ---"
# Already loaded above from gzipped file.

#---------------------------------------------------------------
# Report warn path (triggered by warnings in design analysis)
#---------------------------------------------------------------
puts "--- trigger warn paths ---"
# Set very large load to trigger potential warnings
set_load 100.0 [get_ports out1]
report_checks

set_load 0 [get_ports out1]

#---------------------------------------------------------------
# Debug: enable and verify paths with timing analysis
#---------------------------------------------------------------
puts "--- debug check path coverage ---"
sta::set_debug "delay_calc" 2

# report_dcalc with debug on exercises debug check/reportLine paths
report_dcalc -from [get_pins buf1/A] -to [get_pins buf1/Z]
puts "dcalc with debug: done"

sta::set_debug "delay_calc" 0

# Debug with levelization
sta::set_debug "levelize" 1
report_checks
sta::set_debug "levelize" 0

# Debug with bfs
sta::set_debug "bfs" 1
report_checks
sta::set_debug "bfs" 0

#---------------------------------------------------------------
# Multiple redirect/log open/close cycles
#---------------------------------------------------------------
puts "--- multiple redirect cycles ---"
set f1 [make_result_file "util_redir1.txt"]
set f2 [make_result_file "util_redir2.txt"]
set f3 [make_result_file "util_redir3.txt"]

# Cycle 1
sta::redirect_file_begin $f1
report_units
sta::redirect_file_end

# Cycle 2
sta::redirect_file_begin $f2
report_checks
sta::redirect_file_end

# Cycle 3 - append
sta::redirect_file_append_begin $f2
report_checks -path_delay min
sta::redirect_file_end

# Cycle 4
sta::redirect_file_begin $f3
report_checks -fields {slew cap input_pins}
sta::redirect_file_end

#---------------------------------------------------------------
# String redirect multiple times
#---------------------------------------------------------------
puts "--- string redirect cycles ---"
sta::redirect_string_begin
report_units
set s1 [sta::redirect_string_end]

sta::redirect_string_begin
report_checks
set s2 [sta::redirect_string_end]

sta::redirect_string_begin
report_checks -path_delay min
report_checks -path_delay max
set s3 [sta::redirect_string_end]

puts "s1 len: [string length $s1]"
puts "s2 len: [string length $s2]"
puts "s3 len: [string length $s3]"

#---------------------------------------------------------------
# Report blank line and reportLineString paths
#---------------------------------------------------------------
puts "--- report_line coverage ---"
sta::report_line ""
sta::report_line "test line 1"
sta::report_line "test line with special chars: \[ \] \{ \}"

#---------------------------------------------------------------
# format_* functions with edge values
#---------------------------------------------------------------
puts "--- format functions edge cases ---"
set ft0 [sta::format_time "0" 3]
puts "format_time(0): $ft0"

set ft_neg [sta::format_time "-1e-9" 3]
puts "format_time(-1ns): $ft_neg"

set ft_large [sta::format_time "1e-6" 6]
puts "format_time(1us, 6 digits): $ft_large"

set fc0 [sta::format_capacitance "0" 3]
puts "format_capacitance(0): $fc0"

set fc_large [sta::format_capacitance "1e-9" 3]
puts "format_capacitance(1nF): $fc_large"

set fr0 [sta::format_resistance "0" 3]
puts "format_resistance(0): $fr0"

set fr_large [sta::format_resistance "1e6" 3]
puts "format_resistance(1MOhm): $fr_large"

set fp0 [sta::format_power "0" 3]
puts "format_power(0): $fp0"

set fp_large [sta::format_power "1.0" 3]
puts "format_power(1W): $fp_large"

#---------------------------------------------------------------
# set_cmd_units with various suffix combinations
#---------------------------------------------------------------
puts "--- set_cmd_units edge cases ---"
set_cmd_units -time ps
report_units
set_cmd_units -time us
report_units
set_cmd_units -time ns

set_cmd_units -capacitance pF
set_cmd_units -capacitance fF
set_cmd_units -capacitance pF

set_cmd_units -resistance Ohm
set_cmd_units -resistance kOhm

set_cmd_units -distance nm
set_cmd_units -distance um
set_cmd_units -distance mm
set_cmd_units -distance um

set_cmd_units -power nW
set_cmd_units -power uW
set_cmd_units -power mW
set_cmd_units -power W
set_cmd_units -power mW

set_cmd_units -current uA
set_cmd_units -current mA
set_cmd_units -current A
set_cmd_units -current mA

set_cmd_units -voltage mV
set_cmd_units -voltage V

#---------------------------------------------------------------
# suppress/unsuppress with actual message IDs
#---------------------------------------------------------------
puts "--- suppress_msg exercising suppressed check ---"
suppress_msg 1 2 3 4 5 6 7 8 9 10
suppress_msg 100 200 300 400 500

# Unsuppress all
unsuppress_msg 1 2 3 4 5 6 7 8 9 10
unsuppress_msg 100 200 300 400 500

# Suppress then trigger a warning by doing something that warns
suppress_msg 100
unsuppress_msg 100
