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
#include "TimingRole.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "DisabledPorts.hh"

namespace sta {

DisabledPorts::DisabledPorts() :
  all_(false),
  from_(nullptr),
  to_(nullptr),
  from_to_(nullptr)
{
}

DisabledPorts::~DisabledPorts()
{
  delete from_;
  delete to_;
  if (from_to_) {
    from_to_->deleteContents();
    delete from_to_;
  }
}

void
DisabledPorts::setDisabledAll()
{
  all_ = true;
}

void
DisabledPorts::removeDisabledAll()
{
  all_ = false;
}

void
DisabledPorts::setDisabledFrom(LibertyPort *port)
{
  if (from_ == nullptr)
    from_ = new LibertyPortSet;
  from_->insert(port);
}

void
DisabledPorts::removeDisabledFrom(LibertyPort *port)
{
  if (from_)
    from_->erase(port);
}

void
DisabledPorts::setDisabledTo(LibertyPort *port)
{
  if (to_ == nullptr)
    to_ = new LibertyPortSet;
  to_->insert(port);
}

void
DisabledPorts::removeDisabledTo(LibertyPort *port)
{
  if (to_)
    to_->erase(port);
}

void
DisabledPorts::setDisabledFromTo(LibertyPort *from, LibertyPort *to)
{
  if (from_to_ == nullptr)
    from_to_ = new LibertyPortPairSet;
  LibertyPortPair pair(from, to);
  if (!from_to_->hasKey(&pair)) {
    LibertyPortPair *pair = new LibertyPortPair(from, to);
    from_to_->insert(pair);
  }
}

void
DisabledPorts::removeDisabledFromTo(LibertyPort *from, LibertyPort *to)
{
  if (from_to_) {
    LibertyPortPair probe(from, to);
    LibertyPortPair *pair = from_to_->findKey(&probe);
    if (pair) {
      from_to_->erase(pair);
      delete pair;
    }
  }
}

bool
DisabledPorts::isDisabled(LibertyPort *from, LibertyPort *to,
			  const TimingRole *role)
{
  LibertyPortPair pair(from, to);
  // set_disable_timing instance does not disable timing checks.
  return (all_ && !role->isTimingCheck())
    || (from_ && from_->hasKey(from))
    || (to_ && to_->hasKey(to))
    || (from_to_ && from_to_->hasKey(&pair));
}

////////////////////////////////////////////////////////////////

DisabledCellPorts::DisabledCellPorts(LibertyCell *cell) :
  DisabledPorts(),
  cell_(cell),
  arc_sets_(nullptr)
{
}


DisabledCellPorts::~DisabledCellPorts()
{
  delete arc_sets_;
}

void
DisabledCellPorts::setDisabled(TimingArcSet *arc_set)
{
  if (arc_sets_ == nullptr)
    arc_sets_ = new TimingArcSetSet;
  arc_sets_->insert(arc_set);
}

void
DisabledCellPorts::removeDisabled(TimingArcSet *arc_set)
{
  if (arc_sets_)
    arc_sets_->erase(arc_set);
}

bool
DisabledCellPorts::isDisabled(TimingArcSet *arc_set) const
{
  return arc_sets_
    && arc_sets_->hasKey(arc_set);
}

class DisabledCellPortsLess
{
public:
  DisabledCellPortsLess();
  bool operator()(const DisabledCellPorts *disable1,
		  const DisabledCellPorts *disable2);
};

DisabledCellPortsLess::DisabledCellPortsLess()
{
}

bool
DisabledCellPortsLess::operator()(const DisabledCellPorts *disable1,
				  const DisabledCellPorts *disable2)
{
  return stringLess(disable1->cell()->name(),
		    disable2->cell()->name());
}

void
sortDisabledCellPortsMap(DisabledCellPortsMap *cell_map,
			 DisabledCellPortsSeq &disables)
{
  DisabledCellPortsMap::Iterator disabled_iter(cell_map);
  while (disabled_iter.hasNext())
    disables.push_back(disabled_iter.next());
  sort(disables, DisabledCellPortsLess());
}

////////////////////////////////////////////////////////////////

DisabledInstancePorts::DisabledInstancePorts(Instance *inst) :
  DisabledPorts(),
  inst_(inst)
{
}

class DisabledInstPortsLess
{
public:
  explicit DisabledInstPortsLess(Network *network);
  bool operator()(const DisabledInstancePorts *disable1,
		  const DisabledInstancePorts *disable2);

private:
  Network *network_;
};

DisabledInstPortsLess::DisabledInstPortsLess(Network *network) :
  network_(network)
{
}

bool
DisabledInstPortsLess::operator()(const DisabledInstancePorts *disable1,
				 const DisabledInstancePorts *disable2)
{
  return stringLess(network_->pathName(disable1->instance()),
		    network_->pathName(disable2->instance()));
}

void
sortDisabledInstancePortsMap(DisabledInstancePortsMap *inst_map,
			     Network *network,
			     DisabledInstancePortsSeq &disables)
{
  DisabledInstancePortsMap::Iterator disabled_iter(inst_map);
  while (disabled_iter.hasNext())
    disables.push_back(disabled_iter.next());
  sort(disables, DisabledInstPortsLess(network));
}

////////////////////////////////////////////////////////////////

class LibertyPortPairNameLess
{
public:
  bool operator()(const LibertyPortPair *pair1,
		  const LibertyPortPair *pair2);
};

bool
LibertyPortPairNameLess::operator()(const LibertyPortPair *pair1,
				    const LibertyPortPair *pair2)
{
  const char *from1 = pair1->first->name();
  const char *from2 = pair2->first->name();
  const char *to1 = pair1->second->name();
  const char *to2 = pair2->second->name();
  return stringLess(from1, from2)
    || (stringEq(from1, from2) && stringLess(to1, to2));
}

void
sortLibertyPortPairSet(LibertyPortPairSet *sets,
		       LibertyPortPairSeq &pairs)
{
  LibertyPortPairSet::Iterator pair_iter(sets);
  while (pair_iter.hasNext())
    pairs.push_back(pair_iter.next());
  sort(pairs, LibertyPortPairNameLess());
}

}
