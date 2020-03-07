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

#ifndef STA_WRITE_SPICE_H
#define STA_WRITE_SPICE_H

namespace sta {

// Write a spice deck for path.
// Throws FileNotReadable, FileNotWritable, SubcktEndsMissing
void
writeSpice(Path *path,
	   // Spice file written for path.
	   const char *spice_filename,
	   // Subckts used by path included in spice file.
	   const char *subckts_filename,
	   // File of all cell spice subckt definitions.
	   const char *lib_subckts_filename,
	   // Device model file included in spice file.
	   const char *models_filename,
	   StaState *sta);

} // namespace
#endif
