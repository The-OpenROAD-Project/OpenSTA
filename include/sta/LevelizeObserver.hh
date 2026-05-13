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

namespace sta {

class Vertex;
class Search;
class GraphDelayCalc;

// Observer fired by Levelize during (re)levelization. Downstream consumers
// override the two hooks to invalidate caches that depend on vertex levels.
class LevelizeObserver
{
public:
  virtual ~LevelizeObserver() = default;
  virtual void levelsChangedBefore() = 0;
  virtual void levelChangedBefore(Vertex *vertex) = 0;
};

// Default observer installed by Sta::makeObservers. Forwards level-change
// events to Search and GraphDelayCalc so their internal caches stay
// consistent. Subclass and override to extend (call the base methods first,
// then add your own invalidation).
class StaLevelizeObserver : public LevelizeObserver
{
public:
  StaLevelizeObserver(Search *search, GraphDelayCalc *graph_delay_calc);
  void levelsChangedBefore() override;
  void levelChangedBefore(Vertex *vertex) override;

private:
  Search *search_;
  GraphDelayCalc *graph_delay_calc_;
};

} // namespace sta
