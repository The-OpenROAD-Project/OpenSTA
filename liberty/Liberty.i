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

#include "Sta.hh"
#include "LibertyWriter.hh"

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

%} // inline
