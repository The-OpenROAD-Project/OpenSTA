# OpenSTA, Static Timing Analyzer
# Copyright (c) 2026, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
# 
# The origin of this software must not be misrepresented; you must not
# claim that you wrote the original software.
# 
# Altered source versions must be plainly marked as such, and must not be
# misrepresented as being the original software.
# 
# This notice may not be removed or altered from any source distribution.

namespace eval sta {

################################################################
#
# Variables
#
################################################################

# Default digits to print after decimal point for reporting commands.
set ::sta_report_default_digits 2

trace add variable ::sta_report_default_digits {read write} \
  sta::trace_report_default_digits

proc trace_report_default_digits { name1 name2 op } {
  global sta_report_default_digits

  if { $op == "write" } {
    if { !([string is integer $sta_report_default_digits] \
           && $sta_report_default_digits >= 0) } {
      sta_error 590 "sta_report_default_digits must be a positive integer."
    }
  }
}

trace add variable ::sta_crpr_enabled {read write} \
  sta::trace_crpr_enabled

proc trace_crpr_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_crpr_enabled \
    crpr_enabled set_crpr_enabled
}

trace add variable ::sta_crpr_mode {read write} \
  sta::trace_crpr_mode

proc trace_crpr_mode { name1 name2 op } {
  global sta_crpr_mode

  if { $op == "read" } {
    set sta_crpr_mode [crpr_mode]
  } elseif { $op == "write" } {
    if { $sta_crpr_mode == "same_pin" || $sta_crpr_mode == "same_transition" } {
      set_crpr_mode $sta_crpr_mode
    } else {
      sta_error 591 "sta_crpr_mode must be pin or transition."
    }
  }
}

trace add variable ::sta_cond_default_arcs_enabled {read write} \
  sta::trace_cond_default_arcs_enabled

proc trace_cond_default_arcs_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_cond_default_arcs_enabled \
    cond_default_arcs_enabled set_cond_default_arcs_enabled
}

trace add variable ::sta_gated_clock_checks_enabled {read write} \
  sta::trace_gated_clk_checks_enabled

proc trace_gated_clk_checks_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_gated_clock_checks_enabled \
    gated_clk_checks_enabled set_gated_clk_checks_enabled
}

trace add variable ::sta_internal_bidirect_instance_paths_enabled {read write} \
  sta::trace_internal_bidirect_instance_paths_enabled

proc trace_internal_bidirect_instance_paths_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_internal_bidirect_instance_paths_enabled \
    bidirect_inst_paths_enabled set_bidirect_inst_paths_enabled
}

trace add variable ::sta_clock_through_tristate_enabled {read write} \
  sta::trace_clock_through_tristate_enabled

proc trace_clock_through_tristate_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_clock_through_tristate_enabled \
    clk_thru_tristate_enabled set_clk_thru_tristate_enabled
}

trace add variable ::sta_preset_clear_arcs_enabled {read write} \
  sta::trace_preset_clr_arcs_enabled

proc trace_preset_clr_arcs_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_preset_clear_arcs_enabled \
    preset_clr_arcs_enabled set_preset_clr_arcs_enabled
}

trace add variable ::sta_recovery_removal_checks_enabled {read write} \
  sta::trace_recovery_removal_checks_enabled

proc trace_recovery_removal_checks_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_recovery_removal_checks_enabled \
    recovery_removal_checks_enabled set_recovery_removal_checks_enabled
}

trace add variable ::sta_dynamic_loop_breaking {read write} \
  sta::trace_dynamic_loop_breaking

proc trace_dynamic_loop_breaking { name1 name2 op } {
  trace_boolean_var $op ::sta_dynamic_loop_breaking \
    dynamic_loop_breaking set_dynamic_loop_breaking
}

trace add variable ::sta_input_port_default_clock {read write} \
  sta::trace_input_port_default_clock

proc trace_input_port_default_clock { name1 name2 op } {
  trace_boolean_var $op ::sta_input_port_default_clock \
    use_default_arrival_clock set_use_default_arrival_clock
}

trace add variable ::sta_propagate_all_clocks {read write} \
  sta::trace_propagate_all_clocks

proc trace_propagate_all_clocks { name1 name2 op } {
  trace_boolean_var $op ::sta_propagate_all_clocks \
    propagate_all_clocks set_propagate_all_clocks
}

trace add variable ::sta_propagate_gated_clock_enable {read write} \
  sta::trace_propagate_gated_clock_enable

proc trace_propagate_gated_clock_enable { name1 name2 op } {
  trace_boolean_var $op ::sta_propagate_gated_clock_enable \
    propagate_gated_clock_enable set_propagate_gated_clock_enable
}

trace add variable ::sta_pocv_mode {read write} \
  sta::trace_pocv_mode

proc trace_pocv_mode { name1 name2 op } {
  global sta_pocv_mode

  if { $op == "read" } {
    set sta_pocv_mode [pocv_mode]
  } elseif { $op == "write" } {
    if { $sta_pocv_mode == "scalar" \
           || $sta_pocv_mode == "normal" \
           || $sta_pocv_mode == "skew_normal" } {
      set_pocv_mode $sta_pocv_mode
    } else {
      sta_error 593 "sta_pocv_mode must be scalar, normal, or skew_normal."
    }
  }
}

trace add variable ::sta_pocv_quantile {read write} \
  sta::trace_pocv_quantile

proc trace_pocv_quantile { name1 name2 op } {
  global sta_pocv_quantile

  if { $op == "read" } {
    set sta_pocv_quantile [pocv_quantile]
  } elseif { $op == "write" } {
    if { [string is double $sta_pocv_quantile] \
           && $sta_pocv_quantile >= 0.0 } {
      set_pocv_quantile $sta_pocv_quantile
    } else {
      sta_error 594 "sta_pocv_quantile must be a positive floating point number."
    }
  }
}

################################################################

define_var_help hierarchy_separator {/ @ ^ # . |} \
  {The `hierarchy_separator` separates instance names in a hierarchical instance, net, or pin name. The default value is '/'.}

define_var_help sta_continue_on_error {0|1} \
  {The `include` and `read_sdc` commands stop and report any errors encountered while reading a file unless `sta_continue_on_error` is 1. The default value is 0.}

define_var_help sta_report_default_digits {integer} \
  {The number of digits to print after a decimal point. The default value is 2.}

define_var_help sta_crpr_enabled {0|1} \
  {During min/max timing analysis for on_chip_variation the data and clock paths may overlap. For a setup check the maximum path delays are used for the data and the minimum path delays are used for the clock. Because the gates cannot simultaneously have minimum and maximum delays the timing check slack is pessimistic. This pessimism is known as Common Reconvergent Pessimism Removal, or CRPR. Enabling CRPR slows down the analysis. The default value is 1.}

define_var_help sta_crpr_mode {same_pin|same_transition} \
  {When the data and clock paths of a timing check overlap (see `sta_crpr_enabled`), pessimism is removed independent of the path rise/fall transitions. When `sta_crpr_mode` is `same_transition`, the pessimism is only removed if the path rise/fall transitions are the same. The default value is `same_pin`.}

define_var_help sta_cond_default_arcs_enabled {0|1} \
  {When set to 0, default timing arcs with no condition (Liberty timing arcs with no when expression) are disabled if there are other conditional timing arcs between the same pins. The default value is 1.}

define_var_help sta_gated_clock_checks_enabled {0|1} \
  {When `sta_gated_clock_checks_enabled` is 1, clock gating setup and hold timing checks are checked. The default value is 1.}

define_var_help sta_internal_bidirect_instance_paths_enabled {0|1} \
  {When set to 0, paths from bidirectional (inout) ports back into the instance are disabled. When set to 1, paths from bidirectional ports back into the instance are enabled. The default value is 0.}

define_var_help sta_preset_clear_arcs_enabled {0|1} \
  {When set to 1, paths through asynchronous preset and clear timing arcs are searched. The default value is 0.}

define_var_help sta_recovery_removal_checks_enabled {0|1} \
  {When `sta_recovery_removal_checks_enabled` is 0, recovery and removal timing checks are disabled. The default value is 1.}

define_var_help sta_dynamic_loop_breaking {0|1} \
  {When `sta_dynamic_loop_breaking` is 0, combinational logic loops are disabled by disabling a timing arc that closes the loop. When `sta_dynamic_loop_breaking` is 1, all paths around the loop are reported. The default value is 0.}

define_var_help sta_input_port_default_clock {0|1} \
  {When `sta_input_port_default_clock` is 1 a default input arrival is added for input ports that do not have an arrival time specified with the `set_input_delay` command. The default value is 0.}

define_var_help sta_propagate_all_clocks {0|1} \
  {All clocks defined after `sta_propagate_all_clocks` is set to 1 are propagated. If it is set before any clocks are defined it has the same effect as

```
set_propagated_clock [all_clocks]
```

after all clocks have been defined. The default value is 0.}

define_var_help sta_propagate_gated_clock_enable {0|1} \
  {When set to 1, paths of gated clock enables are propagated through the clock gating instances. If the gated clock controls sequential elements setting `sta_propagate_gated_clock_enable` to 0 prevents spurious paths from the clock enable. The default value is 1.}

define_var_help sta_pocv_mode {scalar|normal|skew_normal} \
  {Enable parametric on chip variation using statistical timing analysis. The default value is `scalar`.}

define_var_help sta_pocv_quantile {float} \
  {The target quantile of a delay probability distribution (confidence level). The default value is 3 standard deviations, or sigma.}

################################################################

proc trace_boolean_var { op var_name get_proc set_proc } {
  upvar 1 $var_name var

  if { $op == "read" } {
    set var [$get_proc]
  } elseif { $op == "write" } {
    if { $var == 0 } {
      $set_proc 0
    } elseif { $var == 1 } {
      $set_proc 1
    } else {
      sta_error 592 "$var_name value must be 0 or 1."
    }
  }
}

# sta namespace end.
}
