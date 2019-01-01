// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include "Machine.hh"
#include "Sdc.hh"
#include "Network.hh"
#include "PortDelay.hh"

namespace sta {

PortDelay::PortDelay(Pin *pin,
		     ClockEdge *clk_edge,
		     Pin *ref_pin) :
  pin_(pin),
  clk_edge_(clk_edge),
  source_latency_included_(false),
  network_latency_included_(false),
  ref_pin_(ref_pin),
  delays_()
{
}

Clock *
PortDelay::clock() const
{
  if (clk_edge_)
    return clk_edge_->clock();
  else
    return NULL;
}

bool
PortDelay::sourceLatencyIncluded() const
{
  return source_latency_included_;
}

void
PortDelay::setSourceLatencyIncluded(bool included)
{
  source_latency_included_ = included;
}

bool
PortDelay::networkLatencyIncluded() const
{
  return network_latency_included_;
}

void
PortDelay::setNetworkLatencyIncluded(bool included)
{
  network_latency_included_ = included;
}

TransRiseFall *
PortDelay::refTransition() const
{
  // Reference pin transition is the clock transition.
  if (clk_edge_)
    return clk_edge_->transition();
  else
    return TransRiseFall::rise();
}

InputDelay::InputDelay(Pin *pin,
		       ClockEdge *clk_edge,
		       Pin *ref_pin,
		       int index,
		       Network *network) :
  PortDelay(pin, clk_edge, ref_pin),
  next_(NULL),
  index_(index)
{
  findVertexLoadPins(pin, network, &vertex_pins_);
}

void
InputDelay::setNext(InputDelay *next)
{
  next_ = next;
}

OutputDelay::OutputDelay(Pin *pin,
			 ClockEdge *clk_edge,
			 Pin *ref_pin,
			 Network *network) :
  PortDelay(pin, clk_edge, ref_pin),
  next_(NULL)
{
  if (network)
    findVertexDriverPins(pin, network, &vertex_pins_);
}

void
OutputDelay::setNext(OutputDelay *next)
{
  next_ = next;
}

////////////////////////////////////////////////////////////////

PinInputDelayIterator::PinInputDelayIterator(InputDelay *input_delay) :
  next_(input_delay)
{
}

bool
PinInputDelayIterator::hasNext()
{
  return next_ != NULL;
}

InputDelay *
PinInputDelayIterator::next()
{
  InputDelay *next = next_;
  if (next)
    next_ = next_->next();
  return next;
}

////////////////////////////////////////////////////////////////

PinOutputDelayIterator::PinOutputDelayIterator(OutputDelay *output_delay) :
  next_(output_delay)
{
}

bool
PinOutputDelayIterator::hasNext()
{
  return next_ != NULL;
}

OutputDelay *
PinOutputDelayIterator::next()
{
  OutputDelay *next = next_;
  if (next)
    next_ = next_->next();
  return next;
}

////////////////////////////////////////////////////////////////

PortDelayLess::PortDelayLess(const Network *network) :
  network_(network)
{
}

bool
PortDelayLess::operator() (const PortDelay *delay1,
			   const PortDelay *delay2) const
{
  Pin *pin1 = delay1->pin();
  Pin *pin2 = delay2->pin();
  int pin_cmp = network_->pathNameCmp(pin1, pin2);
  if (pin_cmp < 0)
    return true;
  else if (pin_cmp > 0)
    return false;
  else
    return clkEdgeLess(delay1->clkEdge(), delay2->clkEdge());
}

} // namespace
