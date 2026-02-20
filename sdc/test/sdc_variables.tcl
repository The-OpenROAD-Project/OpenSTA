# Test STA variable settings for Variables.cc code coverage
# Targets: Variables.cc, Sdc.cc (variable-related paths)
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

# Setup basic constraints
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_output_delay -clock clk1 3.0 [get_ports out1]

############################################################
# CRPR variables
############################################################

# Enable/disable CRPR
set ::sta_crpr_enabled 1

set ::sta_crpr_enabled 0

set ::sta_crpr_enabled 1

# CRPR mode
set ::sta_crpr_mode "same_pin"

set ::sta_crpr_mode "same_transition"

# Read back crpr mode
set mode $::sta_crpr_mode

############################################################
# Condition default arcs
############################################################

set ::sta_cond_default_arcs_enabled 1

set ::sta_cond_default_arcs_enabled 0

# Read back
set val $::sta_cond_default_arcs_enabled

############################################################
# Gated clock checks
############################################################

set ::sta_gated_clock_checks_enabled 1

set ::sta_gated_clock_checks_enabled 0

set val $::sta_gated_clock_checks_enabled

############################################################
# Bidirectional instance paths
############################################################

set ::sta_internal_bidirect_instance_paths_enabled 1

set ::sta_internal_bidirect_instance_paths_enabled 0

set val $::sta_internal_bidirect_instance_paths_enabled

############################################################
# Bidirectional net paths
############################################################

set ::sta_bidirect_net_paths_enabled 1

set ::sta_bidirect_net_paths_enabled 0

set val $::sta_bidirect_net_paths_enabled

############################################################
# Clock through tristate
############################################################

set ::sta_clock_through_tristate_enabled 1

set ::sta_clock_through_tristate_enabled 0

set val $::sta_clock_through_tristate_enabled

############################################################
# Preset/clear arcs
############################################################

set ::sta_preset_clear_arcs_enabled 1

set ::sta_preset_clear_arcs_enabled 0

set val $::sta_preset_clear_arcs_enabled

############################################################
# Recovery/removal checks
############################################################

set ::sta_recovery_removal_checks_enabled 1

set ::sta_recovery_removal_checks_enabled 0

set val $::sta_recovery_removal_checks_enabled

############################################################
# Dynamic loop breaking
############################################################

set ::sta_dynamic_loop_breaking 1

set ::sta_dynamic_loop_breaking 0

set val $::sta_dynamic_loop_breaking

############################################################
# Input port default clock
############################################################

set ::sta_input_port_default_clock 1

set ::sta_input_port_default_clock 0

set val $::sta_input_port_default_clock

############################################################
# Propagate all clocks
############################################################

set ::sta_propagate_all_clocks 1

set ::sta_propagate_all_clocks 0

set val $::sta_propagate_all_clocks

############################################################
# Propagate gated clock enable
############################################################

set ::sta_propagate_gated_clock_enable 1

set ::sta_propagate_gated_clock_enable 0

set val $::sta_propagate_gated_clock_enable

############################################################
# POCV enabled (may require SSTA compilation, use catch)
############################################################

# catch: POCV variables may not exist if SSTA is not compiled in
catch {
  set ::sta_pocv_enabled 1
  set ::sta_pocv_enabled 0
  set val $::sta_pocv_enabled
}

############################################################
# Report default digits
############################################################

set ::sta_report_default_digits 4

set ::sta_report_default_digits 2

############################################################
# Final report to verify everything still works
############################################################

report_checks -from [get_ports in1] -to [get_ports out1]
