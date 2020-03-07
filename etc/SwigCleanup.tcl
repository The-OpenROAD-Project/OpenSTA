#! /bin/sh
# The next line is executed by /bin/sh, but not Tcl \
exec tclsh $0 ${1+"$@"}

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

# Suppress compiler warnings in bletcherous swig generated code.

set swig_file [lindex $argv 0]
set backup_file "$swig_file.backup"

# Copy to the side before munging.
file rename -force $swig_file $backup_file

set in_stream [open $backup_file r]
set out_stream [open $swig_file w]

# include "Machine.hh" happens too late to define this gcc pragma.
puts $out_stream "#ifndef __GNUC__
#define __attribute__(x)
#endif"

# pragmas to suppress warnings
puts $out_stream "#define DIAG_STR(s) #s
#define DIAG_JOINSTR(x,y) DIAG_STR(x ## y)
#ifdef _MSC_VER
#define DIAG_DO_PRAGMA(x) __pragma (#x)
#define DIAG_PRAGMA(compiler,x) DIAG_DO_PRAGMA(warning(x))
#else
#define DIAG_DO_PRAGMA(x) _Pragma (#x)
#define DIAG_PRAGMA(compiler,x) DIAG_DO_PRAGMA(compiler diagnostic x)
#endif
#if defined(__clang__)
# define DISABLE_WARNING(gcc_unused,clang_option,msvc_unused) DIAG_PRAGMA(clang,push) DIAG_PRAGMA(clang,ignored DIAG_JOINSTR(-W,clang_option))
#elif defined(_MSC_VER)
# define DISABLE_WARNING(gcc_unused,clang_unused,msvc_errorcode) DIAG_PRAGMA(msvc,push) DIAG_DO_PRAGMA(warning(disable:##msvc_errorcode))
#elif defined(__GNUC__)
#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
# define DISABLE_WARNING(gcc_option,clang_unused,msvc_unused) DIAG_PRAGMA(GCC,push) DIAG_PRAGMA(GCC,ignored DIAG_JOINSTR(-W,gcc_option))
#else
# define DISABLE_WARNING(gcc_option,clang_unused,msvc_unused) DIAG_PRAGMA(GCC,ignored DIAG_JOINSTR(-W,gcc_option))
#endif
#endif

DISABLE_WARNING(cast-qual,cast-qual,00)
DISABLE_WARNING(missing-braces,missing-braces,00)
DISABLE_WARNING(pedantic,pedantic,00)
DISABLE_WARNING(missing-field-initializers,missing-field-initializers,00)
"

while { ! [eof $in_stream] } {
  gets $in_stream line
  puts $out_stream $line
}

close $in_stream

# Disable emacs syntax highlighting.
puts $out_stream "// Local Variables:
// mode:c++
// End:"

close $out_stream

file delete $backup_file
