# OpenSTA, Static Timing Analyzer
# Copyright (c) 2020, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

namespace eval sta {

################################################################
#
# Variables
#
################################################################

trace variable ::link_make_black_boxes "rw" \
  sta::trace_link_make_black_boxes

proc trace_link_make_black_boxes { name1 name2 op } {
  trace_boolean_var $op ::link_make_black_boxes \
    link_make_black_boxes set_link_make_black_boxes
}

# Default digits to print after decimal point for reporting commands.
set ::sta_report_default_digits 2

trace variable ::sta_report_default_digits "rw" \
  sta::trace_report_default_digits

proc trace_report_default_digits { name1 name2 op } {
  global sta_report_default_digits

  if { $op == "w" } {
    if { !([string is integer $sta_report_default_digits] \
	   && $sta_report_default_digits >= 0) } {
      sta_error "sta_report_default_digits must be a positive integer."
    }
  }
}

trace variable ::sta_crpr_enabled "rw" \
  sta::trace_crpr_enabled

proc trace_crpr_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_crpr_enabled \
    crpr_enabled set_crpr_enabled
}

trace variable ::sta_crpr_mode "rw" \
  sta::trace_crpr_mode

proc trace_crpr_mode { name1 name2 op } {
  global sta_crpr_mode

  if { $op == "r" } {
    set sta_crpr_mode [crpr_mode]
  } elseif { $op == "w" } {
    if { $sta_crpr_mode == "same_pin" || $sta_crpr_mode == "same_transition" } {
      set_crpr_mode $sta_crpr_mode
    } else {
      sta_error "sta_crpr_mode must be pin or transition."
    }
  }
}

trace variable ::sta_cond_default_arcs_enabled "rw" \
  sta::trace_cond_default_arcs_enabled

proc trace_cond_default_arcs_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_cond_default_arcs_enabled \
    cond_default_arcs_enabled set_cond_default_arcs_enabled
}

trace variable ::sta_gated_clock_checks_enabled "rw" \
  sta::trace_gated_clk_checks_enabled

proc trace_gated_clk_checks_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_gated_clock_checks_enabled \
    gated_clk_checks_enabled set_gated_clk_checks_enabled
}

trace variable ::sta_internal_bidirect_instance_paths_enabled "rw" \
  sta::trace_internal_bidirect_instance_paths_enabled

proc trace_internal_bidirect_instance_paths_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_internal_bidirect_instance_paths_enabled \
    bidirect_inst_paths_enabled set_bidirect_inst_paths_enabled
}

trace variable ::sta_bidirect_net_paths_enabled "rw" \
  sta::trace_bidirect_net_paths_enabled

proc trace_bidirect_net_paths_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_bidirect_net_paths_enabled \
    bidirect_net_paths_enabled set_bidirect_net_paths_enabled
}

trace variable ::sta_clock_through_tristate_enabled "rw" \
  sta::trace_clock_through_tristate_enabled

proc trace_clock_through_tristate_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_clock_through_tristate_enabled \
    clk_thru_tristate_enabled set_clk_thru_tristate_enabled
}

trace variable ::sta_preset_clear_arcs_enabled "rw" \
  sta::trace_preset_clr_arcs_enabled

proc trace_preset_clr_arcs_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_preset_clear_arcs_enabled \
    preset_clr_arcs_enabled set_preset_clr_arcs_enabled
}

trace variable ::sta_disable_recovery_removal_checks "rw" \
  sta::trace_disable_recovery_removal_checks

proc trace_disable_recovery_removal_checks { name1 name2 op } {
  trace_bool_var $op ::sta_disable_recovery_removal_checks \
    recovery_removal_checks_enabled set_recovery_removal_checks_enabled
}

trace variable ::sta_dynamic_loop_breaking "rw" \
  sta::trace_dynamic_loop_breaking

proc trace_dynamic_loop_breaking { name1 name2 op } {
  trace_boolean_var $op ::sta_dynamic_loop_breaking \
    dynamic_loop_breaking set_dynamic_loop_breaking
}

trace variable ::sta_input_port_default_clock "rw" \
  sta::trace_input_port_default_clock

proc trace_input_port_default_clock { name1 name2 op } {
  trace_boolean_var $op ::sta_input_port_default_clock \
    use_default_arrival_clock set_use_default_arrival_clock
}

trace variable ::sta_propagate_all_clocks "rw" \
  sta::trace_propagate_all_clocks

proc trace_propagate_all_clocks { name1 name2 op } {
  trace_boolean_var $op ::sta_propagate_all_clocks \
    propagate_all_clocks set_propagate_all_clocks
}

trace variable ::sta_propagate_gated_clock_enable "rw" \
  sta::trace_propagate_gated_clock_enable

proc trace_propagate_gated_clock_enable { name1 name2 op } {
  trace_boolean_var $op ::sta_propagate_gated_clock_enable \
    propagate_gated_clock_enable set_propagate_gated_clock_enable
}

trace variable ::sta_pocv_enabled "rw" \
  sta::trace_pocv_enabled

proc trace_pocv_enabled { name1 name2 op } {
  trace_boolean_var $op ::sta_pocv_enabled \
    pocv_enabled set_pocv_enabled
}

# Report path numeric field width is digits + extra.
set report_path_field_width_extra 5

################################################################

proc trace_boolean_var { op var_name get_proc set_proc } {
  upvar 1 $var_name var

  if { $op == "r" } {
    set var [$get_proc]
  } elseif { $op == "w" } {
    if { $var == 0 } {
      $set_proc 0
    } elseif { $var == 1 } {
      $set_proc 1
    } else {
      sta_error "$var_name value must be 0 or 1."
    }
  }
}

# sta namespace end.
}
