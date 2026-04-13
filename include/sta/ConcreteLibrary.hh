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

#pragma once

#include <functional>
#include <string_view>
#include <vector>
#include <map>

#include "StringUtil.hh"
#include "NetworkClass.hh"

// The classes defined in this file are a contrete implementation of
// the library API.  They can be used by a reader to construct classes
// that implement the library portion of the network API.

namespace sta {

class ConcreteLibrary;
class ConcreteCell;
class ConcretePort;
class ConcreteCellPortBitIterator;
class PatternMatch;
class LibertyCell;
class LibertyPort;

using ConcreteCellMap = std::map<std::string, ConcreteCell*, std::less<>>;
using ConcretePortSeq = std::vector<ConcretePort*>;
using ConcretePortMap = std::map<std::string, ConcretePort*, std::less<>>;
using ConcreteLibraryCellIterator = MapIterator<ConcreteCellMap, ConcreteCell*>;
using ConcreteCellPortIterator = VectorIterator<ConcretePortSeq, ConcretePort*>;
using ConcretePortMemberIterator = VectorIterator<ConcretePortSeq, ConcretePort*>;

class ConcreteLibrary
{
public:
  ConcreteLibrary(std::string_view name,
                  std::string_view filename,
                  bool is_liberty);
  virtual ~ConcreteLibrary();
  const std::string &name() const { return name_; }
  ObjectId id() const { return id_; }
  bool isLiberty() const { return is_liberty_; }
  const std::string &filename() const { return filename_; }
  void addCell(ConcreteCell *cell);
  ConcreteCell *makeCell(std::string_view name,
                         bool is_leaf,
                         std::string_view filename);
  void deleteCell(ConcreteCell *cell);
  ConcreteLibraryCellIterator *cellIterator() const;
  ConcreteCell *findCell(std::string_view name) const;
  CellSeq findCellsMatching(const PatternMatch *pattern) const;
  char busBrktLeft() const { return bus_brkt_left_; }
  char busBrktRight() const { return bus_brkt_right_; }
  void setBusBrkts(char left,
                   char right);

protected:
  void removeCell(ConcreteCell *cell);

  std::string name_;
  ObjectId id_;
  std::string filename_;
  bool is_liberty_;
  char bus_brkt_left_;
  char bus_brkt_right_;
  ConcreteCellMap cell_map_;

private:
  friend class ConcreteCell;
};

class ConcreteCell
{
public:
  // Use ConcreteLibrary::deleteCell.
  virtual ~ConcreteCell();
  const std::string &name() const { return name_; }
  ObjectId id() const { return id_; }
  const std::string &filename() const { return filename_; }
  ConcreteLibrary *library() const { return library_; }
  LibertyCell *libertyCell() const { return liberty_cell_; }
  void setLibertyCell(LibertyCell *cell);
  void *extCell() const { return ext_cell_; }
  void setExtCell(void *ext_cell);
  int portBitCount() const { return port_bit_count_; }
  ConcretePort *findPort(std::string_view name) const;
  PortSeq findPortsMatching(const PatternMatch *pattern) const;
  ConcreteCellPortIterator *portIterator() const;
  ConcreteCellPortBitIterator *portBitIterator() const;
  bool isLeaf() const { return is_leaf_; }
  void setIsLeaf(bool is_leaf);
  void setAttribute(std::string_view key,
                    std::string_view value);
  std::string getAttribute(std::string_view key) const;
  const AttributeMap &attributeMap() const { return attribute_map_; }

  // Cell acts as port factory.
  ConcretePort *makePort(std::string_view name);
  // Bus port.
  ConcretePort *makeBusPort(std::string_view name,
                            int from_index,
                            int to_index);
  // Bundle port.
  ConcretePort *makeBundlePort(std::string_view name,
                               ConcretePortSeq *members);
  // Group previously defined bus bit ports together.
  void groupBusPorts(char bus_brkt_left,
                     char bus_brkt_right,
                     std::function<bool(std::string_view)> port_msb_first);
  size_t portCount() const;
  void setName(std::string_view name);
  virtual void addPort(ConcretePort *port);
  void addPortBit(ConcretePort *port);

protected:
  ConcreteCell(std::string_view name,
               std::string_view filename,
               bool is_leaf,
               ConcreteLibrary *library);
  ConcretePort *makeBusPort(std::string_view name,
                            int from_index,
                            int to_index,
                            ConcretePortSeq *members);
  void makeBusPortBits(ConcretePort *bus_port,
                       std::string_view bus_name,
                       int from_index,
                       int to_index);
  // Bus port bit (internal to makeBusPortBits).
  ConcretePort *makePort(std::string bit_name,
                         int bit_index);
  void makeBusPortBit(ConcretePort *bus_port,
                      std::string_view bus_name,
                      int index);

  std::string name_;
  ObjectId id_;
  // Filename is optional.
  std::string filename_;
  ConcreteLibrary *library_;
  LibertyCell *liberty_cell_;
  // External application cell.
  void *ext_cell_;
  // Non-bus and bus ports (but no expanded bus bit ports).
  ConcretePortSeq ports_;
  ConcretePortMap port_map_;
  // Port bit count (expanded buses).
  int port_bit_count_;
  bool is_leaf_;
  AttributeMap attribute_map_;

private:
  friend class ConcreteLibrary;
  friend class ConcreteCellPortBitIterator;
};

class ConcretePort
{
public:
  virtual ~ConcretePort();
  const std::string &name() const { return name_; }
  ObjectId id() const { return id_; }
  std::string busName() const;
  Cell *cell() const;
  ConcreteLibrary *library() const { return cell_->library(); }
  PortDirection *direction() const { return direction_; }
  LibertyPort *libertyPort() const { return liberty_port_; }
  void setLibertyPort(LibertyPort *port);
  // External application port.
  void *extPort() const { return ext_port_; }
  void setExtPort(void *port);
  void setDirection(PortDirection *dir);
  // Bundles are groups of related ports that do not use
  // bus notation.
  bool isBundle() const { return is_bundle_; }
  ConcretePort *bundlePort() const { return bundle_port_; }
  bool isBundleMember() const { return bundle_port_ != nullptr; }
  void setBundlePort(ConcretePort *port);
  bool isBus() const { return is_bus_; }
  // Index of cell bit ports.
  // Bus/bundle ports do not have an pin index.
  int pinIndex() const { return pin_index_; }
  void setPinIndex(int index);
  // Size is the bus/bundle member count (1 for non-bus/bundle ports).
  int size() const;
  int fromIndex() const { return from_index_; }
  int toIndex() const { return to_index_; }
  // Bus member, bus[subscript].
  ConcretePort *findBusBit(int index) const;
  // Predicate to determine if subscript is within bus range.
  //     (toIndex > fromIndex) && fromIndex <= subscript <= toIndex
  //  || (fromIndex > toIndex) && fromIndex >= subscript >= toIndex
  bool busIndexInRange(int index) const;
  // A port has members if it is a bundle or bus.
  bool hasMembers() const;
  ConcretePort *findMember(int index) const;
  ConcretePortMemberIterator *memberIterator() const;
  void setBusBitIndex(int index);

  // Bus bit port functions.
  // Bus bit is one bit of a bus port.
  bool isBusBit() const;
  // Bit index within bus port.
  // The bit index of A[3] is 3.
  int busBitIndex() const { return to_index_; }
  ConcretePortSeq *memberPorts() const { return member_ports_; }
  void addPortBit(ConcretePort *port);

protected:
  // Constructors for factory in cell class.
  ConcretePort(std::string_view name,
               bool is_bus,
               int from_index,
               int to_index,
               bool is_bundle,
               ConcretePortSeq *member_ports,
               ConcreteCell *cell);

  std::string name_;
  ObjectId id_;
  ConcreteCell *cell_;
  PortDirection *direction_;
  LibertyPort *liberty_port_;
  // External application port.
  void *ext_port_;
  int pin_index_;
  bool is_bundle_;
  bool is_bus_;
  int from_index_;
  int to_index_;
  // Expanded bus bit ports (ordered by from_index_ to to_index_)
  // or bundle member ports.
  ConcretePortSeq *member_ports_;
  ConcretePort *bundle_port_;

private:
  friend class ConcreteCell;
};

class ConcreteCellPortBitIterator : public Iterator<ConcretePort*>
{
public:
  ConcreteCellPortBitIterator(const ConcreteCell *cell);
  bool hasNext() override;
  ConcretePort *next() override;

private:
  void findNext();

  const ConcretePortSeq &ports_;
  ConcretePortSeq::const_iterator port_iter_;
  ConcretePortMemberIterator *member_iter_;
  ConcretePort *next_;
};

} // namespace sta
