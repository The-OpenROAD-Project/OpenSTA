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

lib_LTLIBRARIES = libgraph.la

include_HEADERS = \
	Delay.hh \
	DelayFloat.hh \
	DelayNormal2.hh \
	Graph.hh \
	GraphClass.hh \
	GraphCmp.hh

libgraph_la_SOURCES = \
	Delay.cc \
	DelayFloat.cc \
	DelayNormal2.cc \
	Graph.cc \
	GraphCmp.cc

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS)
