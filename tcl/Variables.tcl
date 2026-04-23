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

trace add variable ::sta_use_kahns_bfs {read write} \
  sta::trace_use_kahns_bfs

proc trace_use_kahns_bfs { name1 name2 op } {
  trace_boolean_var $op ::sta_use_kahns_bfs \
    use_kahns_bfs set_use_kahns_bfs
}

trace add variable ::sta_kahn_visit_skip {read write} \
  sta::trace_kahn_visit_skip

proc trace_kahn_visit_skip { name1 name2 op } {
  trace_boolean_var $op ::sta_kahn_visit_skip \
    use_kahns_visit_skip set_use_kahns_visit_skip
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
