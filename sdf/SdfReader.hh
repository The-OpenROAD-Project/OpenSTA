// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

namespace sta {

class Report;
class MinMax;
class Network;
class Graph;
class Corner;

// Sdf index is:
//  sdf_min = 0
//  sdf_typ = 1
//  sdf_max = 2
//
// If unescaped_dividers is true, path names in the SDF do not have to
// escape hierarchy dividers when the path name is escaped.  For
// example, the escaped Verilog instance name "\inst1/inst2 " can be
// referenced as "inst1/inst2".  The correct SDF name is
// "inst1\/inst2", since the divider does not represent a change in
// hierarchy in this case.
//
// If incremental_only is true non-incremental annoatations are ignored.
//
// path is a hierararchial path prefix for instances and pins in the
// sdf file.  Pass 0 (nullptr) to specify no path.
//
// The cond_use option is used when the SDF file contains conditional
// delays and the library does not have conditional delay arcs.  If
// cond_use is min, the minimum of all conditional delays is used.  If
// cond_use is max, the maximum of all conditional delays is used.  If
// cond_use is min_max and min_max operating conditions are in use, the
// minimum of the conditional delay values is used for minimum operating
// conditions and the maximum of the conditional delay values is used for
// maximum operating conditions.

// Read sdf_index value from sdf triples.
bool
readSdfSingle(const char *filename,
	      const char *path,
              Corner *corner,
	      int sdf_index,
	      AnalysisType analysis_type,
	      bool unescaped_dividers,
	      bool incremental_only,
              MinMaxAll *cond_use,
	      StaState *sta);

// Read sdf_min_index and sdf_max_index values from sdf triples.
bool
readSdfMinMax(const char *filename,
	      const char *path,
	      Corner *corner,
              int sdf_min_index,
	      int sdf_max_index,
	      AnalysisType analysis_type,
	      bool unescaped_dividers,
	      bool incremental_only,
              MinMaxAll *cond_use,
	      StaState *sta);

} // namespace
