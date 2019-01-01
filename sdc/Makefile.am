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

lib_LTLIBRARIES = libsdc.la

include_HEADERS = \
	Clock.hh \
	ClockGatingCheck.hh \
	ClockGroups.hh \
	ClockInsertion.hh \
	ClockLatency.hh \
	CycleAccting.hh \
	DataCheck.hh \
	DeratingFactors.hh \
	DisabledPorts.hh \
	ExceptionPath.hh \
	InputDrive.hh \
	MinMaxValues.hh \
	PinPair.hh \
	PortDelay.hh \
	PortExtCap.hh \
	RiseFallMinMax.hh \
	RiseFallValues.hh \
	Sdc.hh \
	SdcClass.hh \
	SdcCmdComment.hh \
	WriteSdc.hh \
	WriteSdcPvt.hh

libsdc_la_SOURCES = \
	Clock.cc \
	ClockGatingCheck.cc \
	ClockGroups.cc \
	ClockInsertion.cc \
	ClockLatency.cc \
	CycleAccting.cc \
	DataCheck.cc \
	DeratingFactors.cc \
	DisabledPorts.cc \
	ExceptionPath.cc \
	InputDrive.cc \
	PinPair.cc \
	PortDelay.cc \
	PortExtCap.cc \
	RiseFallMinMax.cc \
	RiseFallValues.cc \
	Sdc.cc \
	SdcCmdComment.cc \
	WriteSdc.cc

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS)
