// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
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

#include "Sta.hh"

#include "Network.hh"
#include "Graph.hh"
#include "Bfs.hh"
#include "Sdc.hh"
#include "SearchPred.hh"
#include "Search.hh"

namespace sta {

void
Sta::ensureClkPins()
{
  if (!clk_pins_valid_) {
    ensureLevelized();
    findClkPins();
    clk_pins_valid_ = true;
  }
}

void
Sta::clkPinsInvalid()
{
  clk_pins_valid_ = false;
  clk_pins_.clear();
  ideal_clk_pins_.clear();
}

void
Sta::clkPinsDisconnectPinBefore(Vertex *vertex)
{
  // If the pin is in the clock network but not a clock endpoint
  // everything downstream is invalid.
  if (clk_pins_.hasKey(vertex->pin())
      && !isClkEnd(vertex, graph_))
    clk_pins_valid_ = false;
}

void
Sta::clkPinsConnectPinAfter(Vertex *vertex)
{
  // If the pin fanin is part of the clock network
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *from = edge->from(graph_);
    if (clk_pins_.hasKey(from->pin())) {
      clk_pins_valid_ = false;
      break;
    }
  }
}

// Find clock network pins.
// This is not as reliable as Search::isClock but is much cheaper.
void
Sta::findClkPins()
{
  // Use two passes to find ideal and propagated clock network pins.
  findClkPins(false, clk_pins_);
  findClkPins(true, ideal_clk_pins_);
}

void
Sta::findClkPins(bool ideal_only,
		 PinSet &clk_pins)
{
  ClkArrivalSearchPred srch_pred(this);
  BfsFwdIterator bfs(BfsIndex::other, &srch_pred, this);
  for (Clock *clk : sdc_->clks()) {
    if (!ideal_only
	|| !clk->isPropagated()) {
      for (Pin *pin : clk->leafPins()) {
	if (!ideal_only
	    || !sdc_->isPropagatedClock(pin)) {
	  Vertex *vertex, *bidirect_drvr_vertex;
	  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
	  bfs.enqueue(vertex);
	  if (bidirect_drvr_vertex)
	    bfs.enqueue(bidirect_drvr_vertex);
	}
      }
    }
  }
  while (bfs.hasNext()) {
    Vertex *vertex = bfs.next();
    Pin *pin = vertex->pin();
    if (!sdc_->isPropagatedClock(pin)) {
      clk_pins.insert(pin);
      bfs.enqueueAdjacentVertices(vertex);
    }
  }
}

bool
Sta::isClock(Pin *pin) const
{
  return clk_pins_.hasKey(pin);
}

bool
Sta::isIdealClock(Pin *pin) const
{
  return ideal_clk_pins_.hasKey(pin);
}

} // namespace
