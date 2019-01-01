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

ACLOCAL_AMFLAGS = -I m4

# app has to follow all library subdirs
SUBDIRS = $(STA_SUBDIRS)

libs:
	for subdir in $(SUBDIRS); do \
	  (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) libs); \
	done

include_HEADERS = config.h

# The automake target uses include directives, which don't work in xemacs.
xtags:
	rm -rf TAGS; \
	for subdir in $(SUBDIRS); do \
	  (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) xtags); \
	done

