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

lib_LTLIBRARIES = libdcalc.la

include_HEADERS = \
	ArcDelayCalc.hh \
	Arnoldi.hh \
	ArnoldiDelayCalc.hh \
	ArnoldiReduce.hh \
	DelayCalc.hh \
	DcalcAnalysisPt.hh \
	DmpCeff.hh \
	DmpDelayCalc.hh \
	GraphDelayCalc.hh \
	GraphDelayCalc1.hh \
	LumpedCapDelayCalc.hh \
	NetCaps.hh \
	RCDelayCalc.hh \
	SimpleRCDelayCalc.hh \
	UnitDelayCalc.hh

libdcalc_la_SOURCES = \
	ArcDelayCalc.cc \
	ArnoldiDelayCalc.cc \
	ArnoldiReduce.cc \
	DcalcAnalysisPt.cc \
	DelayCalc.cc \
	DmpCeff.cc \
	DmpDelayCalc.cc \
	GraphDelayCalc.cc \
	GraphDelayCalc1.cc \
	LumpedCapDelayCalc.cc \
	NetCaps.cc \
	RCDelayCalc.cc \
	SimpleRCDelayCalc.cc \
	UnitDelayCalc.cc

TCL_SRCS = \
	DelayCalc.i \
	DelayCalc.tcl

EXTRA_DIST = \
	$(TCL_SRCS)

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS) $(TCL_SRCS)
