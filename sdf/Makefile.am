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

lib_LTLIBRARIES = libsdf.la

include_HEADERS = \
	ReportAnnotation.hh \
	Sdf.hh \
	SdfReader.hh \
	SdfWriter.hh

libsdf_la_SOURCES = \
	ReportAnnotation.cc \
	SdfReader.cc \
	SdfParse.yy \
	SdfLex.ll \
	SdfWriter.cc

SdfLex.ll: SdfParse.hh

SdfLex.cc: SdfLex.ll
	$(LEX) $(LFLAGS) -o SdfLex.cc --prefix=SdfLex_ --header-file=SdfLex.hh SdfLex.ll

# Rules to support automake pre 1.12 that name header .h instead of .hh
SdfParse.hh: SdfParse.cc
	if test -f SdfParse.h; then \
	  cp SdfParse.h SdfParse.hh; \
	fi

TCL_SRCS = \
	Sdf.i \
	Sdf.tcl

EXTRA_DIST = \
	$(TCL_SRCS) \
	SdfParse.hh

MAINTAINERCLEANFILES = \
	SdfParse.hh \
	SdfParse.cc \
	SdfLex.cc

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS) $(TCL_SRCS)
