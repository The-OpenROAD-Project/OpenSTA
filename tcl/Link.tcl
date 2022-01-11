# OpenSTA, Static Timing Analyzer
# Copyright (c) 2022, Parallax Software, Inc.
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

# Network commands.

namespace eval sta {

define_cmd_args "link_design" {[top_cell_name]}

proc_redirect link_design {
  variable current_design_name

  check_argc_eq0or1 "link_design" $args
  if { $args == "" } {
    set top_cell_name ""
  } else {
    set top_cell_name [lindex $args 0]
  }
  if { $top_cell_name == "" } {
    if { $current_design_name == "" } {
      sta_error 593 "missing top_cell_name argument and no current_design."
      return 0
    } else {
      set top_cell_name $current_design_name
    }
  }
  link_design_cmd $top_cell_name
}

# sta namespace end
}
