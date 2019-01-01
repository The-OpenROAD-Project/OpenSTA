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

bin_PROGRAMS = sta

include_HEADERS = \
	StaMain.hh

sta_SOURCES = \
	Main.cc \
	StaMain.cc \
	StaApp_wrap.cc \
	TclInitVar.cc

NETWORK_LIBS = \
	../verilog/libverilog.la

sta_DEPENDENCIES = $(NETWORK_LIBS) $(STA_LIBS) $(CUDD_LIBS)

sta_LDADD = $(NETWORK_LIBS) $(STA_LIBS) $(CUDD_LIBS)

StaApp_wrap.cc: $(SWIG_DEPEND) StaApp.i ../verilog/Verilog.i
	$(SWIG) $(SWIG_FLAGS) -namespace -prefix sta \
		-o StaApp_wrap.cc StaApp.i
	../etc/SwigCleanup.tcl StaApp_wrap.cc

# TCL files included as part of the executable.
# These files are encoded and shipped as part of the executable 
# so that they do not have to be installed on the client host.

TclInitVar.cc: ../etc/TclEncode.tcl $(TCL_INIT_FILES)
	../etc/TclEncode.tcl TclInitVar.cc "tcl_inits" \
		$(TCL_INIT_FILES) ../verilog/Verilog.tcl

EXTRA_DIST = \
	StaApp.i

# TclInitVar.cc is derived and TCL version specific, so don't dist it.
dist-hook:
	rm -rf $(distdir)/TclInitVar.cc

MAINTAINERCLEANFILES = \
	StaApp_wrap.cc \
	TclInitVar.cc

libs: $(sta_OBJECTS)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS)
