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

#pragma once

#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "LibertyClass.hh"
#include "StaState.hh"

namespace sta {

// Class hierarchy:
// SearchPred
//  SearchAdj (unless loop disabled, latch D->Q, timing check, dynamic loop)
//  SearchPred0 (unless disabled or constant)
//   EvalPred (unless timing check)
//    SearchThru (unless latch D->Q)
//   SearchPred1 (unless loop disabled)
//  ClkTreeSearchPred (only wire or combinational)

// Virtual base class for search predicates.
class SearchPred
{
public:
  SearchPred(const StaState *sta);
  virtual ~SearchPred() {}
  // Search is allowed from from_vertex.
  virtual bool searchFrom(const Vertex *from_vertex,
                          const Mode *mode) const = 0;
  bool searchFrom(const Vertex *from_vertex) const;
  // Search is allowed through edge.
  // from/to pins are NOT checked.
  // inst can be either the from_pin or to_pin instance because it
  // is only referenced when they are the same (non-wire edge).
  virtual bool searchThru(Edge *edge,
                          const Mode *mode) const = 0;
  bool searchThru(Edge *edge) const;
  // Search is allowed to to_pin.
  virtual bool searchTo(const Vertex *to_vertex,
                        const Mode *mode) const = 0;
  bool searchTo(const Vertex *to_vertex) const;
  void copyState(const StaState *sta);

protected:
  const StaState *sta_;
};

class SearchPred0 : public SearchPred
{
public:
  SearchPred0(const StaState *sta);
  // Search from a vertex unless
  //  disabled by constraint
  //  constant logic zero/one
  bool searchFrom(const Vertex *from_vertex,
                  const Mode *mode) const override;
  // Search thru an edge unless
  //  traverses disabled from/to pin pair
  //  disabled by condition expression
  //  wire that traverses a disabled hierarchical pin
  //  register set/reset edge (and search thru them is disabled)
  //  cond expression is disabled
  //  non-controlling constant values on other pins that disable the
  //    edge (such as a mux select)
  bool searchThru(Edge *edge,
                  const Mode *mode) const override;
  // Search to a vertex unless
  //  constant logic zero/one
  bool searchTo(const Vertex *to_vertex,
                const Mode *mode) const override;
};

// SearchPred0 unless
//  disabled to break combinational loop
class SearchPred1 : public SearchPred0
{
public:
  SearchPred1(const StaState *sta);
  bool searchThru(Edge *edge,
                  const Mode *mode) const override;

  using SearchPred::searchFrom;
  using SearchPred::searchThru;
  using SearchPred::searchTo;
};

// Predicate for BFS search to stop at the end of the clock tree.
// Search only thru combinational gates and wires.
class ClkTreeSearchPred : public SearchPred
{
public:
  ClkTreeSearchPred(const StaState *sta);
  bool searchFrom(const Vertex *from_vertex,
                  const Mode *mode) const override;
  bool searchThru(Edge *edge,
                  const Mode *mode) const override;
  // The variable part of searchThru used by descendents.
  virtual bool searchThruAllow(const TimingRole *role) const;
  bool searchTo(const Vertex *to_vertex,
                const Mode *mode) const override;
};

bool
isClkEnd(Vertex *vertex,
         const Mode *mode);

// Predicate to see if arc/edge is disabled by constants on other pins
// that effect the unateness of the edge.
bool
searchThru(const Edge *edge,
           const TimingArc *arc,
           const Mode *mode);
bool
searchThru(Vertex *from_vertex,
           const RiseFall *from_rf,
           const Edge *edge,
           Vertex *to_vertex,
           const RiseFall *to_rf,
           const Mode *mode);

////////////////////////////////////////////////////////////////

bool
hasFanin(Vertex *vertex,
         SearchPred *pred,
         const Graph *graph,
         const Mode *mode);

// Vertices with no fanout have at no enabled (non-disabled) edges
// leaving them.
bool
hasFanout(Vertex *vertex,
          SearchPred *pred,
          const Graph *graph,
          const Mode *mode);

} // namespace
