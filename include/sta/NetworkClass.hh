// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>

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

using LibraryIterator = Iterator<Library*>;
using LibertyLibraryIterator = Iterator<LibertyLibrary*>;
using CellSeq = std::vector<Cell*>;
using PortSeq = std::vector<const Port*>;
using CellPortIterator = Iterator<Port*>;
using CellPortBitIterator = Iterator<Port*>;
using PortMemberIterator = Iterator<Port*>;

using PinSeq = std::vector<const Pin*>;
using PinUnorderedSet = std::unordered_set<const Pin*>;
using InstanceSeq = std::vector<const Instance*>;
using NetSeq = std::vector<const Net*>;
using ConstNetSeq = std::vector<const Net*>;
using InstanceChildIterator = Iterator<Instance*>;
using InstancePinIterator = Iterator<Pin*>;
using InstanceNetIterator = Iterator<Net*>;
using LeafInstanceIterator = Iterator<Instance*>;
using NetIterator = Iterator<Net*>;
using NetPinIterator = Iterator<const Pin*>;
using NetTermIterator = Iterator<Term*>;
using ConnectedPinIterator = Iterator<const Pin*>;
using NetConnectedPinIterator = ConnectedPinIterator;
using PinConnectedPinIterator = ConnectedPinIterator;
using ObjectId = uint32_t;
using AttributeMap = std::map<std::string, std::string>;

enum class LogicValue : unsigned { zero, one, unknown, rise, fall };

class CellIdLess
{
public:
  CellIdLess(const Network *network);
  bool operator()(const Cell *cell1,
                  const Cell *cell2) const;
private:
  const Network *network_;
};

class PortIdLess
{
public:
  PortIdLess(const Network *network);
  bool operator()(const Port *port1,
                  const Port *port2) const;
private:
  const Network *network_;
};

class InstanceIdLess
{
public:
  InstanceIdLess(const Network *network);
  bool operator()(const Instance *inst1,
                  const Instance *inst2) const;
private:
  const Network *network_;
};

class PinIdLess
{
public:
  PinIdLess(const Network *network);
  bool operator()(const Pin *pin1,
                  const Pin *pin2) const;
private:
  const Network *network_;
};

class PinIdHash
{
public:
  PinIdHash(const Network *network);
  size_t operator()(const Pin *pin) const;

private:
  const Network *network_;
};

class NetIdLess
{
public:
  NetIdLess(const Network *network);
  bool operator()(const Net *net1,
                  const Net *net2) const;
private:
  const Network *network_;
};

////////////////////////////////////////////////////////////////

class CellSet : public std::set<const Cell*, CellIdLess>
{
public:
  CellSet(const Network *network);
};

class PortSet : public std::set<const Port*, PortIdLess>
{
public:
  PortSet(const Network *network);
};

class InstanceSet : public std::set<const Instance*, InstanceIdLess>
{
public:
  InstanceSet();
  InstanceSet(const Network *network);
};

class PinSet : public std::set<const Pin*, PinIdLess>
{
public:
  PinSet();
  PinSet(const Network *network);
};

class NetSet : public std::set<const Net*, NetIdLess>
{
public:
  NetSet();
  NetSet(const Network *network);
};

} // namespace
