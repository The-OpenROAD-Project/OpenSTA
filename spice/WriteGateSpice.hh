// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include <vector>

#include "Transition.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "ArcDelayCalc.hh"
#include "CircuitSim.hh"

namespace sta {

class StaState;

// Write a spice deck for gates (parallel or victim/aggressors).
void
writeGateSpice(ArcDcalcArgSeq &gates,
               const char *spice_filename,
               const char *subckt_filename,
               StringSeq lib_subckt_filenames,
               const char *model_filename,
               const char *power_name,
               const char *gnd_name,
               CircuitSim ckt_sim,
               const Scene *scene,
               const MinMax *min_max,
               const StaState *sta);

} // namespace sta
