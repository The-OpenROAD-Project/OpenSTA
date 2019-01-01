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

lib_LTLIBRARIES = libutil.la

include_HEADERS = \
	Condition.hh \
	Debug.hh \
	DisallowCopyAssign.hh \
	Error.hh \
	Fuzzy.hh \
	Hash.hh \
	HashSet.hh \
	HashMap.hh \
	Iterator.hh \
	Machine.hh \
	Map.hh \
	MinMax.hh \
	Mutex.hh \
	ObjectIndex.hh \
	PatternMatch.hh \
	Pthread.hh \
	Pool.hh \
	ReadWriteLock.hh \
	Report.hh \
	ReportStd.hh \
	ReportTcl.hh \
	Set.hh \
	Stats.hh \
	StringSeq.hh \
	StringSet.hh \
	StringUtil.hh \
	Thread.hh \
	ThreadException.hh \
	ThreadForEach.hh \
	ThreadPool.hh \
	ThreadWorker.hh \
	TokenParser.hh \
	UnorderedMap.hh \
	Vector.hh \
	Zlib.hh

libutil_la_SOURCES = \
	Condition.cc \
	Debug.cc \
	Error.cc \
	Fuzzy.cc \
	Machine.cc \
	MinMax.cc \
	Mutex.cc \
	PatternMatch.cc \
	Pthread.cc \
	ReadWriteLock.cc \
	Report.cc \
	ReportStd.cc \
	ReportTcl.cc \
	Stats.cc \
	StringSeq.cc \
	StringSet.cc \
	StringUtil.cc \
	Thread.cc \
	ThreadException.cc \
	ThreadPool.cc \
	ThreadWorker.cc \
	TokenParser.cc

libs: $(lib_LTLIBRARIES)

xtags: $(SOURCES) $(HEADERS)
	etags -a -o ../TAGS $(SOURCES) $(HEADERS)
