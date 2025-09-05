# OpenSTA, Static Timing Analyzer
# Copyright (c) 2025, Parallax Software, Inc.
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
# 
# The origin of this software must not be misrepresented; you must not
# claim that you wrote the original software.
# 
# Altered source versions must be plainly marked as such, and must not be
# misrepresented as being the original software.
# 
# This notice may not be removed or altered from any source distribution.

# Regression variables.

# Application program to run tests on.
set app "sta"
set sta_dir [file dirname $test_dir]
set app_path [file join $sta_dir "build" $app]
# Application options.
set app_options "-no_init -no_splash -exit"
# Log files for each test are placed in result_dir.
set result_dir [file join $test_dir "results"]
# Collective diffs.
set diff_file [file join $result_dir "diffs"]
# File containing list of failed tests.
set failure_file [file join $result_dir "failures"]
# Use the DIFF_OPTIONS envar to change the diff options
# (Solaris diff doesn't support this envar)
set diff_options "-c"
if [info exists env(DIFF_OPTIONS)] {
  set diff_options $env(DIFF_OPTIONS)
}

set valgrind_suppress [file join $test_dir valgrind.suppress]
set valgrind_options "--num-callers=20 --leak-check=full --freelist-vol=100000000 --leak-resolution=high --suppressions=$valgrind_suppress"
if { [exec "uname"] == "Darwin" } {
  append valgrind_options " --dsymutil=yes"
}

proc cleanse_logfile { test log_file } {
  # Nothing to be done here.
}

################################################################

# Record a test in the regression suite.
proc record_test { test cmd_dir } {
  global cmd_dirs test_groups
  set cmd_dirs($test) $cmd_dir
  lappend test_groups(all) $test
  return $test
}

# Record tests in the $STA/test directory.
proc record_sta_tests { tests } {
  global test_dir
  foreach test $tests {
    # Prune commented tests from the list.
    if { [string index $test 0] != "#" } {
      record_test $test $test_dir
    }
  }
}

# Record tests in the $STA/examples directory.
proc record_example_tests { tests } {
  global test_dir test_groups
  set example_dir [file join $test_dir ".." "examples"]
  foreach test $tests {
    # Prune commented tests from the list.
    if { [string index $test 0] != "#" } {
      record_test $test $example_dir
    }
  }
}

################################################################

proc define_test_group { name tests } {
  global test_groups
  set test_groups($name) $tests
}

proc group_tests { name } {
  global test_groups
  return $test_groups($name)
}

# Clear the test lists.
proc clear_tests {} {
  global test_groups
  unset test_groups
}

proc list_delete { list delete } {
  set result {}
  foreach item $list {
    if { [lsearch $delete $item] == -1 } {
      lappend result $item
    }
  }
  return $result
}

################################################################

# Regression test lists.

# Record tests in sta/examples
record_example_tests {
  delay_calc
  min_max_delays
  multi_corner
  power
  power_vcd
  sdf_delays
  spef_parasitics
}

record_sta_tests {
  get_filter
  get_is_memory
  get_lib_pins_of_objects
  get_noargs
  get_objrefs
  liberty_arcs_one2one_1
  liberty_arcs_one2one_2
  liberty_backslash_eol
  liberty_ccsn
  liberty_float_as_str
  liberty_latch3
  path_group_names
  prima3
  report_checks_src_attr
  report_json1
  report_json2
  suppress_msg
  verilog_attribute
  report_checks_sorted
}

define_test_group fast [group_tests all]
