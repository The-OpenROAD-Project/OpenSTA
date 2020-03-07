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

#include "DisallowCopyAssign.hh"
#include "Map.hh"
#include "NetworkClass.hh"
#include "LibertyClass.hh"
#include "SdcClass.hh"

namespace sta {

class TimingRole;
class DisabledCellPorts;
class DisabledInstancePorts;

typedef Vector<DisabledInstancePorts*> DisabledInstancePortsSeq;
typedef Vector<DisabledCellPorts*> DisabledCellPortsSeq;
typedef Vector<LibertyPortPair*> LibertyPortPairSeq;
typedef Set<TimingArcSet*> TimingArcSetSet;

// Base class for disabled cell and instance ports.
class DisabledPorts
{
public:
  DisabledPorts();
  ~DisabledPorts();
  void setDisabledAll();
  void removeDisabledAll();
  void setDisabledFrom(LibertyPort *port);
  void removeDisabledFrom(LibertyPort *port);
  void setDisabledTo(LibertyPort *port);
  void removeDisabledTo(LibertyPort *port);
  void setDisabledFromTo(LibertyPort *from, LibertyPort *to);
  void removeDisabledFromTo(LibertyPort *from, LibertyPort *to);
  bool isDisabled(LibertyPort *from, LibertyPort *to, const TimingRole *role);
  LibertyPortPairSet *fromTo() const { return from_to_; }
  LibertyPortSet *from() const { return from_; }
  LibertyPortSet *to() const { return to_; }
  bool all() const { return all_; }

private:
  DISALLOW_COPY_AND_ASSIGN(DisabledPorts);

  bool all_;
  LibertyPortSet *from_;
  LibertyPortSet *to_;
  LibertyPortPairSet *from_to_;
};

// set_disable_timing cell [-from] [-to]
class DisabledCellPorts : public DisabledPorts
{
public:
  DisabledCellPorts(LibertyCell *cell);
  ~DisabledCellPorts();
  LibertyCell *cell() const { return cell_; }
  void setDisabled(TimingArcSet *arc_set);
  void removeDisabled(TimingArcSet *arc_set);
  bool isDisabled(TimingArcSet *arc_set) const;
  TimingArcSetSet *timingArcSets() const { return arc_sets_; }

  using DisabledPorts::isDisabled;

private:
  DISALLOW_COPY_AND_ASSIGN(DisabledCellPorts);

  LibertyCell *cell_;
  TimingArcSetSet *arc_sets_;
};

// set_disable_timing instance [-from] [-to]
class DisabledInstancePorts : public DisabledPorts
{
public:
  DisabledInstancePorts(Instance *inst);
  Instance *instance() const { return inst_; }

private:
  DISALLOW_COPY_AND_ASSIGN(DisabledInstancePorts);

  Instance *inst_;
};

void
sortDisabledCellPortsMap(DisabledCellPortsMap *cell_map,
			 DisabledCellPortsSeq &disables);
void
sortDisabledInstancePortsMap(DisabledInstancePortsMap *inst_map,
			     Network *network,
			     DisabledInstancePortsSeq &disables);
void
sortLibertyPortPairSet(LibertyPortPairSet *sets,
		       LibertyPortPairSeq &pairs);

} // namespace
