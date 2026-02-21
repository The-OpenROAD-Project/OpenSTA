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

#include "RiseFallMinMaxDelay.hh"

namespace sta {

RiseFallMinMaxDelay::RiseFallMinMaxDelay()
{
  for (int rf_index = 0; rf_index<RiseFall::index_count; rf_index++) {
    for (int mm_index = 0; mm_index < MinMax::index_count; mm_index++) {
      exists_[rf_index][mm_index] = false;
    }
  }
}

bool
RiseFallMinMaxDelay::empty() const
{
  for (int rf_index = 0 ; rf_index < RiseFall::index_count ; rf_index++) {
    for (int mm_index = 0; mm_index < MinMax::index_count; mm_index++) {
      if (exists_[rf_index][mm_index])
        return false;
    }
  }
  return true;
}

void
RiseFallMinMaxDelay::value(const RiseFall *rf,
                           const MinMax *min_max,
                           // Return values.
                           Delay &value,
                           bool &exists) const
{
  exists = exists_[rf->index()][min_max->index()];
  if (exists)
    value = values_[rf->index()][min_max->index()];
}

void
RiseFallMinMaxDelay::mergeValue(const RiseFall *rf,
                                const MinMax *min_max,
                                Delay &value,
                                const StaState *sta)
{
  int rf_index = rf->index();
  int mm_index = min_max->index();
  if (!exists_[rf_index][mm_index]
      || delayGreater(value, values_[rf_index][mm_index], min_max, sta)) {
    values_[rf_index][mm_index] = value;
    exists_[rf_index][mm_index] = true;
  }
}

} // namespace
