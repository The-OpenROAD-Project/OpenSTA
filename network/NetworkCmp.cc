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

#include <algorithm>
#include "Machine.hh"
#include "StringUtil.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "NetworkCmp.hh"

namespace sta {

PortNameLess::PortNameLess(const Network *network) :
  network_(network)
{
}

bool
PortNameLess::operator()(const Port *port1,
			 const Port *port2) const
{
  return stringLess(network_->name(port1), network_->name(port2));
}

PinPathNameLess::PinPathNameLess(const Network *network) :
  network_(network)
{
}

bool
PinPathNameLess::operator()(const Pin *pin1,
			    const Pin *pin2) const
{
  return network_->pathNameLess(pin1, pin2);
}

NetPathNameLess::NetPathNameLess(const Network *network) :
  network_(network)
{
}

bool
NetPathNameLess::operator()(const Net *net1,
			    const Net *net2) const
{
  return network_->pathNameLess(net1, net2);
}

InstancePathNameLess::InstancePathNameLess(const Network *network) :
  network_(network)
{
}

bool
InstancePathNameLess::operator()(const Instance *inst1,
				 const Instance *inst2) const
{
  return network_->pathNameLess(inst1, inst2);
}

////////////////////////////////////////////////////////////////

void
sortPinSet(PinSet *set,
	   const Network *network,
	   PinSeq &pins)
{
  PinSet::Iterator pin_iter(set);
  while (pin_iter.hasNext())
    pins.push_back(pin_iter.next());
  sort(pins, PinPathNameLess(network));
}

void
sortPortSet(PortSet *set,
	    const Network *network,
	    PortSeq &ports)
{
  PortSet::Iterator port_iter(set);
  while (port_iter.hasNext())
    ports.push_back(port_iter.next());
  sort(ports, PortNameLess(network));
}

void
sortInstanceSet(InstanceSet *set,
		const Network *network,
		InstanceSeq &insts)
{
  InstanceSet::Iterator inst_iter(set);
  while (inst_iter.hasNext())
    insts.push_back(inst_iter.next());
  // Sort ports so regression results are portable.
  sort(insts, InstancePathNameLess(network));
}

void
sortNetSet(NetSet *set,
	   const Network *network,
	   NetSeq &nets)
{
  NetSet::Iterator net_iter(set);
  while (net_iter.hasNext())
    nets.push_back(net_iter.next());
  // Sort nets so regression results are netable.
  sort(nets, NetPathNameLess(network));
}

} // namespace
