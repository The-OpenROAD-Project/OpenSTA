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

# Trim tclreadline::Loop's eval frames from the end of $errorInfo.
proc ::sta::trim_tclreadline_error_info { args } {
  global errorInfo
  set marker "\n    (\"eval\" body line "
  set idx [string last $marker $errorInfo]
  if { $idx != -1 } {
    set errorInfo [string range $errorInfo 0 [expr { $idx - 1 }]]
  }
}

proc init_sta_cmds {} {
  global auto_index

  # Import exported commands from sta namespace to global namespace.
  namespace import sta::*

  if { [info exists tclreadline::version] } {
    history
    history event
    eval $auto_index(::tclreadline::ScriptCompleter)
    ::tclreadline::readline builtincompleter true
    ::tclreadline::readline customcompleter ::tclreadline::ScriptCompleter
    proc ::tclreadline::prompt1 {} { return {% } }
    proc ::tclreadline::prompt2 {} { return {> } }
    # tclreadline::Setup does catch {rename ::tclreadline::Exit ""}.
    # If Exit does not exist that catch fails and overwrites $errorInfo,
    # hiding any traceback from errors in the .sta init file.
    proc ::tclreadline::Exit { args } {}
    # Drop the interactive Loop's "eval $::tclreadline::LINE" frames
    # that otherwise trail every command's $errorInfo.
    trace add variable ::tclreadline::errorMsg write \
      ::sta::trim_tclreadline_error_info
  }
}
