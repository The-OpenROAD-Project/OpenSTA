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
	MakeConcreteParasitics.hh \
	NullParasitics.hh \
	Parasitics.hh \
	ParasiticsClass.hh \
	ReadParasitics.hh \
	ReduceParasitics.hh \
	SpefNamespace.hh \
	SpefReader.hh \
	SpfReader.hh

libparasitics_la_SOURCES = \
	ConcreteParasitics.cc \
	EstimateParasitics.cc \
	NullParasitics.cc \
	Parasitics.cc \
	ReadParasitics.cc \
	ReduceParasitics.cc \
	SpefLex.ll \
	SpefNamespace.cc \
	SpefParse.yy \
	SpefReader.cc \
	SpefReaderPvt.hh \
	SpfLex.ll \
	SpfParse.yy \
	SpfReader.cc \
	SpfReaderPvt.hh \
	SpfSpefReader.cc \
	SpfSpefReader.hh

SpfLex.ll: SpfParse.hh

SpefLex.ll: SpefParse.hh

# Rules to support automake pre 1.12 that name header .h instead of .hh
SpfParse.hh: SpfParse.cc
	if test -f SpfParse.h; then \
	  cp  SpfParse.h SpfParse.hh; \
	fi

SpefParse.hh: SpefParse.cc
	if test -f SpefParse.h; then \
	  cp  SpefParse.h SpefParse.hh; \
	fi

TCL_SRCS = \
	Parasitics.i \
	Parasitics.tcl

EXTRA_DIST = \
	$(TCL_SRCS) \
	SpfParse.hh \
	SpefParse.hh

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS) $(TCL_SRCS)
