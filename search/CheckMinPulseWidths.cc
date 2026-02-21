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

#include "CheckMinPulseWidths.hh"

#include "ContainerHelpers.hh"
#include "Debug.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "Sdc.hh"
#include "GraphDelayCalc.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "Path.hh"
#include "Scene.hh"
#include "SearchPred.hh"
#include "PathEnd.hh"
#include "Search.hh"
#include "search/Crpr.hh"

namespace sta {

static void
minPulseWidth(const Path *path,
              const StaState *sta,
              // Return values.
              float &min_width,
              bool &exists);

CheckMinPulseWidths::CheckMinPulseWidths(StaState *sta) :
  heap_(0, MinPulseWidthSlackLess(sta)),
  sta_(sta)
{
}

void
CheckMinPulseWidths::clear()
{
  checks_.clear();
  heap_.clear();
}

MinPulseWidthCheckSeq &
CheckMinPulseWidths::check(const Net *net,
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
    sort(checks_, MinPulseWidthSlackLess(sta_));
  else
    checks_ = heap_.extract();
  return checks_;
}

void
CheckMinPulseWidths::checkNet(const Net *net,
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
CheckMinPulseWidths::checkAll(bool violators,
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
CheckMinPulseWidths::checkVertex(Vertex *vertex,
                                 bool violators,
                                 const SceneSeq &scenes)
{
  Search *search = sta_->search();
  Debug *debug = sta_->debug();
  const MinMax *min_max = MinMax::max();
  SceneSet scene_set = Scene::sceneSet(scenes);
  VertexPathIterator path_iter(vertex, search);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Vertex *path_vertex = path->vertex(sta_);
    const Mode *mode = path->mode(sta_);
    if (isClkEnd(path_vertex, mode)
        && path->isClock(search)
        && !path->tag(sta_)->clkInfo()->isGenClkSrcPath()
        && scene_set.find(path->scene(sta_)) != scene_set.end()
        && path->minMax(sta_) == min_max) {
      float min_width;
      bool exists;
      minPulseWidth(path, sta_, min_width, exists);
      if (exists) {
        MinPulseWidthCheck check(path);
        Path *close_path = check.closePath(sta_);
        // Don't bother visiting if nobody is home.
        if (close_path) {
          debugPrint(debug, "mpw", 2, "%s %s %s",
                     path_vertex->to_string(sta_).c_str(),
                     path->transition(sta_) == RiseFall::rise() ? "(high)" : "(low)",
                     delayAsString(check.slack(sta_), sta_));
          if (violators) {
            if (delayLess(check.slack(sta_), 0.0, sta_))
              checks_.push_back(check);
          }
          else
            heap_.insert(check);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

MinPulseWidthCheck::MinPulseWidthCheck() :
  open_path_(nullptr)
{
}

MinPulseWidthCheck::MinPulseWidthCheck(Path *open_path) :
  open_path_(open_path)
{
}

std::string
MinPulseWidthCheck::to_string(const StaState *sta)
{
  std::string result = sta->network()->pathName(pin(sta));
  result += " ";
  result += (openTransition(sta) == RiseFall::rise()) ? "(high)" : "(low)";
  return result;
}

Pin *
MinPulseWidthCheck::pin(const StaState *sta) const
{
  return open_path_->pin(sta);
}

const RiseFall *
MinPulseWidthCheck::openTransition(const StaState *sta) const
{
  return open_path_->transition(sta);
}

Path *
MinPulseWidthCheck::closePath(const StaState *sta) const
{
  Scene *scene = open_path_->scene(sta);
  const MinMax *close_min_max = open_path_->tgtClkMinMax(sta);
  const RiseFall *open_rf = open_path_->transition(sta);
  const RiseFall *close_rf = open_rf->opposite();
  Tag *open_tag = open_path_->tag(sta);
  const ClkInfo *open_clk_info = open_tag->clkInfo();
  const ClkInfo close_clk_info(scene,
                               open_clk_info->clkEdge()->opposite(),
                               open_clk_info->clkSrc(),
                               open_clk_info->isPropagated(),
                               open_clk_info->genClkSrc(),
                               open_clk_info->isGenClkSrcPath(),
                               open_clk_info->pulseClkSense(),
                               delay_zero, 0.0, nullptr,
                               open_clk_info->minMax(),
                               open_clk_info->crprClkPath(sta),
                               sta);
  Tag close_tag(scene,
                0,
                close_rf,
                close_min_max,
                &close_clk_info,
                open_tag->isClock(),
                open_tag->inputDelay(),
                open_tag->isSegmentStart(),
                open_tag->states(),
                false);
  debugPrint(sta->debug(), "mpw", 3, " open  %s",
             open_tag->to_string(sta).c_str());
  debugPrint(sta->debug(), "mpw", 3, " close %s",
             close_tag.to_string(sta).c_str());
  VertexPathIterator close_iter(open_path_->vertex(sta), scene, close_min_max,
                                close_rf, sta);
  while (close_iter.hasNext()) {
    Path *close_path = close_iter.next();
    if (Tag::matchNoPathAp(close_path->tag(sta), &close_tag)) {
      debugPrint(sta->debug(), "mpw", 3, " match %s",
                 close_path->tag(sta)->to_string(sta).c_str());
      return close_path;
    }
  }
  return nullptr;
}

Arrival
MinPulseWidthCheck::openArrival(const StaState *) const
{
  return open_path_->arrival();
}

Arrival
MinPulseWidthCheck::closeArrival(const StaState *sta) const
{
  Path *close = closePath(sta);
  return close->arrival();
}

Arrival
MinPulseWidthCheck::openDelay(const StaState *sta) const
{
  return openArrival(sta) - openClkEdge(sta)->time();
}

Arrival
MinPulseWidthCheck::closeDelay(const StaState *sta) const
{
  return closeArrival(sta) - closeClkEdge(sta)->time();
}

const ClockEdge *
MinPulseWidthCheck::openClkEdge(const StaState *sta) const
{
  return open_path_->clkEdge(sta->search());
}

const ClockEdge *
MinPulseWidthCheck::closeClkEdge(const StaState *sta) const
{
  Tag *open_tag = open_path_->tag(sta);
  const ClkInfo *open_clk_info = open_tag->clkInfo();
  return open_clk_info->clkEdge()->opposite();
}

float
MinPulseWidthCheck::closeOffset(const StaState *sta) const
{
  const ClockEdge *open_clk_edge = openClkEdge(sta);
  const ClockEdge *close_clk_edge = closeClkEdge(sta);
  if (open_clk_edge->time() > close_clk_edge->time())
    return open_clk_edge->clock()->period();
  else
    return 0.0;
}

Arrival
MinPulseWidthCheck::width(const StaState *sta) const
{
  return closeArrival(sta) + closeOffset(sta)
    - open_path_->arrival()
    + checkCrpr(sta);
}

float
MinPulseWidthCheck::minWidth(const StaState *sta) const
{
  float min_width;
  bool exists;
  minPulseWidth(open_path_, sta, min_width, exists);
  return min_width;
}

// Precedence:
//  set_min_pulse_width SDC command
//  SDF annotation
//  Liberty library
//    port min_pulse_width_low/high
//    min_pulse_width timing group
static void
minPulseWidth(const Path *path,
              const StaState *sta,
              // Return values.
              float &min_width,
              bool &exists)
{
  Pin *pin = path->pin(sta);
  const Clock *clk = path->clock(sta);
  const RiseFall *rf = path->transition(sta);
  const Sdc *sdc = path->sdc(sta);
  // set_min_pulse_width command.
  sdc->minPulseWidth(pin, clk, rf, min_width, exists);
  if (!exists) {
    DcalcAPIndex dcalc_ap = path->dcalcAnalysisPtIndex(sta);
    Vertex *vertex = path->vertex(sta);
    Graph *graph = sta->graph();
    Edge *edge;
    TimingArc *arc;
    graph->minPulseWidthArc(vertex, rf, edge, arc);
    if (edge) {
      min_width = delayAsFloat(graph->arcDelay(edge, arc, dcalc_ap));
      exists = true;
    }
  }
}

Crpr
MinPulseWidthCheck::checkCrpr(const StaState *sta) const
{
  CheckCrpr *check_crpr = sta->search()->checkCrpr();
  Path *close = closePath(sta);
  if (close)
    return check_crpr->checkCrpr(openPath(), close);
  else
    return 0.0;
}

Slack
MinPulseWidthCheck::slack(const StaState *sta) const
{
  return width(sta) - minWidth(sta);
}

Scene *
MinPulseWidthCheck::scene(const StaState *sta) const
{
  return open_path_->scene(sta);
}

////////////////////////////////////////////////////////////////

MinPulseWidthSlackLess::MinPulseWidthSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
MinPulseWidthSlackLess::operator()(const MinPulseWidthCheck &check1,
                                   const MinPulseWidthCheck &check2) const
{
  Slack slack1 = check1.slack(sta_);
  Slack slack2 = check2.slack(sta_);
  const Pin *pin1 = check1.pin(sta_);
  const Pin *pin2 = check2.pin(sta_);
  return delayLess(slack1, slack2, sta_)
    || (delayEqual(slack1, slack2)
        // Break ties for the sake of regression stability.
        && (sta_->network()->pinLess(pin1, pin2)
            || (pin1 == pin2
                && check1.openPath()->rfIndex(sta_)
                < check2.openPath()->rfIndex(sta_))));
}

} // namespace
