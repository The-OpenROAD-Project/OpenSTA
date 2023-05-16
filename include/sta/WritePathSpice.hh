// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <string>
#include <set>

namespace sta {

using std::string;
using std::set;

typedef set<string> StdStringSet;

class Path;
class StaState;

// Write a spice deck for path.
// Throws FileNotReadable, FileNotWritable, SubcktEndsMissing
void
writePathSpice(Path *path,
	       // Spice file written for path.
	       const char *spice_filename,
	       // Subckts used by path included in spice file.
	       const char *subckt_filename,
	       // File of all cell spice subckt definitions.
	       const char *lib_subckt_filename,
	       // Device model file included in spice file.
	       const char *model_filename,
	       // Nets off of path to include in the spice run.
               StdStringSet *off_path_pin_names,
               const char *power_name,
	       const char *gnd_name,
	       StaState *sta);

} // namespace
