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

# OpenROAD fork: analysis_corner support.
# Sourced after Sta.tcl (list order in CMakeLists.txt/BUILD matters).

namespace eval sta {

define_cmd_args "define_analysis_corner" {name\
                                            [-liberty liberty_files \
                                             | -liberty_min liberty_min_files -liberty_max liberty_max_files]\
                                            [-spef spef_name | -spef_min spef_min_name -spef_max spef_max_name]}

proc define_analysis_corner { args } {
  parse_key_args "define_analysis_corner" args \
    keys {-liberty -liberty_min -liberty_max -spef -spef_min -spef_max} \
    flags {}
  check_argc_eq1 "define_analysis_corner" $args
  set name [lindex $args 0]

  set liberty_min_files {}
  set liberty_max_files {}
  if { [info exists keys(-liberty)] } {
    set liberty_min_files $keys(-liberty)
    set liberty_max_files $keys(-liberty)
  } elseif { [info exists keys(-liberty_min)] && [info exists keys(-liberty_max)] } {
    set liberty_min_files $keys(-liberty_min)
    set liberty_max_files $keys(-liberty_max)
  } elseif { [info exists keys(-liberty_min)] || [info exists keys(-liberty_max)] } {
    sta_error 3705 "-liberty_min and -liberty_max are required arguments."
  }

  set spef_min_name ""
  set spef_max_name ""
  if { [info exists keys(-spef)] } {
    set spef_min_name $keys(-spef)
    set spef_max_name $keys(-spef)
  } elseif { [info exists keys(-spef_min)] && [info exists keys(-spef_max)] } {
    set spef_min_name $keys(-spef_min)
    set spef_max_name $keys(-spef_max)
  } elseif { [info exists keys(-spef_min)] || [info exists keys(-spef_max)] } {
    sta_error 3706 "-spef_min and -spef_max are required arguments."
  }

  define_analysis_corner_cmd $name $liberty_min_files $liberty_max_files \
    $spef_min_name $spef_max_name
}

define_cmd_args "get_analysis_corners" {[corner_name]}

proc get_analysis_corners { args } {
  check_argc_eq0or1 "get_analysis_corners" $args
  if { [llength $args] == 0 } {
    set pattern "*"
  } else {
    set pattern [lindex $args 0]
  }
  return [find_analysis_corners_matching $pattern]
}

define_cmd_args "set_scene_analysis_corner" {scene_name corner_name}

proc set_scene_analysis_corner { args } {
  check_argc_eq2 "set_scene_analysis_corner" $args
  set scene_name [lindex $args 0]
  set corner_name [lindex $args 1]
  set scene [find_scene $scene_name]
  if { $scene == "NULL" } {
    sta_error 3702 "$scene_name is not the name of a scene."
  }
  set corner [find_analysis_corner $corner_name]
  if { $corner == "NULL" } {
    sta_error 3703 "$corner_name is not the name of an analysis corner."
  }
  set_scene_analysis_corner_cmd $scene $corner
}

# Extend define_scene with -analysis_corner by wrapping the proc at
# runtime instead of editing Sta.tcl (upstream merge hygiene).
if { [info procs define_scene_base] == "" } {
  rename define_scene define_scene_base
}

proc define_scene { args } {
  # Passthrough parse: extract our key plus the base liberty/spef keys so
  # the corner bundle is only injected when the user supplied none of them
  # (explicit args win over the bundle). Unparsed args stay for the base cmd.
  parse_key_args "define_scene" args \
    keys {-analysis_corner -liberty -liberty_min -liberty_max \
          -spef -spef_min -spef_max} \
    flags {} 0
  set corner "NULL"
  if { [info exists keys(-analysis_corner)] } {
    set corner [find_analysis_corner $keys(-analysis_corner)]
    if { $corner == "NULL" } {
      sta_error 3704 "$keys(-analysis_corner) is not the name of an analysis corner."
    }
  }
  # Reinject the user's liberty/spef keys verbatim.
  set user_liberty 0
  foreach key {-liberty -liberty_min -liberty_max} {
    if { [info exists keys($key)] } {
      set user_liberty 1
      lappend args $key $keys($key)
    }
  }
  set user_spef 0
  foreach key {-spef -spef_min -spef_max} {
    if { [info exists keys($key)] } {
      set user_spef 1
      lappend args $key $keys($key)
    }
  }
  if { $corner != "NULL" } {
    # Compose the corner's bundle for anything the user left out.
    # Min/max are always set as a pair, so checking min suffices.
    if { !$user_liberty } {
      set liberty_min [analysis_corner_liberty_min $corner]
      if { $liberty_min == {} } {
        sta_error 3707 "analysis corner $keys(-analysis_corner) defines no liberty files; use -liberty or define_analysis_corner -liberty."
      }
      lappend args -liberty_min $liberty_min \
        -liberty_max [analysis_corner_liberty_max $corner]
    }
    if { !$user_spef } {
      set spef_min [analysis_corner_spef_min $corner]
      if { $spef_min != "" } {
        lappend args -spef_min $spef_min \
          -spef_max [analysis_corner_spef_max $corner]
      }
    }
  }
  define_scene_base {*}$args
  if { $corner != "NULL" } {
    # Sta::makeScene leaves the new scene as cmd_scene_.
    set_scene_analysis_corner_cmd [cmd_scene] $corner
  }
}

################################################################
#
# Corner-scoped SDC (currently timing derates).
#
################################################################

define_cmd_args "set_cmd_analysis_corner" {corner_name}

proc set_cmd_analysis_corner { args } {
  check_argc_eq1 "set_cmd_analysis_corner" $args
  set corner_name [lindex $args 0]
  set corner [find_analysis_corner $corner_name]
  if { $corner == "NULL" } {
    sta_error 3708 "$corner_name is not the name of an analysis corner."
  }
  set_cmd_analysis_corner_cmd $corner
}

define_cmd_args "unset_cmd_analysis_corner" {}

proc unset_cmd_analysis_corner { args } {
  check_argc_eq0 "unset_cmd_analysis_corner" $args
  set_cmd_analysis_corner_cmd NULL
}

# Extend read_sdc with -analysis_corner by wrapping the proc at runtime
# (same technique as define_scene above). Unknown keys (-mode, -echo,
# redirection) pass through parse_key_args verbatim to the base command.
if { [info procs read_sdc_base] == "" } {
  rename read_sdc read_sdc_base
}

proc read_sdc { args } {
  parse_key_args "read_sdc" args keys {-analysis_corner} flags {} 0
  if { [info exists keys(-analysis_corner)] } {
    set corner [find_analysis_corner $keys(-analysis_corner)]
    if { $corner == "NULL" } {
      sta_error 3709 "$keys(-analysis_corner) is not the name of an analysis corner."
    }
    set prev_corner [cmd_analysis_corner]
    set_cmd_analysis_corner_cmd $corner
    try {
      read_sdc_base {*}$args
    } finally {
      set_cmd_analysis_corner_cmd $prev_corner
    }
  } else {
    read_sdc_base {*}$args
  }
}

# Commands that may not run in an analysis corner scope. Structural
# constraints (clocks, exceptions, logic state) stay mode-level by design.
variable corner_scope_disallowed_cmds {
  create_clock create_generated_clock set_clock_groups set_propagated_clock
  set_false_path set_multicycle_path set_max_delay set_min_delay
  group_path set_case_analysis set_disable_timing
}
# Corner-scoped by design but not implemented yet.
variable corner_scope_pending_cmds {
  set_input_delay set_output_delay set_clock_latency set_clock_uncertainty
  set_input_transition set_load
}

proc check_corner_scope { cmd } {
  variable corner_scope_pending_cmds
  if { [cmd_analysis_corner] != "NULL" } {
    if { [lsearch -exact $corner_scope_pending_cmds $cmd] >= 0 } {
      sta_error 3711 "$cmd is not yet supported in an analysis corner scope."
    } else {
      sta_error 3710 "$cmd is not allowed in an analysis corner scope; only timing derates are corner-scoped."
    }
  }
}

# Guard the disallowed commands with a corner-scope check (runtime
# wrappers; the base commands are untouched). The base is invoked with
# uplevel/linsert rather than {*}$args because sdc_file_line list-parses
# the command text of every stack frame (Util.tcl).
foreach cmd [concat $corner_scope_disallowed_cmds $corner_scope_pending_cmds] {
  if { [info procs ${cmd}_corner_scope_base] == "" } {
    rename $cmd ${cmd}_corner_scope_base
    proc $cmd { args } \
      "check_corner_scope $cmd ; uplevel 1 \[linsert \$args 0 ::sta::${cmd}_corner_scope_base\]"
  }
}

# sta namespace end.
}
