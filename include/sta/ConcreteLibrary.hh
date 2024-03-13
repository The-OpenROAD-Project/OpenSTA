// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#pragma once

#include <functional>

#include "Vector.hh"
#include "Map.hh"
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

typedef Map<const char*, ConcreteCell*, CharPtrLess> ConcreteCellMap;
typedef Map<string, string> AttributeMap;
typedef Vector<ConcretePort*> ConcretePortSeq;
typedef Map<const char*, ConcretePort*, CharPtrLess> ConcretePortMap;
typedef ConcreteCellMap::ConstIterator ConcreteLibraryCellIterator;
typedef ConcretePortSeq::ConstIterator ConcreteCellPortIterator;
typedef ConcretePortSeq::ConstIterator ConcretePortMemberIterator;

class ConcreteLibrary
{
public:
  explicit ConcreteLibrary(const char *name,
			   const char *filename,
			   bool is_liberty);
  virtual ~ConcreteLibrary();
  const char *name() const { return name_; }
  void setName(const char *name);
  ObjectId id() const { return id_; }
  bool isLiberty() const { return is_liberty_; }
  const char *filename() const { return filename_; }
  void addCell(ConcreteCell *cell);
  ConcreteCell *makeCell(const char *name,
			 bool is_leaf,
			 const char *filename);
  void deleteCell(ConcreteCell *cell);
  ConcreteLibraryCellIterator *cellIterator() const;
  ConcreteCell *findCell(const char *name) const;
  CellSeq findCellsMatching(const PatternMatch *pattern) const;
  char busBrktLeft() const { return bus_brkt_left_; }
  char busBrktRight() const { return bus_brkt_right_; }
  void setBusBrkts(char left,
		   char right);

protected:
  void renameCell(ConcreteCell *cell,
		  const char *cell_name);

  const char *name_;
  ObjectId id_;
  const char *filename_;
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
  const char *name() const { return name_; }
  ObjectId id() const { return id_; }
  const char *filename() const { return filename_; }
  ConcreteLibrary *library() const { return library_; }
  LibertyCell *libertyCell() const { return liberty_cell_; }
  void setLibertyCell(LibertyCell *cell);
  void *extCell() const { return ext_cell_; }
  void setExtCell(void *ext_cell);
  int portBitCount() const { return port_bit_count_; }
  ConcretePort *findPort(const char *name) const;
  PortSeq findPortsMatching(const PatternMatch *pattern) const;
  ConcreteCellPortIterator *portIterator() const;
  ConcreteCellPortBitIterator *portBitIterator() const;
  bool isLeaf() const { return is_leaf_; }
  void setIsLeaf(bool is_leaf);
  void setAttribute(const string &key,
                    const string &value);
  string getAttribute(const string &key) const;

  // Cell acts as port factory.
  ConcretePort *makePort(const char *name);
  // Bus port.
  ConcretePort *makeBusPort(const char *name,
			    int from_index,
			    int to_index);
  // Bundle port.
  ConcretePort *makeBundlePort(const char *name,
			       ConcretePortSeq *members);
  // Group previously defined bus bit ports together.
  void groupBusPorts(const char bus_brkt_left,
		     const char bus_brkt_right,
                     std::function<bool(const char*)> port_msb_first);
  size_t portCount() const;
  void setName(const char *name);
  void addPort(ConcretePort *port);
  void addPortBit(ConcretePort *port);

protected:
  ConcreteCell(const char *name,
	       const char *filename,
               bool is_leaf,
               ConcreteLibrary *library);
  ConcretePort *makeBusPort(const char *name,
			    int from_index,
			    int to_index,
			    ConcretePortSeq *members);
  void makeBusPortBits(ConcretePort *bus_port,
		       const char *name,
		       int from_index,
		       int to_index);
  // Bus port bit (internal to makeBusPortBits).
  ConcretePort *makePort(const char *bit_name,
			 int bit_index);
  void makeBusPortBit(ConcretePort *bus_port,
		      const char *name,
		      int index);

  const char *name_;
  ObjectId id_;
  // Filename is optional.
  const char *filename_;
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
  const char *name() const { return name_; }
  ObjectId id() const { return id_; }
  const char *busName() const;
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
  ConcretePort(const char *name,
               bool is_bus,
	       int from_index,
	       int to_index,
	       bool is_bundle,
	       ConcretePortSeq *member_ports,
               ConcreteCell *cell);

  const char *name_;
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

private:
  friend class ConcreteCell;
};

class ConcreteCellPortBitIterator : public Iterator<ConcretePort*>
{
public:
  explicit ConcreteCellPortBitIterator(const ConcreteCell *cell);
  virtual bool hasNext();
  virtual ConcretePort *next();

private:
  void findNext();

  ConcretePortSeq::ConstIterator port_iter_;
  ConcretePortMemberIterator *member_iter_;
  ConcretePort *next_;
};

} // Namespace
