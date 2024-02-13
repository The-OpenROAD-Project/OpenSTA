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

#include "StaState.hh"

#include <limits>

#include "DispatchQueue.hh"
#include "Units.hh"
#include "Network.hh"

namespace sta {

StaState::StaState() :
  report_(nullptr),
  debug_(nullptr),
  units_(nullptr),
  network_(nullptr),
  sdc_(nullptr),
  corners_(nullptr),
  graph_(nullptr),
  levelize_(nullptr),
  parasitics_(nullptr),
  arc_delay_calc_(nullptr),
  graph_delay_calc_(nullptr),
  sim_(nullptr),
  search_(nullptr),
  latches_(nullptr),
  clk_network_(nullptr),
  thread_count_(1),
  dispatch_queue_(nullptr),
  pocv_enabled_(false),
  sigma_factor_(1.0)
{
}

StaState::StaState(const StaState *sta)
{
  *this = *sta;
}

void
StaState::copyState(const StaState *sta)
{
  *this = *sta;
}

void
StaState::copyUnits(const Units *units)
{
  *units_ = *units;
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

void
StaState::setReport(Report *report)
{
  report_ = report;
}

void
StaState::setDebug(Debug *debug)
{
  debug_ = debug;
}

} // namespace
