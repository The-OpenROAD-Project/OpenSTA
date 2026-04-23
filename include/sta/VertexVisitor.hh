// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include "GraphClass.hh"
#include "NetworkClass.hh"

namespace sta {

// Abstract base class for visiting a vertex.
class VertexVisitor
{
public:
  virtual ~VertexVisitor() = default;
  virtual VertexVisitor *copy() const = 0;
  virtual void visit(Vertex *vertex) = 0;
  void operator()(Vertex *vertex) { visit(vertex); }
  // If true, Kahn's Stage 1 discovery and successor decrement must
  // stop at register-CK boundaries. Mirrors the clks_only branch in
  // ArrivalVisitor::visit (postponeClkFanouts) at the discovery
  // layer so Kahn's active set matches the narrowed one level-BFS
  // walks under findClkArrivals. Kahn's itself still runs; only the
  // reg-CK fanout is excluded.
  virtual bool stopDiscoveryAtRegClk() const { return false; }
  // Reports whether the most recent visit() call produced a change
  // worth propagating (e.g. ArrivalVisitor returns whether arrivals
  // changed). Consulted only when Kahn's visit-skip toggle
  // (sta_kahn_visit_skip) is on; the default "true" means every
  // visitor is treated as always-changing, so the skip optimization
  // safely no-ops for visitors that haven't opted in.
  virtual bool lastVisitChanged() const { return true; }
};

// Collect visited pins into a PinSet.
class VertexPinCollector : public VertexVisitor
{
public:
  VertexPinCollector(PinSet &pins);
  const PinSet &pins() const { return pins_; }
  void visit(Vertex *vertex) override;
  VertexVisitor *copy() const override;

protected:
  PinSet &pins_;
};

} // namespace sta
