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

#ifndef STA_EQUIV_CELLS_H
#define STA_EQUIV_CELLS_H

#include "Map.hh"
#include "LibertyClass.hh"

namespace sta {

// Build a map from each cell in the library to a group (CellSeq) of
// cells with equivalent functionality, sorted by drive strength.
LibertyCellEquivMap *
makeEquivCellMap(const LibertyLibrary *library);

// Delete the LibertyCellEquivMap returned by makeEquivCellMap.
void
deleteEquivCellMap(LibertyCellEquivMap *equiv_map);

// Predicate that is true when the ports, functions, sequentials and
// timing arcs match.
bool
equivCells(const LibertyCell *cell1,
	   const LibertyCell *cell2);

// Predicate that is true when the ports match.
bool
equivCellPorts(const LibertyCell *cell1,
	       const LibertyCell *cell2);

// Predicate that is true when the ports and their functions match.
bool
equivCellPortsAndFuncs(const LibertyCell *cell1,
		       const LibertyCell *cell2);

// Predicate that is true when the timing arc sets match.
bool
equivCellTimingArcSets(const LibertyCell *cell1,
		       const LibertyCell *cell2);

} // namespace
#endif
