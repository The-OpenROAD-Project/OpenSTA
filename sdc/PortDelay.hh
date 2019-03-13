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

#ifndef STA_PORT_DELAY_H
#define STA_PORT_DELAY_H

#include "DisallowCopyAssign.hh"
#include "RiseFallMinMax.hh"
#include "SdcClass.hh"

namespace sta {

class PortDelay;

typedef Vector<PortDelay*> PortDelaySeq;

// set_input_delay arrival, set_output_delay departure
class PortDelay
{
public:
  RiseFallMinMax *delays() { return &delays_; }
  Pin *pin() const { return pin_; }
  PinSet &vertexPins() { return vertex_pins_; }
  Clock *clock() const;
  ClockEdge *clkEdge() const { return clk_edge_; }
  bool sourceLatencyIncluded() const;
  void setSourceLatencyIncluded(bool included);
  bool networkLatencyIncluded() const;
  void setNetworkLatencyIncluded(bool included);
  Pin *refPin() const { return ref_pin_; }
  TransRiseFall *refTransition() const;

protected:
  PortDelay(Pin *pin, ClockEdge *clk_edge, Pin *ref_pin);

  Pin *pin_;
  ClockEdge *clk_edge_;
  bool source_latency_included_;
  bool network_latency_included_;
  Pin *ref_pin_;
  RiseFallMinMax delays_;
  PinSet vertex_pins_;

private:
  DISALLOW_COPY_AND_ASSIGN(PortDelay);
};

class InputDelay : public PortDelay
{
public:
  int index() const { return index_; }

protected:
  InputDelay(Pin *pin,
	     ClockEdge *clk_edge,
	     Pin *ref_pin,
	     int index,
	     Network *network);
  InputDelay *next() { return next_; }

  // Linked list of port delays on the same pin wrt different clocks.
  InputDelay *next_;
  void setNext(InputDelay *next);

private:
  DISALLOW_COPY_AND_ASSIGN(InputDelay);

  int index_;

  friend class Sdc;
  friend class PinInputDelayIterator;
  friend class InputDelayIterator;
};

class InputDelayVertexPinIterator : public PinSet::Iterator
{
public:
  InputDelayVertexPinIterator(InputDelay *input_delay) :
    PinSet::Iterator(input_delay->vertexPins())
  {
  }
};

class OutputDelay : public PortDelay
{
public:

protected:
  OutputDelay(Pin *pin,
	      ClockEdge *clk_edge,
	      Pin *ref_pin,
	      Network *network);
  OutputDelay *next() { return next_; }
  void setNext(OutputDelay *next);

  // Linked list of port delays on the same port wrt different clocks.
  OutputDelay *next_;

private:
  DISALLOW_COPY_AND_ASSIGN(OutputDelay);

  friend class Sdc;
  friend class PinOutputDelayIterator;
  friend class OutputDelayIterator;
};

class OutputDelayVertexPinIterator : public PinSet::Iterator
{
public:
  OutputDelayVertexPinIterator(OutputDelay *output_delay) :
    PinSet::Iterator(output_delay->vertexPins())
  {
  }
};

class PinInputDelayIterator : public Iterator<InputDelay*>
{
public:
  PinInputDelayIterator(const Pin *pin,
			const Sdc *sdc);
  virtual bool hasNext();
  virtual InputDelay *next();

protected:
  PinInputDelayIterator() {}

  InputDelay *next_;

  DISALLOW_COPY_AND_ASSIGN(PinInputDelayIterator);
};

class VertexPinInputDelayIterator : public PinInputDelayIterator
{
public:
  VertexPinInputDelayIterator(const Pin *vertex_pin,
			      const Sdc *sdc);

private:
  DISALLOW_COPY_AND_ASSIGN(VertexPinInputDelayIterator);
};

class PinOutputDelayIterator : public Iterator<OutputDelay*>
{
public:
  PinOutputDelayIterator(const Pin *pin,
			 const Sdc *sdc);
  virtual bool hasNext();
  virtual OutputDelay *next();

protected:
  PinOutputDelayIterator() {}

  OutputDelay *next_;

  DISALLOW_COPY_AND_ASSIGN(PinOutputDelayIterator);
};

class VertexPinOutputDelayIterator : public PinOutputDelayIterator
{
public:
  VertexPinOutputDelayIterator(const Pin *vertex_pin,
			       const Sdc *sdc);

private:
  DISALLOW_COPY_AND_ASSIGN(VertexPinOutputDelayIterator);
};

// Prediate used to sort port delays.
class PortDelayLess
{
public:
  explicit PortDelayLess(const Network *network);
  bool operator()(const PortDelay *delay1,
		  const PortDelay *delay2) const;

private:
  const Network *network_;
};

} // namespace
#endif
