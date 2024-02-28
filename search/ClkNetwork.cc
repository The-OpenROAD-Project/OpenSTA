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

#include "ClkNetwork.hh"

#include "Debug.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Bfs.hh"
#include "Sdc.hh"
#include "SearchPred.hh"
#include "Search.hh"

namespace sta {

ClkNetwork::ClkNetwork(StaState *sta) :
  StaState(sta),
  clk_pins_valid_(false)
{
}

ClkNetwork::~ClkNetwork()
{
  clk_pins_map_.deleteContentsClear();
}

void
ClkNetwork::ensureClkNetwork()
{
  if (!clk_pins_valid_)
    findClkPins();
}

void
ClkNetwork::clear()
{
  clk_pins_valid_ = false;
  pin_clks_map_.clear();
  clk_pins_map_.deleteContentsClear();
  pin_ideal_clks_map_.clear();
}

void
ClkNetwork::clkPinsInvalid()
{
  debugPrint(debug_, "clk_network", 1, "clk network invalid");
  clk_pins_valid_ = false;
}

void
ClkNetwork::deletePinBefore(const Pin *pin)
{
  if (isClock(pin))
    clkPinsInvalid();
}

void
ClkNetwork::disconnectPinBefore(const Pin *pin)
{
  if (isClock(pin))
    clkPinsInvalid();
}

void
ClkNetwork::connectPinAfter(const Pin *pin)
{
  if (isClock(pin))
    clkPinsInvalid();
}

class ClkSearchPred : public ClkTreeSearchPred
{
public:
  ClkSearchPred(const StaState *sta);
  virtual bool searchTo(const Vertex *to);
};

ClkSearchPred::ClkSearchPred(const StaState *sta) :
  ClkTreeSearchPred(sta)
{
}

bool
ClkSearchPred::searchTo(const Vertex *to)
{
  const Sdc *sdc = sta_->sdc();
  return !sdc->isLeafPinClock(to->pin());
}

void
ClkNetwork::findClkPins()
{
  debugPrint(debug_, "clk_network", 1, "find clk network");
  clear();
  findClkPins(false, pin_clks_map_);
  findClkPins(true, pin_ideal_clks_map_);
  clk_pins_valid_ = true;
}

void
ClkNetwork::findClkPins(bool ideal_only,
			PinClksMap &pin_clks_map)
{
  ClkSearchPred srch_pred(this);
  BfsFwdIterator bfs(BfsIndex::other, &srch_pred, this);
  for (Clock *clk : sdc_->clks()) {
    if (!ideal_only
	|| !clk->isPropagated()) {
      PinSet *clk_pins = clk_pins_map_[clk];
      if (clk_pins == nullptr) {
        clk_pins = new PinSet(network_);
        clk_pins_map_[clk] = clk_pins;
      }
      for (const Pin *pin : clk->leafPins()) {
	if (!ideal_only
	    || !sdc_->isPropagatedClock(pin)) {
	  Vertex *vertex, *bidirect_drvr_vertex;
	  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
	  bfs.enqueue(vertex);
	  if (bidirect_drvr_vertex)
	    bfs.enqueue(bidirect_drvr_vertex);
	}
      }
      while (bfs.hasNext()) {
	Vertex *vertex = bfs.next();
	const Pin *pin = vertex->pin();
	if (!ideal_only
	    || !sdc_->isPropagatedClock(pin)) {
	  clk_pins->insert(pin);
	  ClockSet &pin_clks = pin_clks_map[pin];
          pin_clks.insert(clk);
	  bfs.enqueueAdjacentVertices(vertex);
	}
      }
    }
  }
}

bool
ClkNetwork::isClock(const Pin *pin) const
{
  return network_->isRegClkPin(pin)
    || pin_clks_map_.hasKey(pin);
}

bool
ClkNetwork::isClock(const Net *net) const
{
  bool is_clk = false;
  NetConnectedPinIterator *pin_iter = network_->pinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (isClock(pin)) {
      is_clk = true;
      break;
    }
  }
  delete pin_iter;
  return is_clk;
}

bool
ClkNetwork::isIdealClock(const Pin *pin) const
{
  return pin_ideal_clks_map_.hasKey(pin);
}

bool
ClkNetwork::isPropagatedClock(const Pin *pin) const
{
  return pin_clks_map_.hasKey(pin)
    && !pin_ideal_clks_map_.hasKey(pin);
}

const ClockSet *
ClkNetwork::clocks(const Pin *pin)
{
  if (pin_clks_map_.hasKey(pin))
    return &pin_clks_map_[pin];
  else
    return nullptr;
}

const ClockSet *
ClkNetwork::idealClocks(const Pin *pin)
{
  if (pin_ideal_clks_map_.hasKey(pin))
    return &pin_ideal_clks_map_[pin];
  else
    return nullptr;
}

const PinSet *
ClkNetwork::pins(const Clock *clk)
{
  if (clk_pins_map_.hasKey(clk))
    return clk_pins_map_[clk];
  else
    return nullptr;
}

float
ClkNetwork::idealClkSlew(const Pin *pin,
                         const RiseFall *rf,
                         const MinMax *min_max)
{
  const ClockSet *clks = clk_network_->idealClocks(pin);
  if (clks && !clks->empty()) {
    float slew = min_max->initValue();
    ClockSet::ConstIterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
      float clk_slew = clk->slew(rf, min_max);
      if (min_max->compare(clk_slew, slew))
        slew = clk_slew;
    }
    return slew;
  }
  else
    return 0.0;
}

} // namespace
