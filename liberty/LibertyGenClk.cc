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

#include "LibertyGenClk.hh"

namespace sta {

LibertyGenClk::LibertyGenClk(const char *name,
                             const char *clock_pin,
                             const char *master_pin,
                             int divided_by,
                             int multiplied_by,
                             float duty_cycle,
                             bool invert,
                             IntSeq *edges,
                             FloatSeq *edge_shifts) :
  name_(name),
  clock_pin_(clock_pin),
  master_pin_(master_pin ? master_pin : ""),
  divided_by_(divided_by),
  multiplied_by_(multiplied_by),
  duty_cycle_(duty_cycle),
  invert_(invert),
  edges_(edges),
  edge_shifts_(edge_shifts)
{
}

LibertyGenClk::~LibertyGenClk()
{
  if (edges_)
    delete edges_;
  if (edge_shifts_)
    delete edge_shifts_;
}

} // namespace sta
