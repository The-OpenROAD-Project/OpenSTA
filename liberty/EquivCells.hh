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

#include "Vector.hh"
#include "Map.hh"
#include "UnorderedMap.hh"
#include "LibertyClass.hh"

namespace sta {

typedef Map<LibertyCell*, LibertyCellSeq*> EquivCellMap;
typedef UnorderedMap<unsigned, LibertyCellSeq*> LibertyCellHashMap;

class EquivCells
{
public:
  // Find equivalent cells in equiv_libs.
  // Optionally add mappings for cells in map_libs.
  EquivCells(LibertyLibrarySeq *equiv_libs,
	     LibertyLibrarySeq *map_libs);
  ~EquivCells();
  // Find equivalents for cell (member of from_libs) in to_libs.
  LibertyCellSeq *equivs(LibertyCell *cell);
  
protected:
  void findEquivCells(const LibertyLibrary *library,
		      LibertyCellHashMap &hash_matches);
  void mapEquivCells(const LibertyLibrary *library,
		     LibertyCellHashMap &hash_matches);

  EquivCellMap equiv_cells_;
  // Unique cell for each equiv cell group.
  LibertyCellSeq unique_equiv_cells_;
};

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

bool
equivCellSequentials(const LibertyCell *cell1,
		     const LibertyCell *cell2);

} // namespace
