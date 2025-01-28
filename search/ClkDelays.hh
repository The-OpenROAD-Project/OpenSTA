// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

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
             Delay &insertion,
             Delay &delay,
             float &internal_latency,
             Delay &latency,
             PathVertex &path,
             bool &exists) const;
  void latency(const RiseFall *src_rf,
               const RiseFall *end_rf,
               const MinMax *min_max,
               // Return values.
               Delay &delay,
               bool &exists) const;
  static Delay latency(PathVertex *clk_path,
                       StaState *sta);
  void setLatency(const RiseFall *src_rf,
                  const RiseFall *end_rf,
                  const MinMax *min_max,
                  PathVertex *path,
                  bool include_internal_latency,
                  StaState *sta);

private:
  static float insertionDelay(PathVertex *clk_path,
                              StaState *sta);
  static float delay(PathVertex *clk_path,
                     StaState *sta);
  static float clkTreeDelay(PathVertex *clk_path,
                            StaState *sta);

  Delay insertion_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  Delay delay_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  float internal_latency_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  Delay latency_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  PathVertex path_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
  bool exists_[RiseFall::index_count][RiseFall::index_count][MinMax::index_count];
};

} // namespace
