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

define_cmd_args "define_analysis_corner" {name}

proc define_analysis_corner { args } {
  check_argc_eq1 "define_analysis_corner" $args
  define_analysis_corner_cmd [lindex $args 0]
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
  # Passthrough parse: unknown keys/values stay in args for define_scene_base.
  parse_key_args "define_scene" args keys {-analysis_corner} flags {} 0
  if { [info exists keys(-analysis_corner)] } {
    set corner [find_analysis_corner $keys(-analysis_corner)]
    if { $corner == "NULL" } {
      sta_error 3704 "$keys(-analysis_corner) is not the name of an analysis corner."
    }
  }
  define_scene_base {*}$args
  if { [info exists keys(-analysis_corner)] } {
    # Sta::makeScene leaves the new scene as cmd_scene_.
    set_scene_analysis_corner_cmd [cmd_scene] $corner
  }
}

# sta namespace end.
}
