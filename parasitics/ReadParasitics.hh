// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

namespace sta {

#include "ParasiticsClass.hh"

class Report;

// Read a Spf or Spef parasitics file that may be compressed with gzip.
// Spf or Spef single value parasitics are read into analysis point ap.
// In a Spef file with triplet values the first value is used.
// If reduce_to is specified detailed parasitics are reduced.
// If delete_after_reduce is specified the detailed parasitics are
// deleted after they are reduced.
// Constraint min/max cnst_min_max and operating condition op_cond
// are used for parasitic network reduction.
// Return true if successful.
bool
readParasiticsFile(const char *filename,
		   Instance *instance,
		   ParasiticAnalysisPt *ap,
		   bool increment,
		   bool pin_cap_included,
		   bool keep_coupling_caps,
		   float coupling_cap_factor,
		   ReduceParasiticsTo reduce_to,
		   bool delete_after_reduce,
		   const OperatingConditions *op_cond,
		   const Corner *corner,
		   const MinMax *cnst_min_max,
		   bool save,
		   bool quiet,
		   Report *report,
		   Network *network,
		   Parasitics *parasitics);

} // namespace
