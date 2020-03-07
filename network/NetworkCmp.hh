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

#pragma once

#include "LibertyClass.hh"
#include "NetworkClass.hh"

namespace sta {

// Comparison operators for sorting network objects.

class PortNameLess
{
public:
  explicit PortNameLess(const Network *network);
  bool operator()(const Port *port1,
		  const Port *port2) const;

private:
  const Network *network_;
};

class PinPathNameLess
{
public:
  explicit PinPathNameLess(const Network *network);
  bool operator()(const Pin *pin1,
		  const Pin *pin2) const;

private:
  const Network *network_;
};

class InstancePathNameLess
{
public:
  explicit InstancePathNameLess(const Network *network);
  bool operator()(const Instance *inst1,
		  const Instance *inst2) const;

private:
  const Network *network_;
};

class NetPathNameLess
{
public:
  explicit NetPathNameLess(const Network *network);
  bool operator()(const Net *net1,
		  const Net *net2) const;

private:
  const Network *network_;
};

void
sortPinSet(PinSet *set,
	   const Network *network,
	   PinSeq &pins);
void
sortPortSet(PortSet *set,
	    const Network *network,
	    PortSeq &ports);
void
sortInstanceSet(InstanceSet *set,
		const Network *network,
		InstanceSeq &insts);
void
sortNetSet(NetSet *set,
	   const Network *network,
	   NetSeq &nets);

} // namespace
