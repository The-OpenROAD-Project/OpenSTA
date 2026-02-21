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

#include "CheckMinPeriods.hh"

#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Clock.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Search.hh"

namespace sta {

CheckMinPeriods::CheckMinPeriods(StaState *sta) :
  heap_(0, MinPeriodSlackLess(sta)),
  sta_(sta)
{
}

void
CheckMinPeriods::clear()
{
  checks_.clear();
  heap_.clear();
}

MinPeriodCheckSeq &
CheckMinPeriods::check(const Net *net,
                       size_t max_count,
                       bool violators,
                       const SceneSeq &scenes)
{
  clear();
  if (!violators)
    heap_.setMaxSize(max_count);

  if (net)
    checkNet(net, violators, scenes);
  else
    checkAll(violators, scenes);

  if (violators)
    sort(checks_, MinPeriodSlackLess(sta_));
  else
    checks_ = heap_.extract();
  return checks_;
}

void
CheckMinPeriods::checkNet(const Net *net,
                          bool violators,
                          const SceneSeq &scenes)
{
  Graph *graph = sta_->graph();
  NetPinIterator *pin_iter = sta_->network()->pinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    Vertex *vertex = graph->pinLoadVertex(pin);
    checkVertex(vertex, violators, scenes);
  }
  delete pin_iter;
}

void
CheckMinPeriods::checkAll(bool violators,
                          const SceneSeq &scenes)
{
  Graph *graph = sta_->graph();
  VertexIterator vertex_iter(graph);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    checkVertex(vertex, violators, scenes);
  }
}

void
CheckMinPeriods::checkVertex(Vertex *vertex,
                             bool violators,
                             const SceneSeq &scenes)
{
  MinPeriodCheck min_check = check(vertex, scenes);
  if (!min_check.isNull()) {
    if (violators) {
      if (delayLess(min_check.slack(sta_), 0.0, sta_))
        checks_.push_back(min_check);
    }
    else
      heap_.insert(min_check);
  }
}

MinPeriodCheck
CheckMinPeriods::check(Vertex *vertex,
                       const SceneSeq &scenes)
{
  Search *search = sta_->search();
  GraphDelayCalc *graph_dcalc = sta_->graphDelayCalc();
  MinPeriodCheck min_slack_check;
  Pin *pin = vertex->pin();
  for (const Scene *scene : scenes) {
    const Mode *mode = scene->mode();
    if (isClkEnd(vertex, mode)) {
      float min_period;
      bool exists;
      graph_dcalc->minPeriod(pin, scene, min_period, exists);
      if (exists) {
        const ClockSet clks = search->clocks(vertex, mode);
        for (Clock *clk : clks) {
          MinPeriodCheck check(pin, clk, scene);
          Slack slack = check.slack(sta_);
          if (min_slack_check.isNull()
              || delayLess(slack, min_slack_check.slack(sta_), sta_))
            min_slack_check = check;
        }
      }
    }
  }
  return min_slack_check;
}

////////////////////////////////////////////////////////////////

MinPeriodCheck::MinPeriodCheck(Pin *pin,
                               Clock *clk,
                               const Scene *scene) :
  pin_(pin),
  clk_(clk),
  scene_(scene)
{
}

MinPeriodCheck::MinPeriodCheck() :
  pin_(nullptr),
  clk_(nullptr),
  scene_(nullptr)
{
}

float
MinPeriodCheck::period() const
{
  return clk_->period();
}

float
MinPeriodCheck::minPeriod(const StaState *sta) const
{
  GraphDelayCalc *graph_dcalc = sta->graphDelayCalc();
  float min_period;
  bool exists;
  graph_dcalc->minPeriod(pin_, scene_, min_period, exists);
  return min_period;
}

Slack
MinPeriodCheck::slack(const StaState *sta) const
{
  return clk_->period() - minPeriod(sta);
}

////////////////////////////////////////////////////////////////

MinPeriodSlackLess::MinPeriodSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
MinPeriodSlackLess::operator()(const MinPeriodCheck &check1,
                               const MinPeriodCheck &check2) const
{
  Slack slack1 = check1.slack(sta_);
  Slack slack2 = check2.slack(sta_);
  const Pin *pin1 = check1.pin();
  const Pin *pin2 = check2.pin();
  return delayLess(slack1, slack2, sta_)
    // Break ties based on pin and clock names.
    || (delayEqual(slack1, slack2)
        && (sta_->network()->pinLess(pin1, pin2)
            || (pin1 == pin2
                && ClockNameLess()(check1.clk(),
                                   check2.clk()))));
}

} // namespace
