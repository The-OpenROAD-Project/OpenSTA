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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <string>
#include <string_view>

#include "LibertyClass.hh"
#include "SdcClass.hh"

namespace sta {

class LibertyGenClk
{
public:
  ~LibertyGenClk();
  std::string_view name() const { return name_; }
  std::string_view clockPin() const { return clock_pin_; }
  std::string_view masterPin() const { return master_pin_; }
  int dividedBy() const { return divided_by_; }
  int multipliedBy() const { return multiplied_by_; }
  float dutyCycle() const { return duty_cycle_; }
  bool invert() const { return invert_; }
  IntSeq *edges() const { return edges_; }
  FloatSeq *edgeShifts() const { return edge_shifts_; }

protected:
  LibertyGenClk(const char *name,
                const char *clock_pin,
                const char *master_pin,
                int divided_by,
                int multiplied_by,
                float duty_cycle,
                bool invert,
                IntSeq *edges,
                FloatSeq *edge_shifts);

  std::string name_;
  std::string clock_pin_;
  std::string master_pin_;
  int divided_by_;
  int multiplied_by_;
  float duty_cycle_;
  bool invert_;
  IntSeq *edges_;
  FloatSeq *edge_shifts_;

  friend class LibertyCell;
};

} // namespace sta
