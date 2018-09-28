// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef STA_WORST_SLACK_H
#define STA_WORST_SLACK_H

namespace sta {

#include "MinMax.hh"
#include "GraphClass.hh"

class StaState;
class WorstSlack;

class WorstSlacks
{
public:
  WorstSlacks(StaState *sta);
  ~WorstSlacks();
  void clear();
  Slack worstSlack(const MinMax *min_max);
  Vertex *worstSlackVertex(const MinMax *min_max);
  void updateWorstSlacks(Vertex *vertex,
			 Slack *slacks);
  void worstSlackNotifyBefore(Vertex *vertex);

protected:
  WorstSlack *worst_slacks_[MinMax::index_count];
};

} // namespace
#endif
