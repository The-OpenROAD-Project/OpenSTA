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

#include <limits>
#include "Machine.hh"
#include "Network.hh"
#include "StaState.hh"

namespace sta {

StaState::StaState() :
  report_(NULL),
  debug_(NULL),
  units_(NULL),
  network_(NULL),
  sdc_(NULL),
  corners_(NULL),
  graph_(NULL),
  levelize_(NULL),
  parasitics_(NULL),
  arc_delay_calc_(NULL),
  graph_delay_calc_(NULL),
  sim_(NULL),
  search_(NULL),
  latches_(NULL),
  thread_count_(1)
{
}

StaState::StaState(const StaState *sta) :
  report_(sta->report_),
  debug_(sta->debug_),
  units_(sta->units_),
  network_(sta->network_),
  sdc_network_(sta->sdc_network_),
  cmd_network_(sta->cmd_network_),
  sdc_(sta->sdc_),
  corners_(sta->corners_),
  graph_(sta->graph_),
  levelize_(sta->levelize_),
  parasitics_(sta->parasitics_),
  arc_delay_calc_(sta->arc_delay_calc_),
  graph_delay_calc_(sta->graph_delay_calc_),
  sim_(sta->sim_),
  search_(sta->search_),
  latches_(sta->latches_),
  thread_count_(sta->thread_count_)
{
}

void
StaState::copyState(const StaState *sta)
{
  report_ = sta->report_;
  debug_ = sta->debug_;
  units_ = sta->units_;
  network_ = sta->network_;
  sdc_network_ = sta->sdc_network_;
  cmd_network_ = sta->cmd_network_;
  sdc_ = sta->sdc_;
  corners_ = sta->corners_;
  graph_ = sta->graph_;
  levelize_ = sta->levelize_;
  parasitics_ = sta->parasitics_;
  arc_delay_calc_ = sta->arc_delay_calc_;
  graph_delay_calc_ = sta->graph_delay_calc_;
  sim_ = sta->sim_;
  search_ = sta->search_;
  latches_ = sta->latches_;
  thread_count_ = sta->thread_count_;
}

NetworkEdit *
StaState::networkEdit()
{
  return dynamic_cast<NetworkEdit*>(network_);
}

NetworkEdit *
StaState::networkEdit() const
{
  return dynamic_cast<NetworkEdit*>(network_);
}

NetworkReader *
StaState::networkReader()
{
  return dynamic_cast<NetworkReader*>(network_);
}

NetworkReader *
StaState::networkReader() const
{
  return dynamic_cast<NetworkReader*>(network_);
}

} // namespace
