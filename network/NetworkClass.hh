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

#include "Set.hh"
#include "Vector.hh"
#include "Iterator.hh"

namespace sta {

class Network;
class NetworkEdit;
class NetworkReader;
class Library;
class Cell;
class Port;
class PortDirection;
class Instance;
class Pin;
class Term;
class Net;
class ConstantPinIterator;
class ViewType;
class LibertyLibrary;

typedef Iterator<Library*> LibraryIterator;
typedef Iterator<LibertyLibrary*> LibertyLibraryIterator;
typedef Vector<Cell*> CellSeq;
typedef Set<Cell*> CellSet;
typedef Vector<Port*> PortSeq;
typedef Set<Port*> PortSet;
typedef Iterator<Port*> CellPortIterator;
typedef Iterator<Port*> CellPortBitIterator;
typedef Iterator<Port*> PortMemberIterator;
typedef std::pair<Port*, Port*> PortPair;

typedef Vector<Pin*> PinSeq;
typedef Vector<const Pin*> ConstPinSeq;
typedef Vector<Instance*> InstanceSeq;
typedef Vector<const Instance*> ConstInstanceSeq;
typedef Vector<Net*> NetSeq;
typedef Set<Pin*> PinSet;
typedef Set<Instance*> InstanceSet;
typedef Set<Net*> NetSet;
typedef Iterator<Instance*> InstanceChildIterator;
typedef Iterator<Pin*> InstancePinIterator;
typedef Iterator<Net*> InstanceNetIterator;
typedef Iterator<Instance*> LeafInstanceIterator;
typedef Iterator<Net*> NetIterator;
typedef Iterator<Pin*> NetPinIterator;
typedef Iterator<Term*> NetTermIterator;
typedef Iterator<Pin*> ConnectedPinIterator;
typedef ConnectedPinIterator NetConnectedPinIterator;
typedef ConnectedPinIterator PinConnectedPinIterator;

class PortPairLess
{
public:
  bool operator()(const PortPair *pair1,
		  const PortPair *pair2) const;
};

enum class LogicValue : unsigned { zero, one, unknown, rise, fall };

} // namespace
