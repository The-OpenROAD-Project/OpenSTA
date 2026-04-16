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

#include "DisabledPorts.hh"

#include <algorithm>

#include "Liberty.hh"
#include "Network.hh"
#include "StringUtil.hh"
#include "TimingRole.hh"

namespace sta {

DisabledPorts::~DisabledPorts()
{
  delete from_;
  delete to_;
  delete from_to_;
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
DisabledPorts::setDisabledFromTo(LibertyPort *from,
                                 LibertyPort *to)
{
  if (from_to_ == nullptr)
    from_to_ = new LibertyPortPairSet;
  from_to_->insert({from, to});
}

void
DisabledPorts::removeDisabledFromTo(LibertyPort *from,
                                    LibertyPort *to)
{
  if (from_to_)
    from_to_->erase({from, to});
}

bool
DisabledPorts::isDisabled(LibertyPort *from,
                          LibertyPort *to,
                          const TimingRole *role)
{
  // set_disable_timing instance does not disable timing checks.
  return (all_ && !role->isTimingCheck())
    || (from_ && from_->contains(from))
    || (to_ && to_->contains(to))
    || (from_to_ && from_to_->contains({from, to}));
}

////////////////////////////////////////////////////////////////

DisabledCellPorts::DisabledCellPorts(LibertyCell *cell) :
  cell_(cell)
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
    && arc_sets_->contains(arc_set);
}

class DisabledCellPortsLess
{
public:
  bool operator()(const DisabledCellPorts *disable1,
                  const DisabledCellPorts *disable2);
};

bool
DisabledCellPortsLess::operator()(const DisabledCellPorts *disable1,
                                  const DisabledCellPorts *disable2)
{
  return disable1->cell()->name() < disable2->cell()->name();
}

DisabledCellPortsSeq
sortByName(const DisabledCellPortsMap *cell_map)
{
  DisabledCellPortsSeq disables;
  for (auto cell_disable : *cell_map) {
    DisabledCellPorts *disable = cell_disable.second;
    disables.push_back(disable);
  }
  sort(disables, DisabledCellPortsLess());
  return disables;
}

////////////////////////////////////////////////////////////////

DisabledInstancePorts::DisabledInstancePorts(Instance *inst) :
  inst_(inst)
{
}

class DisabledInstPortsLess
{
public:
  DisabledInstPortsLess(const Network *network);
  bool operator()(const DisabledInstancePorts *disable1,
                  const DisabledInstancePorts *disable2);

private:
  const Network *network_;
};

DisabledInstPortsLess::DisabledInstPortsLess(const Network *network) :
  network_(network)
{
}

bool
DisabledInstPortsLess::operator()(const DisabledInstancePorts *disable1,
                                  const DisabledInstancePorts *disable2)
{
  return network_->pathName(disable1->instance()) <
    network_->pathName(disable2->instance());
}

DisabledInstancePortsSeq
sortByPathName(const DisabledInstancePortsMap *inst_map,
               const Network *network)
{
  DisabledInstancePortsSeq disables;
  for (auto inst_disable : *inst_map) {
    DisabledInstancePorts *disable = inst_disable.second;
    disables.push_back(disable);
  }
  sort(disables, DisabledInstPortsLess(network));
  return disables;
}

////////////////////////////////////////////////////////////////

class LibertyPortPairNameLess
{
public:
  bool operator()(const LibertyPortPair &pair1,
                  const LibertyPortPair &pair2);
};

bool
LibertyPortPairNameLess::operator()(const LibertyPortPair &pair1,
                                    const LibertyPortPair &pair2)
{
  const std::string &from1 = pair1.first->name();
  const std::string &from2 = pair2.first->name();
  const std::string &to1 = pair1.second->name();
  const std::string &to2 = pair2.second->name();
  return from1 < from2
    || (from1 == from2 && to1 < to2);
}

LibertyPortPairSeq
sortByName(const LibertyPortPairSet *set)
{
  LibertyPortPairSeq pairs;
  for (const LibertyPortPair &pair : *set)
    pairs.push_back(pair);
  sort(pairs, LibertyPortPairNameLess());
  return pairs;
}

} // namespace sta
