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

#include "SearchPred.hh"

#include "TimingArc.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Mode.hh"
#include "Levelize.hh"
#include "Search.hh"
#include "Latches.hh"
#include "Sim.hh"
#include "Variables.hh"

namespace sta {

static bool
searchThruSimEdge(const Vertex *vertex,
                  const RiseFall *rf,
                  const Mode *mode);
static bool
searchThruTimingSense(const Edge *edge,
                      const RiseFall *from_rf,
                      const RiseFall *to_rf,
                      const Mode *mode);

SearchPred::SearchPred(const StaState *sta) :
  sta_(sta)
{
}

void
SearchPred::copyState(const StaState *sta)
{
  sta_ = sta;
}

bool
SearchPred::searchFrom(const Vertex *from_vertex) const
{
  for (const Mode *mode : sta_->modes()) {
    if (searchFrom(from_vertex, mode))
      return true;
  }
  return false;
}

bool
SearchPred::searchThru(Edge *edge) const
{
  for (const Mode *mode : sta_->modes()) {
    if (searchThru(edge, mode))
      return true;
  }
  return false;
}

bool
SearchPred::searchTo(const Vertex *to_vertex) const
{
  for (const Mode *mode : sta_->modes()) {
    if (searchTo(to_vertex, mode))
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////

SearchPred0::SearchPred0(const StaState *sta) :
  SearchPred(sta)
{
}

bool
SearchPred0::searchFrom(const Vertex *from_vertex,
                        const Mode *mode) const
{
  const Pin *from_pin = from_vertex->pin();
  const Sdc *sdc = mode->sdc();
  const Sim *sim = mode->sim();
  return !(sdc->isDisabledConstraint(from_pin)
           || sim->isConstant(from_vertex));
}

bool
SearchPred0::searchThru(Edge *edge,
                        const Mode *mode) const
{
  const TimingRole *role = edge->role();
  const Variables *variables = sta_->variables();
  const Sdc *sdc = mode->sdc();
  const Sim *sim = mode->sim();
  return !(role->isTimingCheck()
           || sdc->isDisabledConstraint(edge)
           // Constants disable edge cond expression.
           || sim->isDisabledCond(edge)
           || sdc->isDisabledCondDefault(edge)
           // Register/latch preset/clr edges are disabled by default.
           || (role == TimingRole::regSetClr()
               && !variables->presetClrArcsEnabled())
           // Constants on other pins disable this edge (ie, a mux select).
           || sim->simTimingSense(edge) == TimingSense::none
           || (edge->isBidirectInstPath()
               && !variables->bidirectInstPathsEnabled())
           || (role == TimingRole::latchDtoQ()
               && sta_->latches()->latchDtoQState(edge, mode)
               == LatchEnableState::closed));
}

bool
SearchPred0::searchTo(const Vertex *to_vertex,
                      const Mode *mode) const
{
  return !mode->sim()->isConstant(to_vertex);
}

////////////////////////////////////////////////////////////////

SearchPred1::SearchPred1(const StaState *sta) :
  SearchPred0(sta)
{
}

bool
SearchPred1::searchThru(Edge *edge,
                        const Mode *mode) const
{
  return SearchPred0::searchThru(edge, mode)
    && !edge->isDisabledLoop();
}

////////////////////////////////////////////////////////////////

ClkTreeSearchPred::ClkTreeSearchPred(const StaState *sta) :
  SearchPred(sta)
{
}

bool
ClkTreeSearchPred::searchFrom(const Vertex *from_vertex,
                              const Mode *mode) const
{
  const Pin *from_pin = from_vertex->pin();
  const Sdc *sdc = mode->sdc();
  return !sdc->isDisabledConstraint(from_pin);
}

bool
ClkTreeSearchPred::searchThru(Edge *edge,
                              const Mode *mode) const
{
  const TimingRole *role = edge->role();
  const Sdc *sdc = mode->sdc();
  return searchThruAllow(role)
    && !((role == TimingRole::tristateEnable()
          && !sta_->variables()->clkThruTristateEnabled())
         || role == TimingRole::regSetClr()
         || sdc->isDisabledConstraint(edge)
         || sdc->isDisabledCondDefault(edge)
         || edge->isBidirectInstPath()
         || edge->isDisabledLoop());
}

bool
ClkTreeSearchPred::searchThruAllow(const TimingRole *role) const
{
  return role->isWire()
    || role == TimingRole::combinational();
}

bool
isClkEnd(Vertex *vertex,
         const Mode *mode)
{
  Graph *graph = mode->graph();
  ClkTreeSearchPred pred(graph);
  VertexOutEdgeIterator edge_iter(vertex, graph);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (pred.searchThru(edge, mode))
      return false;
  }
  return true;
}

bool
ClkTreeSearchPred::searchTo(const Vertex *,
                            const Mode *) const
{
  return true;
}

////////////////////////////////////////////////////////////////

bool
searchThru(const Edge *edge,
           const TimingArc *arc,
           const Mode *mode)
{
  const Graph *graph = mode->graph();
  const RiseFall *from_rf = arc->fromEdge()->asRiseFall();
  const RiseFall *to_rf = arc->toEdge()->asRiseFall();
  // Ignore transitions other than rise/fall.
  return from_rf && to_rf
    && searchThru(edge->from(graph), from_rf, edge, edge->to(graph), to_rf, mode);
}

bool
searchThru(Vertex *from_vertex,
           const RiseFall *from_rf,
           const Edge *edge,
           Vertex *to_vertex,
           const RiseFall *to_rf,
           const Mode *mode)
{
  return searchThruTimingSense(edge, from_rf, to_rf, mode)
    && searchThruSimEdge(from_vertex, from_rf, mode)
    && searchThruSimEdge(to_vertex, to_rf, mode);
}

// set_case_analysis rising/falling filters rise/fall edges during search.
static bool
searchThruSimEdge(const Vertex *vertex,
                  const RiseFall *rf,
                  const Mode *mode)
{
  LogicValue sim_value = mode->sim()->simValue(vertex->pin());
  switch (sim_value) {
  case LogicValue::rise:
    return rf == RiseFall::rise();
  case LogicValue::fall:
    return rf == RiseFall::fall();
  default:
    return true;
  };
}

static bool
searchThruTimingSense(const Edge *edge,
                      const RiseFall *from_rf,
                      const RiseFall *to_rf,
                      const Mode *mode)
{
  switch (mode->sim()->simTimingSense(edge)) {
  case TimingSense::unknown:
    return true;
  case TimingSense::positive_unate:
    return from_rf == to_rf;
  case TimingSense::negative_unate:
    return from_rf != to_rf;
  case TimingSense::non_unate:
    return true;
  case TimingSense::none:
    return false;
  default:
    return true;
  }
}

////////////////////////////////////////////////////////////////

bool
hasFanin(Vertex *vertex,
         SearchPred *pred,
         const Graph *graph,
         const Mode *mode)
{
  if (pred->searchTo(vertex, mode)) {
    VertexInEdgeIterator edge_iter(vertex, graph);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph);
      if (pred->searchFrom(from_vertex, mode)
          && pred->searchThru(edge, mode))
        return true;
    }
  }
  return false;
}

bool
hasFanout(Vertex *vertex,
          SearchPred *pred,
          const Graph *graph,
          const Mode *mode)
{
  if (pred->searchFrom(vertex, mode)) {
    VertexOutEdgeIterator edge_iter(vertex, graph);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph);
      if (pred->searchTo(to_vertex, mode)
          && pred->searchThru(edge, mode))
        return true;
    }
  }
  return false;
}

} // namespace
