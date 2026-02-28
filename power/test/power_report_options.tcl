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

# Instance power reports
report_power -instances [get_cells buf1]

report_power -instances [get_cells reg1]

report_power -instances [get_cells *] -digits 4

# JSON instance power
report_power -instances [get_cells *] -format json

report_power -instances [get_cells buf1] -format json

#---------------------------------------------------------------
# Different activity settings and recheck
#---------------------------------------------------------------
puts "--- Different activity values ---"
unset_power_activity -global

set_power_activity -global -activity 0.1 -duty 0.3
report_power

unset_power_activity -global
set_power_activity -global -activity 0.8 -duty 0.7
report_power

unset_power_activity -global
set_power_activity -global -activity 1.0 -duty 0.5
report_power

#---------------------------------------------------------------
# report_activity_annotation
#---------------------------------------------------------------
puts "--- Activity annotation reporting ---"
report_activity_annotation

report_activity_annotation -report_unannotated

report_activity_annotation -report_annotated

#---------------------------------------------------------------
# Annotate some specific activity then report annotation
#---------------------------------------------------------------
puts "--- Annotate then check annotation ---"
unset_power_activity -global
set_power_activity -input -activity 0.5 -duty 0.5
set_power_activity -pins [get_pins buf1/A] -activity 0.3 -duty 0.4

report_activity_annotation

report_activity_annotation -report_annotated

report_activity_annotation -report_unannotated

report_power

report_power -digits 6

report_power -format json

#---------------------------------------------------------------
# Unset pin activity and verify
#---------------------------------------------------------------
puts "--- Unset pin activity ---"
unset_power_activity -pins [get_pins buf1/A]
report_power

unset_power_activity -input
report_power

#---------------------------------------------------------------
# Density-based activity
#---------------------------------------------------------------
puts "--- Density-based activity ---"
set_power_activity -global -density 1e8 -duty 0.5
report_power

report_activity_annotation

unset_power_activity -global

#---------------------------------------------------------------
# Input port specific activity
#---------------------------------------------------------------
puts "--- Input port activity ---"
set_power_activity -input_ports [get_ports in1] -activity 0.4 -duty 0.6
report_power

report_activity_annotation -report_annotated

unset_power_activity -input_ports [get_ports in1]
report_power
