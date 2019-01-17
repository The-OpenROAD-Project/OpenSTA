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

lib_LTLIBRARIES = libparasitics.la

include_HEADERS = \
	ConcreteParasitics.hh \
	ConcreteParasiticsPvt.hh \
	EstimateParasitics.hh \
	NullParasitics.hh \
	Parasitics.hh \
	ParasiticsClass.hh \
	ReduceParasitics.hh \
	SpefNamespace.hh \
	SpefReader.hh

libparasitics_la_SOURCES = \
	ConcreteParasitics.cc \
	EstimateParasitics.cc \
	NullParasitics.cc \
	Parasitics.cc \
	ReduceParasitics.cc \
	SpefLex.ll \
	SpefNamespace.cc \
	SpefParse.yy \
	SpefReader.cc \
	SpefReaderPvt.hh \
	SpfReaderPvt.hh

SpefLex.ll: SpefParse.hh

SpefLex.cc: SpefLex.ll
	$(LEX) $(LFLAGS) -o SpefLex.cc --prefix=SpefLex_ --header-file=SpefLex.hh SpefLex.ll

SpefParse.hh: SpefParse.cc
	if test -f SpefParse.h; then \
	  cp  SpefParse.h SpefParse.hh; \
	fi

TCL_SRCS = \
	Parasitics.i \
	Parasitics.tcl

EXTRA_DIST = \
	$(TCL_SRCS) \
	SpefParse.hh

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS) $(TCL_SRCS)
