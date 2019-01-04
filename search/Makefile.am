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

lib_LTLIBRARIES = libsearch.la

include_HEADERS = \
	Bfs.hh \
	CheckMaxSkews.hh \
	CheckMinPeriods.hh \
	CheckMinPulseWidths.hh \
	CheckSlewLimits.hh \
	CheckTiming.hh \
	ClkInfo.hh \
	ClkSkew.hh \
	Corner.hh \
	Crpr.hh \
	FindRegister.hh \
	GatedClk.hh \
	Genclks.hh \
	Latches.hh \
	Levelize.hh \
	Path.hh \
	PathAnalysisPt.hh \
	PathEnd.hh \
	PathEnum.hh \
	PathEnumed.hh \
	PathExpanded.hh \
	PathRef.hh \
	PathGroup.hh \
	PathVertex.hh \
	PathVertexRep.hh \
	Power.hh \
	Property.hh \
	ReportPath.hh \
	Search.hh \
	SearchClass.hh \
	SearchPred.hh \
	Sim.hh \
	Sta.hh \
	StaState.hh \
	Tag.hh \
	TagGroup.hh \
	VertexVisitor.hh \
	VisitPathEnds.hh \
	VisitPathGroupVertices.hh \
	WorstSlack.hh \
	WritePathSpice.hh

libsearch_la_SOURCES = \
	Bfs.cc \
	CheckMaxSkews.cc \
	CheckMinPeriods.cc \
	CheckMinPulseWidths.cc \
	CheckSlewLimits.cc \
	CheckTiming.cc \
	ClkInfo.cc \
	ClkSkew.cc \
	Corner.cc \
	Crpr.cc \
	FindRegister.cc \
	GatedClk.cc \
	Genclks.cc \
	Latches.cc \
	Levelize.cc \
	Path.cc \
	PathAnalysisPt.cc \
	PathEnd.cc \
	PathEnum.cc \
	PathEnumed.cc \
	PathExpanded.cc \
	PathGroup.cc \
	PathRef.cc \
	PathVertex.cc \
	PathVertexRep.cc \
	Power.cc \
	Property.cc \
	ReportPath.cc \
	Search.cc \
	SearchPred.cc \
	Sim.cc \
	Sta.cc \
	StaState.cc \
	Tag.cc \
	TagGroup.cc \
	VertexVisitor.cc \
	VisitPathEnds.cc \
	VisitPathGroupVertices.cc \
	WorstSlack.cc \
	WritePathSpice.cc

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS)
