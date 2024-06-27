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

%module write_gate_spice

%{

#include "spice/WritePathSpice.hh"

%}

%inline %{

void
write_path_spice_cmd(PathRef *path,
		     const char *spice_filename,
		     const char *subckt_filename,
		     const char *lib_subckt_filename,
		     const char *model_filename,
		     const char *power_name,
		     const char *gnd_name,
                     CircuitSim ckt_sim)
{
  Sta *sta = Sta::sta();
  writePathSpice(path, spice_filename, subckt_filename,
		 lib_subckt_filename, model_filename,
		 power_name, gnd_name, ckt_sim, sta);
}

%} // inline
