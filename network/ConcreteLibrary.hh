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

#ifndef STA_CONCRETE_LIBRARY_H
#define STA_CONCRETE_LIBRARY_H

#include "DisallowCopyAssign.hh"
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

typedef Map<const char*, ConcreteCell*, CharPtrLess> ConcreteCellMap;
typedef Vector<ConcretePort*> ConcretePortSeq;
typedef Map<const char*, ConcretePort*, CharPtrLess> ConcretePortMap;
typedef ConcreteCellMap::ConstIterator ConcreteLibraryCellIterator;
typedef ConcretePortSeq::ConstIterator ConcreteCellPortIterator;
typedef ConcretePortSeq::ConstIterator ConcretePortMemberIterator;

class ConcreteLibrary
{
public:
  explicit ConcreteLibrary(const char *name,
			   const char *filename);
  virtual ~ConcreteLibrary();
  virtual const char *name() const { return name_; }
  void setName(const char *name);
  virtual const char *filename() const { return filename_; }
  void addCell(ConcreteCell *cell);
  virtual ConcreteCell *makeCell(const char *name,
				 bool is_leaf,
				 const char *filename);
  void deleteCell(ConcreteCell *cell);
  ConcreteLibraryCellIterator *cellIterator() const;
  virtual ConcreteCell *findCell(const char *name) const;
  virtual void findCellsMatching(const PatternMatch *pattern,
				 CellSeq *cells) const;
  virtual char busBrktLeft() { return bus_brkt_left_; }
  virtual char busBrktRight() { return bus_brkt_right_; }
  void setBusBrkts(char left,
		   char right);

protected:
  void renameCell(ConcreteCell *cell,
		  const char *cell_name);

  const char *name_;
  const char *filename_;
  char bus_brkt_left_;
  char bus_brkt_right_;
  ConcreteCellMap cell_map_;

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteLibrary);

  friend class ConcreteCell;
};

class ConcreteCell
{
public:
  // Use ConcreteLibrary::deleteCell.
  virtual ~ConcreteCell();
  virtual ConcreteLibrary *library() const { return library_; }
  virtual const char *name() const { return name_; }
  virtual const char *filename() const { return filename_; }
  virtual int portBitCount() const { return port_bit_count_; }
  virtual ConcretePort *findPort(const char *name) const;
  virtual void findPortsMatching(const PatternMatch *pattern,
				 PortSeq *ports) const;
  virtual ConcreteCellPortIterator *portIterator() const;
  virtual ConcreteCellPortBitIterator *portBitIterator() const;
  virtual bool isLeaf() const { return is_leaf_; }
  void setIsLeaf(bool is_leaf);

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
  void groupBusPorts(const char *bus_brkts_left,
		     const char *bus_brkts_right);
  size_t portCount() const;
  void setName(const char *name);
  virtual void addPort(ConcretePort *port);
  void addPortBit(ConcretePort *port);

protected:
  ConcreteCell(ConcreteLibrary *library,
	       const char *name,
	       bool is_leaf,
	       const char *filename);
  ConcretePort *makeBusPort(const char *name,
			    int from_index,
			    int to_index,
			    ConcretePortSeq *members);
  void makeBusPortBits(ConcretePort *bus_port,
		       const char *name,
		       int from_index,
		       int to_index);
  // Bus port bit (internal to makeBusPortBits).
  virtual ConcretePort *makePort(const char *bit_name,
				 int bit_index);
  void makeBusPortBit(ConcretePort *bus_port,
		      const char *name,
		      int index);

  ConcreteLibrary *library_;
  const char *name_;
  // Filename is optional.
  const char *filename_;
  // Non-bus and bus ports (but no expanded bus bit ports).
  ConcretePortSeq ports_;
  ConcretePortMap port_map_;
  // Port bit count (expanded buses).
  int port_bit_count_;
  bool is_leaf_;

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteCell);

  friend class ConcreteLibrary;
  friend class ConcreteCellPortBitIterator;
};

class ConcretePort
{
public:
  virtual ~ConcretePort();
  virtual const char *name() const { return name_; }
  virtual const char *busName() const;
  virtual Cell *cell() const;
  virtual ConcreteLibrary *library() const { return cell_->library(); }
  virtual PortDirection *direction() const { return direction_; }
  virtual void setDirection(PortDirection *dir);
  // Bundles are groups of related ports that do not use
  // bus notation.
  virtual bool isBundle() const { return is_bundle_; }
  virtual bool isBus() const { return is_bus_; }
  // Index of cell bit ports.
  // Bus/bundle ports do not have an pin index.
  virtual int pinIndex() const { return pin_index_; }
  void setPinIndex(int index);
  // Size is the bus/bundle member count (1 for non-bus/bundle ports).
  virtual int size() const;
  virtual int fromIndex() const { return from_index_; }
  virtual int toIndex() const { return to_index_; }
  // Bus member, bus[subscript].
  virtual ConcretePort *findBusBit(int index) const;
  // Predicate to determine if subscript is within bus range.
  //     (toIndex > fromIndex) && fromIndex <= subscript <= toIndex
  //  || (fromIndex > toIndex) && fromIndex >= subscript >= toIndex
  bool busIndexInRange(int index) const;
  // A port has members if it is a bundle or bus.
  bool hasMembers() const;
  virtual ConcretePort *findMember(int index) const;
  virtual ConcretePortMemberIterator *memberIterator() const;
  void setBusBitIndex(int index);

  // Bus bit port functions.
  // Bus bit is one bit of a bus port.
  virtual bool isBusBit() const;
  // Bit index within bus port.
  // The bit index of A[3] is 3.
  virtual int busBitIndex() const { return to_index_; }
  ConcretePortSeq *memberPorts() const { return member_ports_; }
  void addPortBit(ConcretePort *port);

protected:
  // Constructors for factory in cell class.
  ConcretePort(ConcreteCell *cell,
	       const char *name,
	       bool is_bus,
	       int from_index,
	       int to_index,
	       bool is_bundle,
	       ConcretePortSeq *member_ports);

  const char *name_;
  ConcreteCell *cell_;
  PortDirection *direction_;
  int pin_index_;
  bool is_bundle_;
  bool is_bus_;
  int from_index_;
  int to_index_;
  // Expanded bus bit ports (ordered by from_index_ to to_index_)
  // or bundle member ports.
  ConcretePortSeq *member_ports_;

private:
  DISALLOW_COPY_AND_ASSIGN(ConcretePort);

  friend class ConcreteCell;
};

class ConcreteCellPortBitIterator : public Iterator<ConcretePort*>
{
public:
  explicit ConcreteCellPortBitIterator(const ConcreteCell *cell);
  virtual bool hasNext();
  virtual ConcretePort *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteCellPortBitIterator);
  void findNext();

  ConcretePortSeq::ConstIterator port_iter_;
  ConcretePortMemberIterator *member_iter_;
  ConcretePort *next_;
};

} // Namespace
#endif
