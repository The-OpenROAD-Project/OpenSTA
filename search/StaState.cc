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

#include "StaState.hh"

#include <limits>

#include "ContainerHelpers.hh"
#include "DispatchQueue.hh"
#include "Units.hh"
#include "Network.hh"
#include "Variables.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "TimingArc.hh"
#include "Mode.hh"
#include "Scene.hh"

namespace sta {

StaState::StaState() :
  report_(nullptr),
  debug_(nullptr),
  units_(nullptr),
  network_(nullptr),
  graph_(nullptr),
  levelize_(nullptr),
  arc_delay_calc_(nullptr),
  graph_delay_calc_(nullptr),
  search_(nullptr),
  latches_(nullptr),
  variables_(nullptr),
  thread_count_(1),
  dispatch_queue_(nullptr),
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

bool
StaState::crprActive(const Mode *mode) const
{
  return mode->sdc()->analysisType() == AnalysisType::ocv
    && variables_->crprEnabled();
}

bool
StaState::isDisabledCondDefault(const Edge *edge) const
{
  return !variables_->condDefaultArcsEnabled()
    && edge->timingArcSet()->isCondDefault();
}

////////////////////////////////////////////////////////////////

size_t
StaState::scenePathCount() const
{
  return scenes_.size() * MinMax::index_count;
}

// The clock insertion delay (source latency) required for setup and
// hold checks is:
//
// hold check
// report_timing -delay_type min
//          path insertion pll_delay
//  src clk  min   early    max
//  tgt clk  max   late     min
//
// setup check
// report_timing -delay_type max
//          path insertion pll_delay
//  src clk  max   late     min
//  tgt clk  min   early    max
//
// For analysis type single or bc_wc only one path is required, but as
// shown above both early and late insertion delays are required.
// To find propagated generated clock insertion delays both early and
// late clock network paths are required. Thus, analysis type single
// makes min and max analysis points.
// Only one of them is enabled to "report paths".

DcalcAPIndex
StaState::dcalcAnalysisPtCount() const
{
  return MinMax::index_count * scenes_.size();
}

const SceneSet
StaState::scenesSet()
{
  return Scene::sceneSet(scenes_);
}


} // namespace
