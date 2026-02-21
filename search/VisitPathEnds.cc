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

#include "VisitPathEnds.hh"

#include "Debug.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "TimingArc.hh"
#include "ExceptionPath.hh"
#include "PortDelay.hh"
#include "Sdc.hh"
#include "Mode.hh"
#include "Graph.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "Path.hh"
#include "PathEnd.hh"
#include "Search.hh"
#include "GatedClk.hh"
#include "Sim.hh"
#include "Variables.hh"
#include "Scene.hh"

namespace sta {

VisitPathEnds::VisitPathEnds(const StaState *sta) :
  StaState(sta)
{
}

void
VisitPathEnds::visitPathEnds(Vertex *vertex,
                             PathEndVisitor *visitor)
{
  visitPathEnds(vertex, Scene::sceneSet(scenes_), MinMaxAll::all(), false, visitor);
}

void
VisitPathEnds::visitPathEnds(Vertex *vertex,
                             const SceneSet &scenes,
                             const MinMaxAll *min_max,
                             bool filtered,
                             PathEndVisitor *visitor)
{
  // Ignore slack on bidirect driver vertex.  The load vertex gets the slack.
  if (!vertex->isBidirectDriver()) {
    const Pin *pin = vertex->pin();
    debugPrint(debug_, "search", 2, "find end slack %s",
               vertex->to_string(this).c_str());
    visitor->vertexBegin(vertex);
    bool is_constrained = false;
    visitClkedPathEnds(pin, vertex, scenes, min_max, filtered, visitor,
                       is_constrained);
    if (search_->unconstrainedPaths()
        && !is_constrained)
      visitUnconstrainedPathEnds(pin, vertex, scenes, min_max, filtered,
                                 visitor);
    visitor->vertexEnd(vertex);
  }
}

void
VisitPathEnds::visitClkedPathEnds(const Pin *pin,
                                  Vertex *vertex,
                                  const SceneSet &scenes,
                                  const MinMaxAll *min_max,
                                  bool filtered,
                                  PathEndVisitor *visitor,
                                  bool &is_constrained)
{
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    const MinMax *path_min_max = path->minMax(this);
    const RiseFall *end_rf = path->transition(this);
    Tag *tag = path->tag(this);
    const Mode *mode = path->mode(this);
    const Sdc *sdc = mode->sdc();
    Scene *scene = path->scene(this);
    if (scenes.contains(scene)
        && min_max->matches(path_min_max)
        // Ignore generated clock source paths.
        && !path->clkInfo(this)->isGenClkSrcPath()
        && !falsePathTo(path, pin, end_rf, path_min_max)
        // Ignore segment startpoint paths.
        && !tag->isSegmentStart()) {
      // set_output_delay to timing check has precedence.
      if (sdc->hasOutputDelay(pin))
        visitOutputDelayEnd(pin, path, end_rf, filtered, visitor,
                            is_constrained);
      else if (vertex->hasChecks())
        visitCheckEnd(pin, vertex, path, end_rf, filtered, visitor,
                      is_constrained);
      else if (!filtered
               || search_->matchesFilter(path, nullptr)) {
        PathDelay *path_delay = pathDelayTo(path, pin, end_rf, path_min_max);
        if (path_delay) {
          PathEndPathDelay path_end(path_delay, path, this);
          visitor->visit(&path_end);
          is_constrained = true;
        }
      }
      if (variables_->gatedClkChecksEnabled())
        visitGatedClkEnd(pin, vertex, path, end_rf, filtered, visitor,
                         is_constrained);
      visitDataCheckEnd(pin, path, end_rf, filtered, visitor,
                        is_constrained);
    }
  }
}

void
VisitPathEnds::visitCheckEnd(const Pin *pin,
                             Vertex *vertex,
                             Path *path,
                             const RiseFall *end_rf,
                             bool filtered,
                             PathEndVisitor *visitor,
                             bool &is_constrained)
{
  const ClockEdge *src_clk_edge = path->clkEdge(this);
  const Clock *src_clk = path->clock(this);
  const MinMax *min_max = path->minMax(this);
  const MinMax *tgt_min_max = path->tgtClkMinMax(this);
  bool check_clked = false;
  const Scene *scene = path->scene(this);
  const Mode *mode = scene->mode();
  const Sdc *sdc = scene->sdc();
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *tgt_clk_vertex = edge->from(graph_);
    const TimingRole *check_role = edge->role();
    if (checkEdgeEnabled(edge, mode)
        && check_role->pathMinMax() == min_max) {
      TimingArcSet *arc_set = edge->timingArcSet();
      for (TimingArc *check_arc : arc_set->arcs()) {
        const RiseFall *clk_rf = check_arc->fromEdge()->asRiseFall();
        if (check_arc->toEdge()->asRiseFall() == end_rf
            && clk_rf) {
          VertexPathIterator tgt_clk_path_iter(tgt_clk_vertex, scene,
                                               tgt_min_max, clk_rf, this);
          while (tgt_clk_path_iter.hasNext()) {
            Path *tgt_clk_path = tgt_clk_path_iter.next();
            const ClkInfo *tgt_clk_info = tgt_clk_path->clkInfo(this);
            const ClockEdge *tgt_clk_edge = tgt_clk_path->clkEdge(this);
            const Clock *tgt_clk = tgt_clk_path->clock(this);
            const Pin *tgt_pin = tgt_clk_vertex->pin();
            ExceptionPath *exception = exceptionTo(path, pin, end_rf,
                                                   tgt_clk_edge, min_max);
            // Ignore generated clock source paths.
            if (!tgt_clk_info->isGenClkSrcPath()
                && tgt_clk_path->isClock(this)) {
              check_clked = true;
              if (!filtered
                  || search_->matchesFilter(path, tgt_clk_edge)) {
                if (src_clk_edge
                    && tgt_clk != sdc->defaultArrivalClock()
                    && sdc->sameClockGroup(src_clk, tgt_clk)
                    && !sdc->clkStopPropagation(tgt_pin, tgt_clk)
                    // False paths and path delays override
                    // paths.
                    && (exception == nullptr
                        || exception->isFilter()
                        || exception->isGroupPath()
                        || exception->isMultiCycle())) {
                  MultiCyclePath *mcp=dynamic_cast<MultiCyclePath*>(exception);
                  if (network_->isLatchData(pin)
                      && check_role == TimingRole::setup()) {
                    PathEndLatchCheck path_end(path, check_arc, edge,
                                               tgt_clk_path, mcp, nullptr,
                                               this);
                    visitor->visit(&path_end);
                    is_constrained = true;
                  }
                  else {
                    PathEndCheck path_end(path, check_arc, edge,
                                          tgt_clk_path, mcp, this);
                    visitor->visit(&path_end);
                    is_constrained = true;
                  }
                }
                else if (exception
                         && exception->isPathDelay()
                         && (src_clk == nullptr
                             || sdc->sameClockGroup(src_clk, tgt_clk))) {
                  PathDelay *path_delay = dynamic_cast<PathDelay*>(exception);
                  if (network_->isLatchData(pin)
                      && check_role == TimingRole::setup()) {
                    PathEndLatchCheck path_end(path, check_arc, edge,
                                               tgt_clk_path, nullptr,
                                               path_delay, this);
                    visitor->visit(&path_end);
                  }
                  else {
                    PathEndPathDelay path_end(path_delay, path, tgt_clk_path,
                                              check_arc, edge, this);
                    visitor->visit(&path_end);
                    is_constrained = true;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  if (!check_clked)
    visitCheckEndUnclked(pin, vertex, path, end_rf, filtered,
                         visitor, is_constrained);
}

void
VisitPathEnds::visitCheckEndUnclked(const Pin *pin,
                                    Vertex *vertex,
                                    Path *path,
                                    const RiseFall *end_rf,
                                    bool filtered,
                                    PathEndVisitor *visitor,
                                    bool &is_constrained)
{
  const Mode *mode = path->mode(this);
  const MinMax *min_max = path->minMax(this);
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    const TimingRole *check_role = edge->role();
    if (checkEdgeEnabled(edge, mode)
        && check_role->pathMinMax() == min_max) {
      TimingArcSet *arc_set = edge->timingArcSet();
      for (TimingArc *check_arc : arc_set->arcs()) {
        const RiseFall *clk_rf = check_arc->fromEdge()->asRiseFall();
        if (check_arc->toEdge()->asRiseFall() == end_rf
            && clk_rf
            && (!filtered
                || search_->matchesFilter(path, nullptr))) {
          ExceptionPath *exception = exceptionTo(path, pin, end_rf,
                                                 nullptr, min_max);
          // False paths and path delays override multicycle paths.
          if (exception
              && exception->isPathDelay()) {
            PathDelay *path_delay = dynamic_cast<PathDelay*>(exception);
            PathEndPathDelay path_end(path_delay, path, nullptr,
                                      check_arc, edge, this);
            visitor->visit(&path_end);
            is_constrained = true;
          }
        }
      }
    }
  }
}

bool
VisitPathEnds::checkEdgeEnabled(const Edge *edge,
                                const Mode *mode) const
{
  const TimingRole *check_role = edge->role();
  const Sdc *sdc = mode->sdc();
  return check_role->isTimingCheck()
    && search_->evalPred()->searchFrom(edge->from(graph_), mode)
    && !sdc->isDisabledConstraint(edge)
    && !mode->sim()->isDisabledCond(edge)
    && !isDisabledCondDefault(edge)
    && !((check_role == TimingRole::recovery()
          || check_role == TimingRole::removal())
         && !variables_->recoveryRemovalChecksEnabled());
}

void
VisitPathEnds::visitOutputDelayEnd(const Pin *pin,
                                   Path *path,
                                   const RiseFall *end_rf,
                                   bool filtered,
                                   PathEndVisitor *visitor,
                                   bool &is_constrained)
{
  const Scene *scene = path->scene(this);
  const Sdc *sdc = scene->sdc();
  const MinMax *min_max = path->minMax(this);
  OutputDelaySet *output_delays = sdc->outputDelaysLeafPin(pin);
  if (output_delays) {
    for (OutputDelay *output_delay : *output_delays) {
      float margin;
      bool exists;
      output_delay->delays()->value(end_rf, min_max, margin, exists);
      if (exists) {
        const Pin *ref_pin = output_delay->refPin();
        const ClockEdge *tgt_clk_edge = output_delay->clkEdge();
        if (!filtered
            || search_->matchesFilter(path, tgt_clk_edge)) {
          if (ref_pin) {
            Clock *tgt_clk = output_delay->clock();
            Vertex *ref_vertex = graph_->pinLoadVertex(ref_pin);
            const RiseFall *ref_rf = output_delay->refTransition();
            VertexPathIterator ref_path_iter(ref_vertex, scene, min_max,
                                             ref_rf, this);
            while (ref_path_iter.hasNext()) {
              Path *ref_path = ref_path_iter.next();
              if (ref_path->isClock(this)
                  && (tgt_clk == nullptr
                      || ref_path->clock(this) == tgt_clk))
                visitOutputDelayEnd1(output_delay, pin, path, end_rf,
                                     ref_path->clkEdge(this), ref_path, min_max,
                                     visitor, is_constrained);
            }
          }
          else
            visitOutputDelayEnd1(output_delay, pin, path, end_rf,
                                 tgt_clk_edge, nullptr, min_max,
                                 visitor, is_constrained);
        }
      }
    }
  }
}

void
VisitPathEnds::visitOutputDelayEnd1(OutputDelay *output_delay,
                                    const Pin *pin,
                                    Path *path,
                                    const RiseFall *end_rf,
                                    const ClockEdge *tgt_clk_edge,
                                    Path *ref_path,
                                    const MinMax *min_max,
                                    PathEndVisitor *visitor,
                                    bool &is_constrained)
{
  // Target clk is not required for path delay,
  // but the exception may be -to clk.
  ExceptionPath *exception = exceptionTo(path, pin, end_rf, tgt_clk_edge,
                                         min_max);
  const ClockEdge *src_clk_edge = path->clkEdge(this);
  const Sdc *sdc = path->sdc(this);
  if (exception
      && exception->isPathDelay()) {
    PathDelay *path_delay = dynamic_cast<PathDelay*>(exception);
    PathEndPathDelay path_end(path_delay, path, output_delay, this);
    visitor->visit(&path_end);
    is_constrained = true;
  }
  else if (src_clk_edge
           && tgt_clk_edge
           && sdc->sameClockGroup(path->clock(this), tgt_clk_edge->clock())
           // False paths and path delays override.
           && (exception == nullptr
               || exception->isFilter()
               || exception->isGroupPath()
               || exception->isMultiCycle())) {
    MultiCyclePath *mcp = dynamic_cast<MultiCyclePath*>(exception);
    PathEndOutputDelay path_end(output_delay, path, ref_path, mcp, this);
    visitor->visit(&path_end);
    is_constrained = true;
  }
}

////////////////////////////////////////////////////////////////

// Look for clock gating functions where path is the clock enable.
void
VisitPathEnds::visitGatedClkEnd(const Pin *pin,
                                Vertex *vertex,
                                Path *path,
                                const RiseFall *end_rf,
                                bool filtered,
                                PathEndVisitor *visitor,
                                bool &is_constrained)
{
  const Scene *scene = path->scene(this);
  const Sdc *sdc = scene->sdc();
  const ClockEdge *src_clk_edge = path->clkEdge(this);
  if (src_clk_edge
      && !path->isClock(this)
      && !sdc->isDisableClockGatingCheck(pin)
      && !sdc->isDisableClockGatingCheck(network_->instance(pin))) {
    const Mode *mode = scene->mode();
    GatedClk *gated_clk = search_->gatedClk();
    Clock *src_clk = src_clk_edge->clock();
    bool is_gated_clk_enable;
    const Pin *clk_pin;
    LogicValue logic_active_value;
    gated_clk->isGatedClkEnable(vertex, mode,
                                is_gated_clk_enable, clk_pin, logic_active_value);
    if (is_gated_clk_enable) {
      const MinMax *min_max = path->minMax(this);
      const MinMax *tgt_min_max = path->tgtClkMinMax(this);
      Vertex *clk_vertex = graph_->pinLoadVertex(clk_pin);
      LogicValue active_value =
        sdc->clockGatingActiveValue(clk_pin, pin);
      const RiseFall *clk_rf =
        // Clock active value specified by set_clock_gating_check
        // overrides the library cell function active value.
        gated_clk->gatedClkActiveTrans((active_value == LogicValue::unknown) ?
                                       logic_active_value : active_value,
                                       min_max);
      VertexPathIterator clk_path_iter(clk_vertex, scene, tgt_min_max,
                                       clk_rf, this);
      while (clk_path_iter.hasNext()) {
        Path *clk_path = clk_path_iter.next();
        const ClockEdge *clk_edge = clk_path->clkEdge(this);
        const Clock *clk = clk_edge ? clk_edge->clock() : nullptr;
        if (clk_path->isClock(this)
            // Ignore unclocked paths (from path delay constraints).
            && clk_edge
            && clk_edge != sdc->defaultArrivalClockEdge()
            // Ignore generated clock source paths.
            && !path->clkInfo(this)->isGenClkSrcPath()
            && !sdc->clkStopPropagation(pin, clk)
            && clk_vertex->hasDownstreamClkPin()) {
          const TimingRole *check_role = (min_max == MinMax::max())
            ? TimingRole::gatedClockSetup()
            : TimingRole::gatedClockHold();
          float margin = clockGatingMargin(clk, clk_pin, pin,
                                           end_rf, min_max, sdc);
          ExceptionPath *exception = exceptionTo(path, pin, end_rf,
                                                 clk_edge, min_max);
          if (sdc->sameClockGroup(src_clk, clk)
              // False paths and path delays override.
              && (exception == nullptr
                  || exception->isFilter()
                  || exception->isGroupPath()
                  || exception->isMultiCycle())
              && (!filtered
                  || search_->matchesFilter(path, clk_edge))) {
            MultiCyclePath *mcp =
              dynamic_cast<MultiCyclePath *>(exception);
            PathEndGatedClock path_end(path, clk_path, check_role,
                                       mcp, margin, this);
            visitor->visit(&path_end);
            is_constrained = true;
          }
        }
      }
    }
  }
}

// Gated clock setup/hold margin respecting precedence rules.
// Look for margin from highest precedence level to lowest.
float
VisitPathEnds::clockGatingMargin(const Clock *clk,
                                 const Pin *clk_pin,
                                 const Pin *enable_pin,
                                 const RiseFall *enable_rf,
                                 const SetupHold *setup_hold,
                                 const Sdc *sdc)
{
  bool exists;
  float margin;
  sdc->clockGatingMarginEnablePin(enable_pin, enable_rf,
                                  setup_hold, exists, margin);
  if (exists)
    return margin;
  Instance *inst = network_->instance(enable_pin);
  sdc->clockGatingMarginInstance(inst, enable_rf, setup_hold,
                                 exists, margin);
  if (exists)
    return margin;
  sdc->clockGatingMarginClkPin(clk_pin, enable_rf, setup_hold,
                               exists, margin);
  if (exists)
    return margin;
  sdc->clockGatingMarginClk(clk, enable_rf, setup_hold,
                            exists, margin);
  if (exists)
    return margin;
  sdc->clockGatingMargin(enable_rf, setup_hold,
                         exists, margin);
  if (exists)
    return margin;
  else
    return 0.0;
}

////////////////////////////////////////////////////////////////

void
VisitPathEnds::visitDataCheckEnd(const Pin *pin,
                                 Path *path,
                                 const RiseFall *end_rf,
                                 bool filtered,
                                 PathEndVisitor *visitor,
                                 bool &is_constrained)
{
  const ClockEdge *src_clk_edge = path->clkEdge(this);
  if (src_clk_edge) {
    const Sdc *sdc = path->sdc(this);
    DataCheckSet *checks = sdc->dataChecksTo(pin);
    if (checks) {
      const Clock *src_clk = src_clk_edge->clock();
      const MinMax *min_max = path->minMax(this);
      for (DataCheck *check : *checks) {
        const Pin *from_pin = check->from();
        Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
        for (auto from_rf : RiseFall::range()) {
          float margin;
          bool margin_exists;
          check->margin(from_rf, end_rf, min_max, margin, margin_exists);
          if (margin_exists)
            visitDataCheckEnd1(check, pin, path, src_clk, end_rf,
                               min_max, from_pin, from_vertex,
                               from_rf, filtered, visitor, is_constrained);
        }
      }
    }
  }
}

bool
VisitPathEnds::visitDataCheckEnd1(DataCheck *check,
                                  const Pin *pin,
                                  Path *path,
                                  const Clock *src_clk,
                                  const RiseFall *end_rf,
                                  const MinMax *min_max,
                                  const Pin *from_pin,
                                  Vertex *from_vertex,
                                  const RiseFall *from_rf,
                                  bool filtered,
                                  PathEndVisitor *visitor,
                                  bool &is_constrained)
{
  bool found_from_path = false;
  const Scene *scene = path->scene(this);
  const Sdc *sdc = scene->sdc();
  const MinMax *tgt_min_max = path->tgtClkMinMax(this);
  VertexPathIterator tgt_clk_path_iter(from_vertex, scene, tgt_min_max,
                                       from_rf,this);
  while (tgt_clk_path_iter.hasNext()) {
    Path *tgt_clk_path = tgt_clk_path_iter.next();
    const ClockEdge *tgt_clk_edge = tgt_clk_path->clkEdge(this);
    // Ignore generated clock source paths.
    if (tgt_clk_edge
        && !tgt_clk_path->clkInfo(this)->isGenClkSrcPath()) {
      found_from_path = true;
      const Clock *tgt_clk = tgt_clk_edge->clock();
      ExceptionPath *exception = exceptionTo(path, pin, end_rf,
                                             tgt_clk_edge, min_max);
      if (sdc->sameClockGroup(src_clk, tgt_clk)
          && !sdc->clkStopPropagation(from_pin, tgt_clk)
          // False paths and path delays override.
          && (exception == 0
              || exception->isFilter()
              || exception->isGroupPath()
              || exception->isMultiCycle())
          && (!filtered
              || search_->matchesFilter(path, tgt_clk_edge))) {
        MultiCyclePath *mcp=dynamic_cast<MultiCyclePath*>(exception);
        PathEndDataCheck path_end(check, path, tgt_clk_path, mcp, this);
        visitor->visit(&path_end);
        is_constrained = true;
      }
    }
  }
  return found_from_path;
}

////////////////////////////////////////////////////////////////

void
VisitPathEnds::visitUnconstrainedPathEnds(const Pin *pin,
                                          Vertex *vertex,
                                          const SceneSet &scenes,
                                          const MinMaxAll *min_max,
                                          bool filtered,
                                          PathEndVisitor *visitor)
{
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    const MinMax *path_min_max = path->minMax(this);
    Scene *scene = path->scene(this);
    const Sdc *sdc = scene->sdc();
    if (scenes.contains(scene)
        && min_max->matches(path_min_max)
        && !sdc->isDisabledConstraint(pin)
        // Ignore generated clock source paths.
        && !path->clkInfo(this)->isGenClkSrcPath()
        && (!filtered
            || search_->matchesFilter(path, nullptr))
        && !falsePathTo(path, pin, path->transition(this),
                        path_min_max)) {
      PathEndUnconstrained path_end(path);
      visitor->visit(&path_end);
    }
  }
}

////////////////////////////////////////////////////////////////

bool
VisitPathEnds::falsePathTo(Path *path,
                           const Pin *pin,
                           const RiseFall *rf,
                           const MinMax *min_max)
{
  ExceptionPath *exception = search_->exceptionTo(ExceptionPathType::false_path, path,
                                                  pin, rf, nullptr, min_max,
                                                  false, false, path->sdc(this));
  return exception != nullptr;
}

PathDelay *
VisitPathEnds::pathDelayTo(Path *path,
                           const Pin *pin,
                           const RiseFall *rf,
                           const MinMax *min_max)
{
  ExceptionPath *exception = search_->exceptionTo(ExceptionPathType::path_delay,
                                                  path, pin, rf, nullptr,
                                                  min_max, false,
                                                  // Register clk pins only
                                                  // match with -to pin.
                                                  network_->isRegClkPin(pin),
                                                  path->sdc(this));
  return dynamic_cast<PathDelay*>(exception);
}

ExceptionPath *
VisitPathEnds::exceptionTo(const Path *path,
                           const Pin *pin,
                           const RiseFall *rf,
                           const ClockEdge *clk_edge,
                           const MinMax *min_max) const
{
  return search_->exceptionTo(ExceptionPathType::any, path, pin, rf, clk_edge,
                              min_max, false, false, path->sdc(this));
}

} // namespace
