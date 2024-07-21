%module liberty

%{

// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#include "Liberty.hh"
#include "EquivCells.hh"
#include "LibertyWriter.hh"
#include "Sta.hh"

using namespace sta;

%}

%inline %{

bool
read_liberty_cmd(char *filename,
		 Corner *corner,
		 const MinMaxAll *min_max,
		 bool infer_latches)
{
  Sta *sta = Sta::sta();
  LibertyLibrary *lib = sta->readLiberty(filename, corner, min_max, infer_latches);
  return (lib != nullptr);
}

void
write_liberty_cmd(LibertyLibrary *library,
                  char *filename)
{
  writeLiberty(library, filename, Sta::sta());
}

void
make_equiv_cells(LibertyLibrary *lib)
{
  LibertyLibrarySeq libs;
  libs.push_back(lib);
  Sta::sta()->makeEquivCells(&libs, nullptr);
}

LibertyCellSeq *
find_equiv_cells(LibertyCell *cell)
{
  return Sta::sta()->equivCells(cell);
}

bool
equiv_cells(LibertyCell *cell1,
	    LibertyCell *cell2)
{
  return sta::equivCells(cell1, cell2);
}

bool
equiv_cell_ports(LibertyCell *cell1,
		 LibertyCell *cell2)
{
  return equivCellPorts(cell1, cell2);
}

bool
equiv_cell_timing_arcs(LibertyCell *cell1,
		       LibertyCell *cell2)
{
  return equivCellTimingArcSets(cell1, cell2);
}

LibertyCellSeq *
find_library_buffers(LibertyLibrary *library)
{
  return library->buffers();
}

const char *
liberty_port_direction(const LibertyPort *port)
{
  return port->direction()->name();
}
	     
bool
liberty_supply_exists(const char *supply_name)
{
  auto network = Sta::sta()->network();
  auto lib = network->defaultLibertyLibrary();
  return lib && lib->supplyExists(supply_name);
}

%} // inline
