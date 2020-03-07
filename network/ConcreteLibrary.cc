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

#include <stdlib.h>
#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "PatternMatch.hh"
#include "PortDirection.hh"
#include "ParseBus.hh"
#include "ConcreteLibrary.hh"

namespace sta {

static constexpr char escape_ = '\\';

ConcreteLibrary::ConcreteLibrary(const char *name,
				 const char *filename,
				 bool is_liberty) :
  name_(stringCopy(name)),
  filename_(stringCopy(filename)),
  is_liberty_(is_liberty),
  bus_brkt_left_('['),
  bus_brkt_right_(']')
{
}

ConcreteLibrary::~ConcreteLibrary()
{
  stringDelete(name_);
  stringDelete(filename_);
  cell_map_.deleteContents();
}

ConcreteCell *
ConcreteLibrary::makeCell(const char *name,
			  bool is_leaf,
			  const char *filename)
{
  ConcreteCell *cell = new ConcreteCell(this, name, is_leaf, filename);
  addCell(cell);
  return cell;
}

void
ConcreteLibrary::addCell(ConcreteCell *cell)
{
  cell_map_[cell->name()] = cell;
}

void
ConcreteLibrary::renameCell(ConcreteCell *cell,
			    const char *cell_name)
{
  cell_map_.erase(cell->name());
  cell_map_[cell_name] = cell;
}

void
ConcreteLibrary::deleteCell(ConcreteCell *cell)
{
  cell_map_.erase(cell->name());
  delete cell;
}

ConcreteLibraryCellIterator *
ConcreteLibrary::cellIterator() const
{
  return new ConcreteLibraryCellIterator(cell_map_);
}

ConcreteCell *
ConcreteLibrary::findCell(const char *name) const
{
  return cell_map_.findKey(name);
}

void
ConcreteLibrary::findCellsMatching(const PatternMatch *pattern,
				   CellSeq *cells) const
{
  ConcreteLibraryCellIterator cell_iter=ConcreteLibraryCellIterator(cell_map_);
  while (cell_iter.hasNext()) {
    ConcreteCell *cell = cell_iter.next();
    if (pattern->match(cell->name()))
      cells->push_back(reinterpret_cast<Cell*>(cell));
  }
}

void
ConcreteLibrary::setBusBrkts(char left,
			     char right)
{
  bus_brkt_left_ = left;
  bus_brkt_right_ = right;
}

////////////////////////////////////////////////////////////////

ConcreteCell::ConcreteCell(ConcreteLibrary *library,
			   const char *name,
			   bool is_leaf,
			   const char *filename):
  library_(library),
  name_(stringCopy(name)),
  filename_(stringCopy(filename)),
  liberty_cell_(nullptr),
  ext_cell_(nullptr),
  port_bit_count_(0),
  is_leaf_(is_leaf)
{
}

ConcreteCell::~ConcreteCell()
{
  stringDelete(name_);
  if (filename_)
    stringDelete(filename_);
  ports_.deleteContents();
}

void
ConcreteCell::setName(const char *name)
{
  const char *name_cpy = stringCopy(name);
  library_->renameCell(this, name_cpy);
  stringDelete(name_);
  name_ = name_cpy;
}

void
ConcreteCell::setLibertyCell(LibertyCell *cell)
{
  liberty_cell_ = cell;
}

void
ConcreteCell::setExtCell(void *ext_cell)
{
  ext_cell_ = ext_cell;
}

ConcretePort *
ConcreteCell::makePort(const char *name)
{
  ConcretePort *port = new ConcretePort(this, name, false, -1, -1, false, nullptr);
  addPort(port);
  return port;
}

ConcretePort *
ConcreteCell::makeBundlePort(const char *name,
			     ConcretePortSeq *members)
{
  ConcretePort *port = new ConcretePort(this, name, false, -1, -1,
					true, members);
  addPort(port);
  return port;
}

ConcretePort *
ConcreteCell::makeBusPort(const char *name,
			  int from_index,
			  int to_index)
{
  ConcretePort *port = new ConcretePort(this, name, true, from_index, to_index,
					false, new ConcretePortSeq);
  addPort(port);
  makeBusPortBits(port, name, from_index, to_index);
  return port;
}

ConcretePort *
ConcreteCell::makeBusPort(const char *name,
			  int from_index,
			  int to_index,
			  ConcretePortSeq *members)
{
  ConcretePort *port = new ConcretePort(this, name, true, from_index, to_index,
					false, members);
  addPort(port);
  return port;
}

void
ConcreteCell::makeBusPortBits(ConcretePort *bus_port,
			      const char *name,
			      int from_index,
			      int to_index)
{
  if (from_index < to_index) {
    for (int index = from_index; index <= to_index; index++)
      makeBusPortBit(bus_port, name, index);
  }
  else {
    for (int index = from_index; index >= to_index; index--)
      makeBusPortBit(bus_port, name, index);
  }
}

void
ConcreteCell::makeBusPortBit(ConcretePort *bus_port,
			     const char *bus_name,
			     int bit_index)
{
  string bit_name;
  stringPrint(bit_name, "%s%c%d%c",
	      bus_name,
	      library_->busBrktLeft(),
	      bit_index,
	      library_->busBrktRight());
  ConcretePort *port = makePort(bit_name.c_str(), bit_index);
  bus_port->addPortBit(port);
  addPortBit(port);
}

ConcretePort *
ConcreteCell::makePort(const char *bit_name,
		       int bit_index)
{
  ConcretePort *port = new ConcretePort(this, bit_name, false, bit_index,
					bit_index, false, nullptr);
  addPortBit(port);
  return port;
}

void
ConcreteCell::addPortBit(ConcretePort *port)
{
  port_map_[port->name()] = port;
  port->setPinIndex(port_bit_count_++);
}

void
ConcreteCell::addPort(ConcretePort *port)
{
  port_map_[port->name()] = port;
  ports_.push_back(port);
  if (!port->hasMembers())
    port->setPinIndex(port_bit_count_++);
}

void
ConcreteCell::setIsLeaf(bool is_leaf)
{
  is_leaf_ = is_leaf;
}

ConcretePort *
ConcreteCell::findPort(const char *name) const
{
  return port_map_.findKey(name);
}

size_t
ConcreteCell::portCount() const
{
  return ports_.size();
}

void
ConcreteCell::findPortsMatching(const PatternMatch *pattern,
				PortSeq *ports) const
{
  char bus_brkt_right = library_->busBrktRight();
  const char *pattern1 = pattern->pattern();
  bool bus_pattern = (pattern1[strlen(pattern1) - 1] == bus_brkt_right);
  ConcreteCellPortIterator *port_iter = portIterator();
  while (port_iter->hasNext()) {
    ConcretePort *port = port_iter->next();
    if (port->isBus() && bus_pattern) {
      ConcretePortMemberIterator *member_iter = port->memberIterator();
      while (member_iter->hasNext()) {
	ConcretePort *port_bit = member_iter->next();
	if (pattern->match(port_bit->name()))
	  ports->push_back(reinterpret_cast<Port*>(port_bit));
      }
      delete member_iter;
    }
    else if (pattern->match(port->name()))
      ports->push_back(reinterpret_cast<Port*>(port));
  }
  delete port_iter;
}

ConcreteCellPortIterator *
ConcreteCell::portIterator() const
{
  return new ConcreteCellPortIterator(ports_);
}

ConcreteCellPortBitIterator *
ConcreteCell::portBitIterator() const
{
  return new ConcreteCellPortBitIterator(this);
}

////////////////////////////////////////////////////////////////

class BusPort
{
public:
  BusPort(const char *name,
	  int from,
	  PortDirection *direction);
  ~BusPort();
  const char *name() const { return name_; }
  void pushMember(ConcretePort *port);
  void setFrom(int from);
  void setTo(int to);
  int from() const { return from_; }
  int to() const { return to_; }
  ConcretePortSeq *members() { return members_; }
  PortDirection *direction() { return direction_; }

private:
  DISALLOW_COPY_AND_ASSIGN(BusPort);

  const char *name_;
  int from_;
  int to_;
  PortDirection *direction_;
  ConcretePortSeq *members_;
};

BusPort::BusPort(const char *name,
		 int from,
		 PortDirection *direction) :
  name_(name),
  from_(from),
  to_(from),
  direction_(direction),
  members_(new ConcretePortSeq)
{
}

BusPort::~BusPort()
{
  // members_ ownership is transfered to bus port.
  stringDelete(name_);
}

void
BusPort::pushMember(ConcretePort *port)
{
  members_->push_back(port);
}

void
BusPort::setFrom(int from)
{
  from_ = from;
}

void
BusPort::setTo(int to)
{
  to_ = to;
}

typedef Map<const char*, BusPort*, CharPtrLess> BusPortMap;

void
ConcreteCell::groupBusPorts(const char bus_brkt_left,
			    const char bus_brkt_right)
{
  const char bus_brkts_left[2]{bus_brkt_left, '\0'};
  const char bus_brkts_right[2]{bus_brkt_right, '\0'};
  BusPortMap port_map;
  // Find ungrouped bus ports.
  // Remove bus bit ports from the ports_ vector during the scan by
  // keeping an index to the next insertion index and skipping over
  // the ones we want to remove.
  ConcretePortSeq ports = ports_;
  ports_.clear();
  for (ConcretePort *port : ports) {
    const char *port_name = port->name();
    char *bus_name;
    int index;
    parseBusName(port_name, bus_brkts_left, bus_brkts_right, escape_,
		 bus_name, index);
    if (bus_name) {
      if (!port->isBusBit()) {
	BusPort *bus_port = port_map.findKey(bus_name);
	if (bus_port) {
	  // Treat it as [max:min]/[from:to], ie downto.
	  bus_port->setFrom(std::max(index, bus_port->from()));
	  bus_port->setTo(std::min(index, bus_port->to()));
	  stringDelete(bus_name);
	}
	else {
	  bus_port = new BusPort(bus_name, index, port->direction());
	  port_map[bus_name] = bus_port;
	}
	bus_port->pushMember(port);
      }
      else
	ports_.push_back(port);
    }
    else
      ports_.push_back(port);
  }

  // Make the bus ports.
  BusPortMap::Iterator bus_iter(port_map);
  while (bus_iter.hasNext()) {
    BusPort *bus_port = bus_iter.next();
    const char *bus_name = bus_port->name();
    ConcretePortSeq *members = bus_port->members();
    sort(members, [&](ConcretePort *port1,
		      ConcretePort *port2) {
		    char *bus_name;
		    int index1, index2;
		    parseBusName(port1->name(), bus_brkts_left, bus_brkts_right, escape_,
				 bus_name, index1);
		    parseBusName(port2->name(), bus_brkts_left, bus_brkts_right, escape_,
				 bus_name, index2);
		    return index1 > index2;
		  });
    ConcretePort *port = makeBusPort(bus_name, bus_port->from(),
				     bus_port->to(), members);
    port->setDirection(bus_port->direction());
    delete bus_port;

    for (ConcretePort *port : *members) {
      char *bus_name;
      int index;
      parseBusName(port->name(), bus_brkts_left, bus_brkts_right, escape_,
		   bus_name, index);
      port->setBusBitIndex(index);
      stringDelete(bus_name);
    }
  }
}

////////////////////////////////////////////////////////////////

ConcretePort::ConcretePort(ConcreteCell *cell,
			   const char *name,
			   bool is_bus,
			   int from_index,
			   int to_index,
			   bool is_bundle,
			   ConcretePortSeq *member_ports) :
  name_(stringCopy(name)),
  cell_(cell),
  direction_(PortDirection::unknown()),
  liberty_port_(nullptr),
  ext_port_(nullptr),
  pin_index_(-1),
  is_bundle_(is_bundle),
  is_bus_(is_bus),
  from_index_(from_index),
  to_index_(to_index),
  member_ports_(member_ports)
{
}

ConcretePort::~ConcretePort()
{
  // The member ports of a bus are owned by the bus port.
  // The member ports of a bundle are NOT owned by the bus port.
  if (is_bus_)
    member_ports_->deleteContents();
  delete member_ports_;
  stringDelete(name_);
}

Cell *
ConcretePort::cell() const
{
  return reinterpret_cast<Cell*>(cell_);
}

void
ConcretePort::setLibertyPort(LibertyPort *port)
{
  liberty_port_ = port;
}

void
ConcretePort::setExtPort(void *port)
{
  ext_port_ = port;
}

const char *
ConcretePort::busName() const
{
  if (is_bus_) {
    ConcreteLibrary *lib = cell_->library();
    return stringPrintTmp("%s%c%d:%d%c",
			  name_,
			  lib->busBrktLeft(),
			  from_index_,
			  to_index_,
			  lib->busBrktRight());
  }
  else
    return name_;
}

ConcretePort *
ConcretePort::findMember(int index) const
{
  return (*member_ports_)[index];
}

bool
ConcretePort::isBusBit() const
{
  return from_index_ != -1
    && from_index_ == to_index_;
}

void
ConcretePort::setBusBitIndex(int index)
{
  from_index_ = to_index_ = index;
}

int
ConcretePort::size() const
{
  if (is_bus_)
    return abs(to_index_ - from_index_) + 1;
  else if (is_bundle_)
    return static_cast<int>(member_ports_->size());
  else
    return 1;
}

void
ConcretePort::setPinIndex(int index)
{
  pin_index_ = index;
}

void
ConcretePort::setDirection(PortDirection *dir)
{
  direction_ = dir;
  if (hasMembers()) {
    ConcretePortMemberIterator *member_iter = memberIterator();
    while (member_iter->hasNext()) {
      ConcretePort *port_bit = member_iter->next();
      port_bit->setDirection(dir);
    }
    delete member_iter;
  }
}

void
ConcretePort::addPortBit(ConcretePort *port)
{
  member_ports_->push_back(port);
}

ConcretePort *
ConcretePort::findBusBit(int index) const
{
  if (from_index_ < to_index_
      && index <= to_index_
      && index >= from_index_)
    return (*member_ports_)[index - from_index_];
  else if (from_index_ >= to_index_
	   && index >= to_index_
	   && index <= from_index_)
    return (*member_ports_)[from_index_ - index];
  else
    return nullptr;
}

bool
ConcretePort::busIndexInRange(int index) const
{
  return (from_index_ <= to_index_
	  && index <= to_index_
	  && index >= from_index_)
    || (from_index_ > to_index_
	&& index >= to_index_
	&& index <= from_index_);
}

bool
ConcretePort::hasMembers() const
{
  return is_bus_ || is_bundle_;
}

ConcretePortMemberIterator *
ConcretePort::memberIterator() const
{
  return new ConcretePortMemberIterator(member_ports_);
}

////////////////////////////////////////////////////////////////

ConcreteCellPortBitIterator::ConcreteCellPortBitIterator(const ConcreteCell*
							 cell) :
  port_iter_(cell->ports_),
  member_iter_(nullptr),
  next_(nullptr)
{
  findNext();
}

bool
ConcreteCellPortBitIterator::hasNext()
{
  return next_ != nullptr;
}

ConcretePort *
ConcreteCellPortBitIterator::next()
{
  ConcretePort *next = next_;
  findNext();
  return next;
}

void
ConcreteCellPortBitIterator::findNext()
{
  if (member_iter_) {
    if (member_iter_->hasNext()) {
      next_ = member_iter_->next();
      return;
    }
    else {
      delete member_iter_;
      member_iter_ = nullptr;
    }
  }
  while (port_iter_.hasNext()) {
    ConcretePort *next = port_iter_.next();
    if (next->isBus()) {
      member_iter_ = next->memberIterator();
      next_ = member_iter_->next();
      return;
    }
    else if (!next->isBundle()) {
      next_ = next;
      return;
    }
    next_ = nullptr;
  }
  next_ = nullptr;
}

} // namespace
