# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2019-2025, The OpenROAD Authors

# OpenROAD fork: analysis_corner support.
# Sourced after Sta.tcl (list order in CMakeLists.txt/BUILD matters).

namespace eval sta {

define_cmd_args "define_analysis_corner" {name\
                                            [-liberty liberty_files \
                                             | -liberty_min liberty_min_files -liberty_max liberty_max_files]\
                                            [-spef spef_name | -spef_min spef_min_name -spef_max spef_max_name]\
                                            [-sdc sdc_files]}

proc define_analysis_corner { args } {
  parse_key_args "define_analysis_corner" args \
    keys {-liberty -liberty_min -liberty_max -spef -spef_min -spef_max -sdc} \
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

  # Corner-scoped SDC files, applied to the (mode, corner) overlay Sdc
  # when the first scene naming this corner is defined.
  set sdc_files {}
  if { [info exists keys(-sdc)] } {
    set sdc_files $keys(-sdc)
  }

  set replaced [define_analysis_corner_cmd $name $liberty_min_files \
                  $liberty_max_files $spef_min_name $spef_max_name $sdc_files]

  # A redefine with data replaces the whole bundle (the cmd empties the
  # corner's overlay Sdcs): forget which (mode, corner) pairs the old
  # bundle was applied to so the new bundle applies to scenes defined
  # from now on.
  if { $replaced } {
    variable corner_bundle_applied
    foreach pair [dict keys $corner_bundle_applied] {
      if { [lindex $pair 1] == $name } {
        dict unset corner_bundle_applied $pair
      }
    }
  }
}

define_cmd_args "get_analysis_corners" {[-filter expr] [corner_name]}

proc get_analysis_corners { args } {
  parse_key_args "get_analysis_corners" args keys {-filter} flags {}
  check_argc_eq0or1 "get_analysis_corners" $args
  if { [llength $args] == 0 } {
    set pattern "*"
  } else {
    set pattern [lindex $args 0]
  }
  set corners [find_analysis_corners_matching $pattern]
  if { [info exists keys(-filter)] } {
    set corners [filter_analysis_corners $keys(-filter) $corners]
  }
  return $corners
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
  # Match define_scene -analysis_corner: apply the corner's SDC bundle
  # to this (mode, corner) pair.
  apply_corner_sdc_bundle [scene_mode_name $scene] $corner
}

# (mode, corner) pairs whose corner SDC bundle has been applied to the
# corner's overlay Sdc; a bundle is applied once per pair, when the first
# scene naming that mode and corner is defined. Redefining a corner with
# new bundle data forgets its pairs (see define_analysis_corner).
variable corner_bundle_applied [dict create]

# Apply the corner's SDC bundle to the (mode, corner) overlay, once per
# pair. Shared by define_scene -analysis_corner and
# set_scene_analysis_corner so both association routes yield the same
# state.
proc apply_corner_sdc_bundle { mode_name corner } {
  variable corner_bundle_applied
  set corner_sdc [analysis_corner_sdc $corner]
  set pair [list $mode_name [analysis_corner_name $corner]]
  if { $corner_sdc != {} && ![dict exists $corner_bundle_applied $pair] } {
    dict set corner_bundle_applied $pair 1
    foreach f $corner_sdc {
      read_sdc -mode $mode_name -analysis_corner [analysis_corner_name $corner] $f
    }
  }
}

# Extend define_scene with -analysis_corner by wrapping the proc at
# runtime instead of editing Sta.tcl (upstream merge hygiene).
if { [info procs define_scene_base] == "" } {
  rename define_scene define_scene_base
}

proc define_scene { args } {
  variable corner_bundle_applied
  # Passthrough parse: extract our key plus the base mode/liberty/spef keys
  # so the corner bundle is only injected when the user supplied none of
  # them (explicit args win over the bundle). Unparsed args stay for the
  # base cmd.
  parse_key_args "define_scene" args \
    keys {-analysis_corner -mode -liberty -liberty_min -liberty_max \
          -spef -spef_min -spef_max} \
    flags {} 0
  set corner "NULL"
  if { [info exists keys(-analysis_corner)] } {
    set corner [find_analysis_corner $keys(-analysis_corner)]
    if { $corner == "NULL" } {
      sta_error 3704 "$keys(-analysis_corner) is not the name of an analysis corner."
    }
  }

  # When the corner carries an SDC bundle, apply it to the (mode, corner)
  # overlay Sdc, once per pair. The mode itself is untouched: the overlay
  # holds only the corner-scoped constraints and the corner-scope guard
  # rejects commands that cannot be corner-scoped.
  set mode_name [cmd_mode_name]
  if { [info exists keys(-mode)] } {
    set mode_name $keys(-mode)
  }
  if { $corner != "NULL" } {
    apply_corner_sdc_bundle $mode_name $corner
  }
  if { [info exists keys(-mode)] } {
    lappend args -mode $mode_name
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
# Corner-scoped SDC: whitelisted commands write into the (mode, corner)
# overlay Sdc (timing derates, IO delays, clock uncertainty, clock
# latency/insertion).
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

# Commands that may not run in an analysis corner scope: every SDC write
# command that is not corner-scoped writes to the MODE Sdc, so it must
# error instead of silently escaping the corner scope. Covers structural
# constraints (clocks, exceptions, logic state) and mode-level
# environment/limits. NOTE: a new upstream SDC write command is unsafe in
# corner scope until added here or to the corner whitelist (Sdc.i
# cmdCornerSdc swaps).
variable corner_scope_disallowed_cmds {
  create_clock create_generated_clock set_clock_groups set_propagated_clock
  set_false_path set_multicycle_path set_max_delay set_min_delay
  group_path set_case_analysis set_disable_timing
  create_voltage_area set_clock_gating_check set_clock_sense set_sense
  set_clock_transition set_data_check set_drive set_driving_cell
  set_fanout_load set_ideal_latency set_ideal_network set_ideal_transition
  set_level_shifter_strategy set_level_shifter_threshold
  set_logic_dc set_logic_one set_logic_zero
  set_max_area set_max_capacitance set_max_dynamic_power set_max_fanout
  set_max_leakage_power set_max_time_borrow set_max_transition
  set_min_capacitance set_min_pulse_width set_operating_conditions
  set_port_fanout_number set_pvt set_resistance set_voltage
  delete_clock delete_generated_clock set_ideal_net set_path_margin
  set_wire_load_min_block_size set_wire_load_mode set_wire_load_model
  set_wire_load_selection_group
  unset_case_analysis unset_clock_groups unset_clock_transition
  unset_data_check unset_disable_timing unset_propagated_clock
  unset_path_exceptions
}
# Corner-scoped by design but not implemented yet.
variable corner_scope_pending_cmds {
  set_input_transition set_load
}

proc check_corner_scope { cmd } {
  variable corner_scope_pending_cmds
  # A build can embed this file without the analysis_corner swig commands
  # (OpenROAD links its own swig module); no corner scope can exist
  # there, so the guarded commands must pass through untouched.
  if { [info commands cmd_analysis_corner] == "" } {
    return
  }
  if { [cmd_analysis_corner] != "NULL" } {
    if { [lsearch -exact $corner_scope_pending_cmds $cmd] >= 0 } {
      sta_error 3711 "$cmd is not yet supported in an analysis corner scope."
    } else {
      sta_error 3710 "$cmd is not allowed in an analysis corner scope; it is a mode-level constraint."
    }
  }
}

# Guard the disallowed commands with a corner-scope check (runtime
# wrappers; the base commands are untouched). The base is invoked with
# uplevel/linsert rather than {*}$args because sdc_file_line list-parses
# the command text of every stack frame (Util.tcl).
foreach cmd [concat $corner_scope_disallowed_cmds $corner_scope_pending_cmds] {
  # Skip names with no command behind them (e.g. documented-only entries
  # like set_ideal_net, or commands removed upstream).
  if { [info commands $cmd] != ""
       && [info procs ${cmd}_corner_scope_base] == "" } {
    rename $cmd ${cmd}_corner_scope_base
    proc $cmd { args } \
      "check_corner_scope $cmd ; uplevel 1 \[linsert \$args 0 ::sta::${cmd}_corner_scope_base\]"
  }
}

# Extend the user-property commands to analysis_corner objects by wrapping
# the base procs at runtime (Property.tcl untouched, upstream merge
# hygiene). Non-corner arguments fall through to the base commands, invoked
# with uplevel/linsert rather than {*}$args because sdc_file_line
# list-parses the command text of every stack frame (Util.tcl).

if { [info procs define_property_corner_base] == "" } {
  rename define_property define_property_corner_base
}
proc define_property { args } {
  set idx [lsearch -exact $args -object_type]
  if { $idx >= 0
       && [lindex $args [expr {$idx + 1}]] == "analysis_corner" } {
    parse_key_args "define_property" args keys {-object_type -type} flags {}
    if { ![info exists keys(-type)] } {
      sta_error 3714 "define_property -type must be specified."
    }
    check_argc_eq1 "define_property" $args
    define_analysis_corner_property_cmd [lindex $args 0] $keys(-type)
  } else {
    uplevel 1 [linsert $args 0 ::sta::define_property_corner_base]
  }
}

if { [info procs set_property_corner_base] == "" } {
  rename set_property set_property_corner_base
}
proc set_property { args } {
  if { [llength $args] == 3
       && [is_object [lindex $args 0]]
       && [object_type [lindex $args 0]] == "AnalysisCorner" } {
    set_analysis_corner_property_cmd [lindex $args 0] [lindex $args 1] \
      [lindex $args 2]
  } else {
    uplevel 1 [linsert $args 0 ::sta::set_property_corner_base]
  }
}

if { [info procs get_object_property_corner_base] == "" } {
  rename get_object_property get_object_property_corner_base
}
proc get_object_property { object prop } {
  if { [is_object $object] && [object_type $object] == "AnalysisCorner" } {
    return [analysis_corner_property $object $prop]
  }
  return [get_object_property_corner_base $object $prop]
}

# Sta::clear() (behind clear_sta / clear_network) deletes every corner
# overlay Sdc; forget which SDC bundles were applied so scenes defined
# afterwards re-read them.
foreach cmd {clear_sta clear_network} {
  if { [info commands ${cmd}_corner_base] == "" } {
    rename $cmd ${cmd}_corner_base
    proc $cmd { args } \
      "variable corner_bundle_applied ; set corner_bundle_applied \[dict create\] ; uplevel 1 \[linsert \$args 0 ::sta::${cmd}_corner_base\]"
  }
}

# sta namespace end.
}
