# OpenSTA, Static Timing Analyzer
# Copyright (c) 2024, Parallax Software, Inc.
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

namespace eval sta {

define_cmd_args "report_dcalc" \
  {[-from from_pin] [-to to_pin] [-corner corner] [-min] [-max] [-digits digits]}

proc_redirect report_dcalc {
  report_dcalc_cmd "report_dcalc" $args "-digits"
}

# Allow any combination of -from/-to pins.
proc report_dcalc_cmd { cmd cmd_args digits_key } {
  global sta_report_default_digits

  parse_key_args $cmd cmd_args \
    keys "$digits_key -from -to -corner" \
    flags {-min -max}
  set corner [parse_corner keys]
  set min_max [parse_min_max_flags flags]
  check_argc_eq0 $cmd $cmd_args

  set digits $sta_report_default_digits
  if [info exists keys($digits_key)] {
    set digits $keys($digits_key)
    check_positive_integer $digits_key $digits
  }
  if {[info exists keys(-from)] && [info exists keys(-to)]} {
    set from_pin [get_port_pin_error "from_pin" $keys(-from)]
    set to_pin [get_port_pin_error "to_pin" $keys(-to)]
    foreach from_vertex [$from_pin vertices] {
      foreach to_vertex [$to_pin vertices] {
	set iter [$from_vertex out_edge_iterator]
	while {[$iter has_next]} {
	  set edge [$iter next]
	  if { [$edge to] == $to_vertex } {
	    report_edge_dcalc $edge $corner $min_max $digits
	  }
	}
	$iter finish
      }
    }
  } elseif [info exists keys(-from)] {
    set from_pin [get_port_pin_error "from_pin" $keys(-from)]
    foreach from_vertex [$from_pin vertices] {
      set iter [$from_vertex out_edge_iterator]
      while {[$iter has_next]} {
	set edge [$iter next]
	report_edge_dcalc $edge $corner $min_max $digits
      }
      $iter finish
    }
  } elseif [info exists keys(-to)] {
    set to_pin [get_port_pin_error "to_pin" $keys(-to)]
    foreach to_vertex [$to_pin vertices] {
      set iter [$to_vertex in_edge_iterator]
      while {[$iter has_next]} {
	set edge [$iter next]
	report_edge_dcalc $edge $corner $min_max $digits
      }
      $iter finish
    }
  }
}

proc report_edge_dcalc { edge corner min_max digits } {
  set role [$edge role]
  if { $role != "wire" } {
    set from_vertex [$edge from]
    set from_pin [$from_vertex pin]
    set to_vertex [$edge to]
    set to_pin [$to_vertex pin]
    set cell [[$to_pin instance] cell]
    set library [$cell library]
    # Filter timing checks based on min_max.
    if {!(($min_max == "max" && $role == "hold") \
	    || ($min_max=="min" && $role=="setup"))} {
      report_line "Library: [get_name $library]"
      report_line "Cell: [get_name $cell]"
      set sense [$edge sense]
      if { $sense != "unknown" } {
        report_line "Arc sense: $sense"
      }
      report_line "Arc type: $role"

      foreach arc [$edge timing_arcs] {
	set from [get_name [$from_pin port]]
	set from_rf [$arc from_edge]
	set to [get_name [$to_pin port]]
	set to_rf [$arc to_edge]
	report_line "$from $from_rf -> $to $to_rf"
        report_line [report_delay_calc_cmd $edge $arc $corner $min_max $digits]
	if { [$edge delay_annotated $arc $corner $min_max] } {
	  set delay [$edge arc_delay $arc $corner $min_max]
	  report_line "Annotated value = [format_time $delay $digits]"
	}
	report_line "............................................."
	report_line ""
      }
    }
  }
}

################################################################

define_hidden_cmd_args "set_delay_calculator" [delay_calc_names]

proc set_delay_calculator { alg } {
  if { [is_delay_calc_name $alg] } {
    set_delay_calculator_cmd $alg
  } else {
    sta_error 180 "delay calculator $alg not found."
  }
}

define_cmd_args "set_pocv_sigma_factor" { factor }

################################################################

define_cmd_args "set_assigned_delay" \
  {-cell|-net [-rise] [-fall] [-corner corner] [-min] [-max]\
     [-from from_pins] [-to to_pins] delay}

# Change the delay for timing arcs between from_pins and to_pins matching
# on cell (instance) or net.
proc set_assigned_delay { args } {
  parse_key_args "set_assigned_delay" args keys {-corner -from -to} \
    flags {-cell -net -rise -fall -max -min}
  check_argc_eq1 "set_assigned_delay" $args
  set corner [parse_corner keys]
  set min_max [parse_min_max_all_check_flags flags]
  set to_rf [parse_rise_fall_flags flags]

  if [info exists keys(-from)] {
    set from_pins [get_port_pins_error "from_pins" $keys(-from)]
  } else {
    sta_error 181 "set_assigned_delay missing -from argument."
  }
  if [info exists keys(-to)] {
    set to_pins [get_port_pins_error "to_pins" $keys(-to)]
  } else {
    sta_error 182 "set_assigned_delay missing -to argument."
  }

  set delay [lindex $args 0]
  if {![string is double $delay]} {
    sta_error 183 "set_assigned_delay delay is not a float."
  }
  set delay [time_ui_sta $delay]

  if {[info exists flags(-cell)] && [info exists flags(-net)]} {
    sta_error 184 "set_annotated_delay -cell and -net options are mutually excluive."
  } elseif {[info exists flags(-cell)]} {
    if { $from_pins != {} } {
      set inst [[lindex $from_pins 0] instance]
      foreach pin $from_pins {
	if {[$pin instance] != $inst} {
	  sta_error 185 "set_assigned_delay pin [get_full_name $pin] is not attached to instance [get_full_name $inst]."
	}
      }
      foreach pin $to_pins {
	if {[$pin instance] != $inst} {
	  sta_error 186 "set_assigned_delay pin [get_full_name $pin] is not attached to instance [get_full_name $inst]"
	}
      }
    }
  } elseif {![info exists flags(-net)]} {
    sta_error 187 "set_assigned_delay -cell or -net required."
  }
  foreach from_pin $from_pins {
    set from_vertices [$from_pin vertices]
    set_assigned_delay1 [lindex $from_vertices 0] \
      $to_pins $to_rf $corner $min_max $delay
    if { [llength $from_vertices] == 2 } {
      set_assigned_delay1 [lindex $from_vertices 1] \
	$to_pins $to_rf $corner $min_max $delay
    }
  }
}

proc set_assigned_delay1 { from_vertex to_pins to_rf corner min_max delay } {
  foreach to_pin $to_pins {
    set to_vertices [$to_pin vertices]
    set_assigned_delay2 $from_vertex [lindex $to_vertices 0] \
      $to_rf $corner $min_max $delay
    if { [llength $to_vertices] == 2 } {
      # Bidirect driver.
      set_assigned_delay2 $from_vertex [lindex $to_vertices 1] \
	$to_rf $corner $min_max $delay
    }
  }
}

proc set_assigned_delay2 {from_vertex to_vertex to_rf corner min_max delay} {
  set matched 0
  set edge_iter [$from_vertex out_edge_iterator]
  while {[$edge_iter has_next]} {
    set edge [$edge_iter next]
    if { [$edge to] == $to_vertex \
	   && ![timing_role_is_check [$edge role]] } {
      foreach arc [$edge timing_arcs] {
	if { $to_rf == "rise_fall" \
	       || $to_rf eq [$arc to_edge_name] } {
	  set_arc_delay $edge $arc $corner $min_max $delay
          set matched 1
	}
      }
    }
  }
  $edge_iter finish
  if { !$matched } {
    sta_error 193 "set_assigned_delay no timing arcs found between from/to pins."
  }
}

################################################################

define_cmd_args "set_assigned_check" \
  {-setup|-hold|-recovery|-removal [-rise] [-fall]\
     [-corner corner] [-min] [-max]\
     [-from from_pins] [-to to_pins] [-clock rise|fall]\
     [-cond sdf_cond] check_value}

proc set_assigned_check { args } {
  parse_key_args "set_assigned_check" args \
    keys {-from -to -corner -clock -cond} \
    flags {-setup -hold -recovery -removal -rise -fall -max -min}
  check_argc_eq1 "set_assigned_check" $args

  if { [info exists keys(-from)] } {
    set from_pins [get_port_pins_error "from_pins" $keys(-from)]
  } else {
    sta_error 188 "set_assigned_check missing -from argument."
  }
  set from_rf "rise_fall"
  if { [info exists keys(-clock)] } {
    set clk_arg $keys(-clock)
    if { $clk_arg eq "rise" \
	   || $clk_arg eq "fall" } {
      set from_rf $clk_arg
    } else {
      sta_error 189 "set_assigned_check -clock must be rise or fall."
    }
  }

  if { [info exists keys(-to)] } {
    set to_pins [get_port_pins_error "to_pins" $keys(-to)]
  } else {
    sta_error 190 "set_assigned_check missing -to argument."
  }
  set to_rf [parse_rise_fall_flags flags]
  set corner [parse_corner keys]
  set min_max [parse_min_max_all_check_flags flags]

  if { [info exists flags(-setup)] } {
    set role "setup"
  } elseif { [info exists flags(-hold)] } {
    set role "hold"
  } elseif { [info exists flags(-recovery)] } {
    set role "recovery"
  } elseif { [info exists flags(-removal)] } {
    set role "removal"
  } else {
    sta_error 191 "set_assigned_check missing -setup|-hold|-recovery|-removal check type.."
  }
  set cond ""
  if { [info exists key(-cond)] } {
    set cond $key(-cond)
  }
  set check_value [lindex $args 0]
  if { ![string is double $check_value] } {
    sta_error 192 "set_assigned_check check_value is not a float."
  }
  set check_value [time_ui_sta $check_value]

  foreach from_pin $from_pins {
    set from_vertices [$from_pin vertices]
    set_assigned_check1 [lindex $from_vertices 0] $from_rf \
      $to_pins $to_rf $role $corner $min_max $cond $check_value
    if { [llength $from_vertices] == 2 } {
      set_assigned_check1 [lindex $from_vertices 1] $from_rf \
	$to_pins $to_rf $role $corner $min_max $cond $check_value
    }
  }
}

proc set_assigned_check1 { from_vertex from_rf to_pins to_rf \
			     role corner min_max cond check_value } {
  foreach to_pin $to_pins {
    set to_vertices [$to_pin vertices]
    set_assigned_check2 $from_vertex $from_rf [lindex $to_vertices 0] \
      $to_rf $role $corner $min_max $cond $check_value
    if { [llength $to_vertices] == 2 } {
      # Bidirect driver.
      set_assigned_check2 $from_vertex $from_rf \
	[lindex $to_vertices 1] $to_rf $role $corner $min_max \
	$cond $check_value
    }
  }
}

proc set_assigned_check2 { from_vertex from_rf to_vertex to_rf \
			     role corner min_max cond check_value } {
  set edge_iter [$from_vertex out_edge_iterator]
  set matched 0
  while {[$edge_iter has_next]} {
    set edge [$edge_iter next]
    if { [$edge to] == $to_vertex } {
      foreach arc [$edge timing_arcs] {
	if { ($from_rf eq "rise_fall" \
		|| $from_rf eq [$arc from_edge_name]) \
	       && ($to_rf eq "rise_fall" \
		     || $to_rf eq [$arc to_edge_name]) \
	       && [$arc role] eq $role \
	       && ($cond eq "" || [$arc sdf_cond] eq $cond) } {
	  set_arc_delay $edge $arc $corner $min_max $check_value
          set matched 1
	}
      }
    }
  }
  $edge_iter finish
  if { !$matched } {
    sta_error 194 "set_assigned_check no check arcs found between from/to pins."
  }
}

################################################################a

define_cmd_args "set_assigned_transition" \
  {[-rise] [-fall] [-corner corner] [-min] [-max] slew pins}

# Change the slew on a list of ports.
proc set_assigned_transition { args } {
  parse_key_args "set_assigned_transition" args keys {-corner} \
    flags {-rise -fall -max -min}

  set corner [parse_corner keys]
  set min_max [parse_min_max_all_check_flags flags]
  set tr [parse_rise_fall_flags flags]
  check_argc_eq2 "set_assigned_transition" $args

  set slew [lindex $args 0]
  if {![string is double $slew]} {
    sta_error 210 "set_assigned_transition transition is not a float."
  }
  set slew [time_ui_sta $slew]
  set pins [get_port_pins_error "pins" [lindex $args 1]]
  foreach pin $pins {
    set vertices [$pin vertices]
    set vertex [lindex $vertices 0]
    set_annotated_slew $vertex $corner $min_max $tr $slew
    if { [llength $vertices] == 2 } {
      # Bidirect driver.
      set vertex [lindex $vertices 1]
      set_annotated_slew $vertex $min_max $tr $slew
    }
  }
}

# sta namespace end
}
