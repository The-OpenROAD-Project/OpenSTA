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

lib_LTLIBRARIES = libliberty.la

include_HEADERS = \
	EquivCells.hh \
	FuncExpr.hh \
	InternalPower.hh \
	LeakagePower.hh \
	Liberty.hh \
	LibertyBuilder.hh \
	LibertyClass.hh \
	LibertyParser.hh \
	LibertyReader.hh \
	LibertyReaderPvt.hh \
	LinearModel.hh \
	Sequential.hh \
	TableModel.hh \
	TimingArc.hh \
	TimingModel.hh \
	TimingRole.hh \
	Transition.hh \
	Units.hh \
	Wireload.hh

libliberty_la_SOURCES = \
	EquivCells.cc \
	FuncExpr.cc \
	InternalPower.cc \
	LeakagePower.cc \
	Liberty.cc \
	LibertyBuilder.cc \
	LibertyExpr.cc \
	LibertyExpr.hh \
	LibertyExprLex.ll \
	LibertyExprParse.yy \
	LibertyExprPvt.hh \
	LibertyLex.ll \
	LibertyParse.yy \
	LibertyParser.cc \
	LibertyReader.cc \
	LinearModel.cc \
	Sequential.cc \
	TableModel.cc \
	TimingArc.cc \
	TimingRole.cc \
	Transition.cc \
	Units.cc \
	Wireload.cc

LibertyExprLex.ll: LibertyExprParse.hh

LibertyExprLex.cc: LibertyExprLex.ll
	$(LEX) $(LFLAGS) -o LibertyExprLex.cc --prefix=LibertyExprLex_ --header-file=LibertyExprLex.hh LibertyExprLex.ll

LibertyLex.cc: LibertyLex.ll
	$(LEX) $(LFLAGS) -o LibertyLex.cc --prefix=LibertyLex_ --header-file=LibertyLex.hh LibertyLex.ll

LibertyLex.ll: LibertyParse.hh

# Rules to support automake pre 1.12 that name header .h instead of .hh
LibertyExprParse.hh: LibertyExprParse.cc
	if test -f LibertyExprParse.h; then \
	  cp LibertyExprParse.h LibertyExprParse.hh; \
	fi

LibertyParse.hh: LibertyParse.cc
	if test -f LibertyParse.h; then \
	  cp LibertyParse.h LibertyParse.hh; \
	fi

EXTRA_DIST = \
	LibertyExt.cc \
	LibertyParse.hh \
	LibertyExprParse.hh

MAINTAINERCLEANFILES = \
	LibertyLex.cc \
	LibertyParse.hh \
	LibertyParse.cc \
	LibertyExprLex.cc \
	LibertyExprParse.hh \
	LibertyExprParse.cc

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS)

