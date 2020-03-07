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
      puts "Library: [get_name $library]"
      puts "Cell: [get_name $cell]"
      puts "Arc sense: [$edge sense]"
      puts "Arc type: $role"

      set arc_iter [$edge timing_arc_iterator]
      while {[$arc_iter has_next]} {
	set arc [$arc_iter next]
	set from [get_name [$from_pin port]]
	set from_rf [$arc from_trans]
	set to [get_name [$to_pin port]]
	set to_rf [$arc to_trans]
	puts "$from $from_rf -> $to $to_rf"
	puts -nonewline [report_delay_calc_cmd $edge $arc $corner $min_max $digits]
	if { [$edge delay_annotated $arc $corner $min_max] } {
	  set delay [$edge arc_delay $arc $corner $min_max]
	  puts "Annotated value = [format_time $delay $digits]"
	}
	puts ""
	puts "............................................."
	puts ""
      }
      $arc_iter finish
    }
  }
}

################################################################

define_hidden_cmd_args "set_delay_calculator" [delay_calc_names]

proc set_delay_calculator { alg } {
  if { [is_delay_calc_name $alg] } {
    set_delay_calculator_cmd $alg
  } else {
    sta_error "delay calculator $alg not found."
  }
}

# sta namespace end
}
