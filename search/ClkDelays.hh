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

#pragma once

#include "MinMax.hh"
#include "StaState.hh"
#include "Transition.hh"
#include "PathVertex.hh"

namespace sta {

class ClkDelays
{
public:
  ClkDelays();
  void delay(const RiseFall *src_rf,
             const RiseFall *end_rf,
             const MinMax *min_max,
             // Return values.
             float &insertion,
             float &delay,
             float &lib_clk_delay,
             float &latency,
             PathVertex &path,
             bool &exists);
  void latency(const RiseFall *src_rf,
               const RiseFall *end_rf,
               const MinMax *min_max,
               // Return values.
               float &delay,
               bool &exists);
  static float latency(PathVertex *clk_path,
                       StaState *sta);
  void setLatency(const RiseFall *src_rf,
                  const RiseFall *end_rf,
                  const MinMax *min_max,
                  PathVertex *path,
                  StaState *sta);

private:
  static float insertionDelay(PathVertex *clk_path,
                              StaState *sta);
  static float delay(PathVertex *clk_path,
                     StaState *sta);
  static float clkTreeDelay(PathVertex *clk_path,
                            StaState *sta);

  float insertion_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  float delay_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  float lib_clk_delay_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  float latency_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  PathVertex path_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  bool exists_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
};

} // namespace
