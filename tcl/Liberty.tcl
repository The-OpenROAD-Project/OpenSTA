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

# Liberty commands.

namespace eval sta {

define_cmd_args "read_liberty" \
  {[-corner corner_name] [-min] [-max] [-no_latch_infer] filename}

proc_redirect read_liberty {
  parse_key_args "read_liberty" args keys {-corner} \
    flags {-min -max -no_latch_infer}
  check_argc_eq1 "read_liberty" $args

  set filename [file nativename $args]
  set corner [parse_corner keys]
  set min_max [parse_min_max_all_flags flags]
  set infer_latches [expr ![info exists flags(-no_latch_infer)]]
  read_liberty_cmd $filename $corner $min_max $infer_latches
}

# sta namespace end
}
