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

lib_LTLIBRARIES = libnetwork.la

include_HEADERS = \
	ConcreteLibrary.hh \
	ConcreteNetwork.hh \
	HpinDrvrLoad.hh \
	MakeConcreteNetwork.hh \
	Network.hh \
	NetworkClass.hh \
	NetworkCmp.hh \
	ParseBus.hh \
	PortDirection.hh \
	SdcNetwork.hh \
	VerilogNamespace.hh

libnetwork_la_SOURCES = \
	ConcreteLibrary.cc \
	ConcreteNetwork.cc \
	HpinDrvrLoad.cc \
	Network.cc \
	NetworkCmp.cc \
	ParseBus.cc \
	PortDirection.cc \
	SdcNetwork.cc \
	VerilogNamespace.cc

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS)
