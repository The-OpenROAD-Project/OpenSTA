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

#include "CheckMaxSkews.hh"

#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "Path.hh"
#include "Search.hh"

namespace sta {

CheckMaxSkews::CheckMaxSkews(StaState *sta) :
  sta_(sta)
{
}

CheckMaxSkews::~CheckMaxSkews()
{
}

void
CheckMaxSkews::clear()
{
  checks_.clear();
}

MaxSkewCheckSeq &
CheckMaxSkews::check(const Net *net,
                     size_t max_count,
                     bool violators,
                     const SceneSeq &scenes)
{
  clear();
  scenes_ = Scene::sceneSet(scenes);

  Graph *graph = sta_->graph();
  const Network *network = sta_->network();
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      Vertex *vertex = graph->pinLoadVertex(pin);
      check(vertex, violators);
    }
    delete pin_iter;
  }
  else {
    VertexIterator vertex_iter(graph);
    while (vertex_iter.hasNext()) {
      Vertex *vertex = vertex_iter.next();
      check(vertex, violators);
    }
  }

  // Sort checks by slack
  sort(checks_, MaxSkewSlackLess(sta_));
  if (!violators && checks_.size() > max_count)
    checks_.resize(max_count);
  return checks_;
}

void
CheckMaxSkews::check(Vertex *vertex,
                     bool violators)
{
  Graph *graph = sta_->graph();
  Search *search = sta_->search();
  const MinMax *clk_min_max = MinMax::max();
  MaxSkewCheck min_slack_check;
  VertexInEdgeIterator edge_iter(vertex, graph);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->role() == TimingRole::skew()) {
      Vertex *ref_vertex = edge->from(graph);
      TimingArcSet *arc_set = edge->timingArcSet();
      for (TimingArc *arc : arc_set->arcs()) {
        const RiseFall *clk_rf = arc->fromEdge()->asRiseFall();
        const RiseFall *ref_rf = arc->toEdge()->asRiseFall();
        VertexPathIterator clk_path_iter(vertex, clk_rf, clk_min_max, search);
        while (clk_path_iter.hasNext()) {
          Path *clk_path = clk_path_iter.next();
          if (clk_path->isClock(search)) {
            const Scene *scene = clk_path->scene(sta_);
            if (scenes_.contains(scene)) {
              const MinMax *ref_min_max = clk_path->tgtClkMinMax(sta_);
              VertexPathIterator ref_path_iter(ref_vertex, scene, ref_min_max,
                                               ref_rf, sta_);
              while (ref_path_iter.hasNext()) {
                Path *ref_path = ref_path_iter.next();
                if (ref_path->isClock(search)) {
                  MaxSkewCheck skew_check(clk_path, ref_path, arc, edge);
                  Slack slack = skew_check.slack(sta_);
                  if ((min_slack_check.isNull()
                       || delayLess(slack, min_slack_check.slack(sta_), sta_))
                      && (!violators ||
                          delayLess(slack, 0.0, sta_)))
                    min_slack_check = skew_check;
                }
              }
            }
          }
        }
      }
    }
  }
  if (!min_slack_check.isNull())
    checks_.push_back(min_slack_check);
}

////////////////////////////////////////////////////////////////

MaxSkewCheck::MaxSkewCheck() :
  clk_path_(nullptr),
  ref_path_(nullptr),
  check_arc_(nullptr),
  check_edge_(nullptr)
{
}

MaxSkewCheck::MaxSkewCheck(Path *clk_path,
                           Path *ref_path,
                           TimingArc *check_arc,
                           Edge *check_edge) :
  clk_path_(clk_path),
  ref_path_(ref_path),
  check_arc_(check_arc),
  check_edge_(check_edge)
{
}

Pin *
MaxSkewCheck::clkPin(const StaState *sta) const
{
  return clk_path_->pin(sta);
}

Pin *
MaxSkewCheck::refPin(const StaState *sta) const
{
  return ref_path_->pin(sta);
}

ArcDelay
MaxSkewCheck::maxSkew(const StaState *sta) const
{
  Search *search = sta->search();
  return search->deratedDelay(ref_path_->vertex(sta),
                              check_arc_, check_edge_, false,
                              clk_path_->minMax(sta),
                              clk_path_->dcalcAnalysisPtIndex(sta),
                              ref_path_->scene(sta)->sdc());
}

Delay
MaxSkewCheck::skew() const
{
  return Delay(clk_path_->arrival() - ref_path_->arrival());
}

Slack
MaxSkewCheck::slack(const StaState *sta) const
{
  return maxSkew(sta) - skew();
}

////////////////////////////////////////////////////////////////

MaxSkewSlackLess::MaxSkewSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
MaxSkewSlackLess::operator()(const MaxSkewCheck &check1,
                             const MaxSkewCheck &check2) const
{
  Slack slack1 = check1.slack(sta_);
  Slack slack2 = check2.slack(sta_);
  return delayLess(slack1, slack2, sta_)
    || (delayEqual(slack1, slack2)
        // Break ties based on constrained pin names.
        && sta_->network()->pinLess(check1.clkPin(sta_), check2.clkPin(sta_)));
}

} // namespace
