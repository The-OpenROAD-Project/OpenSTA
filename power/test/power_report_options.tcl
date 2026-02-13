# Test power report options: activity annotation, multiple activity settings
# Exercises: report_activity_annotation, various set_power_activity combinations

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog power_test1.v
link_design power_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]
set_input_transition 0.1 [get_ports in1]

#---------------------------------------------------------------
# Baseline power
#---------------------------------------------------------------
puts "--- Baseline power ---"
set_power_activity -global -activity 0.5 -duty 0.5
report_power
puts "PASS: report_power baseline"

# Instance power reports
report_power -instances [get_cells buf1]
puts "PASS: report_power -instances buf1"

report_power -instances [get_cells reg1]
puts "PASS: report_power -instances reg1"

report_power -instances [get_cells *] -digits 4
puts "PASS: report_power -instances all -digits 4"

# JSON instance power
report_power -instances [get_cells *] -format json
puts "PASS: report_power -instances json"

report_power -instances [get_cells buf1] -format json
puts "PASS: report_power -instances buf1 json"

#---------------------------------------------------------------
# Different activity settings and recheck
#---------------------------------------------------------------
puts "--- Different activity values ---"
unset_power_activity -global

set_power_activity -global -activity 0.1 -duty 0.3
report_power
puts "PASS: report_power activity=0.1 duty=0.3"

unset_power_activity -global
set_power_activity -global -activity 0.8 -duty 0.7
report_power
puts "PASS: report_power activity=0.8 duty=0.7"

unset_power_activity -global
set_power_activity -global -activity 1.0 -duty 0.5
report_power
puts "PASS: report_power activity=1.0 duty=0.5"

#---------------------------------------------------------------
# report_activity_annotation
#---------------------------------------------------------------
puts "--- Activity annotation reporting ---"
report_activity_annotation
puts "PASS: report_activity_annotation"

report_activity_annotation -report_unannotated
puts "PASS: report_activity_annotation -report_unannotated"

report_activity_annotation -report_annotated
puts "PASS: report_activity_annotation -report_annotated"

#---------------------------------------------------------------
# Annotate some specific activity then report annotation
#---------------------------------------------------------------
puts "--- Annotate then check annotation ---"
unset_power_activity -global
set_power_activity -input -activity 0.5 -duty 0.5
set_power_activity -pins [get_pins buf1/A] -activity 0.3 -duty 0.4

report_activity_annotation
puts "PASS: report_activity_annotation after pin annotation"

report_activity_annotation -report_annotated
puts "PASS: report_activity_annotation -report_annotated after pin annotation"

report_activity_annotation -report_unannotated
puts "PASS: report_activity_annotation -report_unannotated after pin annotation"

report_power
puts "PASS: report_power after activity annotation"

report_power -digits 6
puts "PASS: report_power -digits 6 after annotation"

report_power -format json
puts "PASS: report_power json after annotation"

#---------------------------------------------------------------
# Unset pin activity and verify
#---------------------------------------------------------------
puts "--- Unset pin activity ---"
unset_power_activity -pins [get_pins buf1/A]
report_power
puts "PASS: report_power after unset pin activity"

unset_power_activity -input
report_power
puts "PASS: report_power after unset input activity"

#---------------------------------------------------------------
# Density-based activity
#---------------------------------------------------------------
puts "--- Density-based activity ---"
set_power_activity -global -density 1e8 -duty 0.5
report_power
puts "PASS: report_power with density"

report_activity_annotation
puts "PASS: annotation after density"

unset_power_activity -global

#---------------------------------------------------------------
# Input port specific activity
#---------------------------------------------------------------
puts "--- Input port activity ---"
set_power_activity -input_ports [get_ports in1] -activity 0.4 -duty 0.6
report_power
puts "PASS: report_power with input_port activity"

report_activity_annotation -report_annotated
puts "PASS: annotation after input_port activity"

unset_power_activity -input_ports [get_ports in1]
report_power
puts "PASS: report_power after unset input_port activity"

puts "ALL PASSED"
