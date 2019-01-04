# OpenSTA, Static Timing Analyzer
# Copyright (c) 2019, Parallax Software, Inc.
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

lib_LTLIBRARIES = libverilog.la

include_HEADERS = \
	Verilog.hh \
	VerilogReader.hh

libverilog_la_SOURCES = \
	VerilogLex.ll \
	VerilogParse.yy \
	VerilogReader.cc

VerilogLex.ll: VerilogParse.hh

VerilogLex.cc: VerilogLex.ll
	$(LEX) $(LFLAGS) -o VerilogLex.cc --prefix=VerilogLex_ --header-file=VerilogLex.hh VerilogLex.ll

# Rules to support automake pre 1.12 that name header .h instead of .hh
VerilogParse.hh: VerilogParse.cc
	if test -f VerilogParse.h; then \
	  cp VerilogParse.h VerilogParse.hh; \
	fi

EXTRA_DIST = \
	VerilogParse.hh \
	Verilog.i \
	Verilog.tcl

MAINTAINERCLEANFILES = \
	VerilogParse.hh \
	VerilogParse.cc \
	VerilogLex.cc

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS)
