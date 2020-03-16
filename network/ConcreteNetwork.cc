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

#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "PatternMatch.hh"
#include "Report.hh"
#include "PortDirection.hh"
#include "ConcreteLibrary.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "ConcreteNetwork.hh"

namespace sta {

static void
makeChildNetwork(Instance *proto,
		 Instance *parent,
		 ConcreteBindingTbl *parent_bindings,
		 NetworkReader *network);
static void
makeClonePins(Instance *proto,
	      Instance *clone,
	      Instance *clone_view,
	      ConcreteBindingTbl *bindings,
	      Instance *parent,
	      ConcreteBindingTbl *parent_bindings,
	      NetworkReader *network);

NetworkReader *
makeConcreteNetwork()
{
  return new ConcreteNetwork;
}

class ConcreteInstanceChildIterator : public InstanceChildIterator
{
public:
  explicit ConcreteInstanceChildIterator(ConcreteInstanceChildMap *map);
  bool hasNext();
  Instance *next();

private:
  ConcreteInstanceChildMap::ConstIterator iter_;
};

ConcreteInstanceChildIterator::
ConcreteInstanceChildIterator(ConcreteInstanceChildMap *map) :
  iter_(map)
{
}

bool
ConcreteInstanceChildIterator::hasNext()
{
  return iter_.hasNext();
}

Instance *
ConcreteInstanceChildIterator::next()
{
  return reinterpret_cast<Instance*>(iter_.next());
}

class ConcreteInstanceNetIterator : public InstanceNetIterator
{
public:
  explicit ConcreteInstanceNetIterator(ConcreteInstanceNetMap *nets);
  bool hasNext();
  Net *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteInstanceNetIterator);
  void findNext();

  ConcreteInstanceNetMap::Iterator iter_;
  ConcreteNet *next_;
};

ConcreteInstanceNetIterator::
ConcreteInstanceNetIterator(ConcreteInstanceNetMap *nets):
  iter_(nets),
  next_(nullptr)
{
  findNext();
}

bool
ConcreteInstanceNetIterator::hasNext()
{
  return next_ != nullptr;
}

// Skip nets that have been merged.
void
ConcreteInstanceNetIterator::findNext()
{
  while (iter_.hasNext()) {
    next_ = iter_.next();
    if (next_->mergedInto() == nullptr)
      return;
  }
  next_ = nullptr;
}

Net *
ConcreteInstanceNetIterator::next()
{
  ConcreteNet *next = next_;
  findNext();
  return reinterpret_cast<Net*>(next);
}

////////////////////////////////////////////////////////////////

class ConcreteInstancePinIterator : public InstancePinIterator
{
public:
  ConcreteInstancePinIterator(const ConcreteInstance *inst,
			      int pin_count);
  bool hasNext();
  Pin *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteInstancePinIterator);
  void findNext();

  ConcretePin **pins_;
  int pin_count_;
  int pin_index_;
  ConcretePin *next_;
};

ConcreteInstancePinIterator::
ConcreteInstancePinIterator(const ConcreteInstance *inst,
			    int pin_count) :
  pins_(inst->pins_),
  pin_count_(pin_count),
  pin_index_(0)
{
  findNext();
}

bool
ConcreteInstancePinIterator::hasNext()
{
  return next_ != nullptr;
}

Pin *
ConcreteInstancePinIterator::next()
{
  ConcretePin *next = next_;
  findNext();
  return reinterpret_cast<Pin*>(next);
}

// Skip over missing pins.
void
ConcreteInstancePinIterator::findNext()
{
  while (pin_index_ < pin_count_) {
    next_ = pins_[pin_index_++];
    if (next_)
      return;
  }
  next_ = nullptr;
}

////////////////////////////////////////////////////////////////

class ConcreteNetPinIterator : public NetPinIterator
{
public:
  explicit ConcreteNetPinIterator(const ConcreteNet *net);
  bool hasNext();
  Pin *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteNetPinIterator);

  ConcretePin *next_;
};

ConcreteNetPinIterator::ConcreteNetPinIterator(const ConcreteNet *net) :
  next_(net->pins_)
{
}

bool
ConcreteNetPinIterator::hasNext()
{
  return next_ != nullptr;
}

Pin *
ConcreteNetPinIterator::next()
{
  ConcretePin *next = next_;
  next_ = next_->net_next_;
  return reinterpret_cast<Pin*>(next);
}

////////////////////////////////////////////////////////////////

class ConcreteNetTermIterator : public NetTermIterator
{
public:
  explicit ConcreteNetTermIterator(const ConcreteNet *net);
  bool hasNext();
  Term *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteNetTermIterator);

  ConcreteTerm *next_;
};

ConcreteNetTermIterator::ConcreteNetTermIterator(const ConcreteNet *net) :
  next_(net->terms_)
{
}

bool
ConcreteNetTermIterator::hasNext()
{
  return next_ != nullptr;
}

Term *
ConcreteNetTermIterator::next()
{
  ConcreteTerm *next = next_;
  next_ = next_->net_next_;
  return reinterpret_cast<Term*>(next);
}

////////////////////////////////////////////////////////////////

ConcreteNetwork::ConcreteNetwork() :
  NetworkReader(),
  top_instance_(nullptr),
  link_func_(nullptr)
{
}

ConcreteNetwork::~ConcreteNetwork()
{
  clear();
}

void
ConcreteNetwork::clear()
{
  deleteTopInstance();
  deleteCellNetworkViews();
  library_seq_.deleteContentsClear();
  library_map_.clear();
  Network::clear();
}

void
ConcreteNetwork::deleteTopInstance()
{
  if (top_instance_) {
    deleteInstance(top_instance_);
    top_instance_ = nullptr;
  }
}

void
ConcreteNetwork::deleteCellNetworkViews()
{
  CellNetworkViewMap::Iterator view_iter(cell_network_view_map_);
  while (view_iter.hasNext()) {
    Instance *view = view_iter.next();
    if (view)
      deleteInstance(view);
  }
  cell_network_view_map_.clear();
}

Instance *
ConcreteNetwork::topInstance() const
{
  return top_instance_;
}

////////////////////////////////////////////////////////////////

class ConcreteLibraryIterator1 : public Iterator<Library*>
{
public:
  explicit ConcreteLibraryIterator1(const ConcreteLibrarySeq &lib_seq_);
  virtual bool hasNext();
  virtual Library *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteLibraryIterator1);

  ConcreteLibraryIterator iter_;
};

ConcreteLibraryIterator1::ConcreteLibraryIterator1(const ConcreteLibrarySeq &lib_seq_):
  iter_(lib_seq_)
{
}

bool
ConcreteLibraryIterator1::hasNext()
{
  return iter_.hasNext();
}

Library *
ConcreteLibraryIterator1::next()
{
  return reinterpret_cast<Library*>(iter_.next());
}

LibraryIterator *
ConcreteNetwork::libraryIterator() const
{
  return new ConcreteLibraryIterator1(library_seq_);
}

////////////////////////////////////////////////////////////////

class ConcreteLibertyLibraryIterator : public Iterator<LibertyLibrary*>
{
public:
  explicit ConcreteLibertyLibraryIterator(const ConcreteNetwork *network);
  virtual ~ConcreteLibertyLibraryIterator();
  virtual bool hasNext();
  virtual LibertyLibrary *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteLibertyLibraryIterator);
  void findNext();

  ConcreteLibrarySeq::ConstIterator iter_;
  LibertyLibrary *next_;
};

ConcreteLibertyLibraryIterator::
ConcreteLibertyLibraryIterator(const ConcreteNetwork *network):
  iter_(network->library_seq_),
  next_(nullptr)
{
  findNext();
}

ConcreteLibertyLibraryIterator::~ConcreteLibertyLibraryIterator()
{
}

bool
ConcreteLibertyLibraryIterator::hasNext()
{
  return next_ != nullptr;
}

LibertyLibrary *
ConcreteLibertyLibraryIterator::next()
{
  LibertyLibrary *next = next_;
  findNext();
  return next;
}

void
ConcreteLibertyLibraryIterator::findNext()
{
  next_ = nullptr;
  while (iter_.hasNext()) {
    ConcreteLibrary *lib = iter_.next();
    if (lib->isLiberty()) {
      LibertyLibrary *liberty = static_cast<LibertyLibrary*>(lib);
      if (liberty) {
	next_ = liberty;
	break;
      }
    }
  }
}

LibertyLibraryIterator *
ConcreteNetwork::libertyLibraryIterator() const
{
  return new ConcreteLibertyLibraryIterator(this);
}

////////////////////////////////////////////////////////////////

Library *
ConcreteNetwork::makeLibrary(const char *name,
			     const char *filename)
{
  ConcreteLibrary *library = new ConcreteLibrary(name, filename, false);
  addLibrary(library);
  return reinterpret_cast<Library*>(library);
}

LibertyLibrary *
ConcreteNetwork::makeLibertyLibrary(const char *name,
				    const char *filename)
{
  LibertyLibrary *library = new LibertyLibrary(name, filename);
  addLibrary(library);
  return library;
}

void
ConcreteNetwork::addLibrary(ConcreteLibrary *library)
{
  library_seq_.push_back(library);
  library_map_[library->name()] = library;
}

Library *
ConcreteNetwork::findLibrary(const char *name)
{
  return reinterpret_cast<Library*>(library_map_.findKey(name));
}

void
ConcreteNetwork::deleteLibrary(ConcreteLibrary *library)
{
  library_map_.erase(library->name());
  library_seq_.eraseObject(library);
  delete library;
}

const char *
ConcreteNetwork::name(const Library *library) const
{
  const ConcreteLibrary *clib =
    reinterpret_cast<const ConcreteLibrary*>(library);
  return clib->name();
}

LibertyLibrary *
ConcreteNetwork::findLiberty(const char *name)
{
  ConcreteLibrary *lib =  library_map_.findKey(name);
  if (lib) {
    if (lib->isLiberty())
      return static_cast<LibertyLibrary*>(lib);
    // Potential name conflict
    else {
      for (ConcreteLibrary *lib : library_seq_) {
	if (stringEq(lib->name(), name)
	    && lib->isLiberty())
	  return static_cast<LibertyLibrary*>(lib);
      }
    }
  }
  return nullptr;
}

LibertyLibrary *
ConcreteNetwork::libertyLibrary(Library *library) const
{
  ConcreteLibrary *lib = reinterpret_cast<ConcreteLibrary*>(library);
  return static_cast<LibertyLibrary*>(lib);
}

Cell *
ConcreteNetwork::makeCell(Library *library,
			  const char *name,
			  bool is_leaf,
			  const char *filename)
{
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(library);
  return reinterpret_cast<Cell*>(clib->makeCell(name, is_leaf, filename));
}

Cell *
ConcreteNetwork::findCell(const Library *library,
			  const char *name) const
{
  const ConcreteLibrary *clib =
    reinterpret_cast<const ConcreteLibrary*>(library);
  return reinterpret_cast<Cell*>(clib->findCell(name));
}

Cell *
ConcreteNetwork::findAnyCell(const char *name)
{
  ConcreteLibrarySeq::Iterator lib_iter(library_seq_);
  while (lib_iter.hasNext()) {
    ConcreteLibrary *lib = lib_iter.next();
    ConcreteCell *cell = lib->findCell(name);
    if (cell)
      return reinterpret_cast<Cell*>(cell);
  }
  return nullptr;
}

void
ConcreteNetwork::findCellsMatching(const Library *library,
				   const PatternMatch *pattern,
				   CellSeq *cells) const
{
  const ConcreteLibrary *clib =
    reinterpret_cast<const ConcreteLibrary*>(library);
  clib->findCellsMatching(pattern, cells);
}

void
ConcreteNetwork::deleteCell(Cell *cell)
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  ConcreteLibrary *clib = ccell->library();
  clib->deleteCell(ccell);
}

////////////////////////////////////////////////////////////////

const char *
ConcreteNetwork::name(const Cell *cell) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return ccell->name();
}

void
ConcreteNetwork::setName(Cell *cell,
			 const char *name)
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  ccell->setName(name);
}

void
ConcreteNetwork::setIsLeaf(Cell *cell,
			   bool is_leaf)
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  ccell->setIsLeaf(is_leaf);
}

Library *
ConcreteNetwork::library(const Cell *cell) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return reinterpret_cast<Library*>(ccell->library());
}

LibertyCell *
ConcreteNetwork::libertyCell(Cell *cell) const
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  return ccell->libertyCell();
}

const LibertyCell *
ConcreteNetwork::libertyCell(const Cell *cell) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return ccell->libertyCell();
}

Cell *
ConcreteNetwork::cell(LibertyCell *cell) const
{
  return reinterpret_cast<Cell*>(cell);
}

const Cell *
ConcreteNetwork::cell(const LibertyCell *cell) const
{
  return reinterpret_cast<const Cell*>(cell);
}

const char *
ConcreteNetwork::filename(const Cell *cell)
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return ccell->filename();
}

Port *
ConcreteNetwork::findPort(const Cell *cell,
			  const char *name) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return reinterpret_cast<Port*>(ccell->findPort(name));
}

void
ConcreteNetwork::findPortsMatching(const Cell *cell,
				   const PatternMatch *pattern,
				   PortSeq *ports) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  ccell->findPortsMatching(pattern, ports);
}

bool
ConcreteNetwork::isLeaf(const Cell *cell) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return ccell->isLeaf();
}

Port *
ConcreteNetwork::makePort(Cell *cell,
			  const char *name)
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  ConcretePort *port = ccell->makePort(name);
  return reinterpret_cast<Port*>(port);
}

Port *
ConcreteNetwork::makeBusPort(Cell *cell,
			     const char *name,
			     int from_index,
			     int to_index)
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  ConcretePort *port = ccell->makeBusPort(name, from_index, to_index);
  return reinterpret_cast<Port*>(port);
}

void
ConcreteNetwork::groupBusPorts(Cell *cell)
{
  Library *lib = library(cell);
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  ccell->groupBusPorts(clib->busBrktLeft(), clib->busBrktRight());
}

Port *
ConcreteNetwork::makeBundlePort(Cell *cell,
				const char *name,
				PortSeq *members)
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  ConcretePortSeq *cmembers = reinterpret_cast<ConcretePortSeq*>(members);
  ConcretePort *port = ccell->makeBundlePort(name, cmembers);
  return reinterpret_cast<Port*>(port);
}

void
ConcreteNetwork::setDirection(Port *port,
			      PortDirection *dir)
{
  ConcretePort *cport = reinterpret_cast<ConcretePort*>(port);
  cport->setDirection(dir);
}

////////////////////////////////////////////////////////////////

class ConcreteCellPortIterator1 : public CellPortIterator
{
public:
  explicit ConcreteCellPortIterator1(const ConcreteCell *cell);
  ~ConcreteCellPortIterator1();
  virtual bool hasNext() { return iter_->hasNext(); }
  virtual Port *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteCellPortIterator1);

  ConcreteCellPortIterator *iter_;
};

ConcreteCellPortIterator1::ConcreteCellPortIterator1(const ConcreteCell *cell):
  iter_(cell->portIterator())
{
}

ConcreteCellPortIterator1::~ConcreteCellPortIterator1()
{
  delete iter_;
}

Port *
ConcreteCellPortIterator1::next()
{
  return reinterpret_cast<Port*>(iter_->next());
}

CellPortIterator *
ConcreteNetwork::portIterator(const Cell *cell) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return new ConcreteCellPortIterator1(ccell);
}

////////////////////////////////////////////////////////////////

class ConcreteCellPortBitIterator1 : public CellPortIterator
{
public:
  explicit ConcreteCellPortBitIterator1(const ConcreteCell *cell);
  ~ConcreteCellPortBitIterator1();
  virtual bool hasNext() { return iter_->hasNext(); }
  virtual Port *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteCellPortBitIterator1);

  ConcreteCellPortBitIterator *iter_;
};

ConcreteCellPortBitIterator1::ConcreteCellPortBitIterator1(const ConcreteCell *cell):
  iter_(cell->portBitIterator())
{
}

ConcreteCellPortBitIterator1::~ConcreteCellPortBitIterator1()
{
  delete iter_;
}

Port *
ConcreteCellPortBitIterator1::next()
{
  return reinterpret_cast<Port*>(iter_->next());
}

CellPortBitIterator *
ConcreteNetwork::portBitIterator(const Cell *cell) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return new ConcreteCellPortBitIterator1(ccell);
}

int
ConcreteNetwork::portBitCount(const Cell *cell) const
{
  const ConcreteCell *ccell = reinterpret_cast<const ConcreteCell*>(cell);
  return ccell->portBitCount();
}

////////////////////////////////////////////////////////////////

const char *
ConcreteNetwork::name(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->name();
}

Cell *
ConcreteNetwork::cell(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->cell();
}

LibertyPort *
ConcreteNetwork::libertyPort(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->libertyPort();
}

PortDirection *
ConcreteNetwork::direction(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->direction();
}

bool
ConcreteNetwork::isBundle(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->isBundle();
}

bool
ConcreteNetwork::isBus(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->isBus();
}

const char *
ConcreteNetwork::busName(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->busName();
}

int
ConcreteNetwork::size(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->size();
}

int
ConcreteNetwork::fromIndex(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->fromIndex();
}

int
ConcreteNetwork::toIndex(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->toIndex();
}

Port *
ConcreteNetwork::findBusBit(const Port *port,
			    int index) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return reinterpret_cast<Port*>(cport->findBusBit(index));
}

Port *
ConcreteNetwork::findMember(const Port *port,
			    int index) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return reinterpret_cast<Port*>(cport->findMember(index));
}

bool
ConcreteNetwork::hasMembers(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return cport->hasMembers();
}

////////////////////////////////////////////////////////////////

class ConcretePortMemberIterator1 : public PortMemberIterator
{
public:
  explicit ConcretePortMemberIterator1(const ConcretePort *port);
  ~ConcretePortMemberIterator1();
  virtual bool hasNext() { return iter_->hasNext(); }
  virtual Port *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ConcretePortMemberIterator1);

  ConcretePortMemberIterator *iter_;
};

ConcretePortMemberIterator1::ConcretePortMemberIterator1(const ConcretePort *
							 port) :
  iter_(port->memberIterator())
{
}

ConcretePortMemberIterator1::~ConcretePortMemberIterator1()
{
  delete iter_;
}

Port *
ConcretePortMemberIterator1::next()
{
  return reinterpret_cast<Port*>(iter_->next());
}

PortMemberIterator *
ConcreteNetwork::memberIterator(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return new ConcretePortMemberIterator1(cport);
}

////////////////////////////////////////////////////////////////

const char *
ConcreteNetwork::name(const Instance *instance) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  return inst->name();
}

Cell *
ConcreteNetwork::cell(const Instance *instance) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  return inst->cell();
}

Instance *
ConcreteNetwork::parent(const Instance *instance) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  return reinterpret_cast<Instance*>(inst->parent());
}

bool
ConcreteNetwork::isLeaf(const Instance *instance) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(inst->cell());
  return ccell->isLeaf();
}

Instance *
ConcreteNetwork::findChild(const Instance *parent,
			   const char *name) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(parent);
  return inst->findChild(name);
}

Pin *
ConcreteNetwork::findPin(const Instance *instance,
			 const char *port_name) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  return reinterpret_cast<Pin*>(inst->findPin(port_name));
}

Pin *
ConcreteNetwork::findPin(const Instance *instance,
			 const Port *port) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  return reinterpret_cast<Pin*>(inst->findPin(port));
}

Net *
ConcreteNetwork::findNet(const Instance *instance,
			 const char *net_name) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  return reinterpret_cast<Net*>(inst->findNet(net_name));
}

void
ConcreteNetwork::findInstNetsMatching(const Instance *instance,
				      const PatternMatch *pattern,
				      NetSeq *nets) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  inst->findNetsMatching(pattern, nets);
}

////////////////////////////////////////////////////////////////

InstanceChildIterator *
ConcreteNetwork::childIterator(const Instance *instance) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  return inst->childIterator();
}

InstancePinIterator *
ConcreteNetwork::pinIterator(const Instance *instance) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  ConcreteCell *cell = reinterpret_cast<ConcreteCell*>(inst->cell());
  int pin_count = cell->portBitCount();
  return new ConcreteInstancePinIterator(inst, pin_count);
}

InstanceNetIterator *
ConcreteNetwork::netIterator(const Instance *instance) const
{
  const ConcreteInstance *inst =
    reinterpret_cast<const ConcreteInstance*>(instance);
  return inst->netIterator();
}

////////////////////////////////////////////////////////////////

Instance *
ConcreteNetwork::instance(const Pin *pin) const
{
  const ConcretePin *cpin = reinterpret_cast<const ConcretePin*>(pin);
  return reinterpret_cast<Instance*>(cpin->instance());
}

Net *
ConcreteNetwork::net(const Pin *pin) const
{
  const ConcretePin *cpin = reinterpret_cast<const ConcretePin*>(pin);
  return reinterpret_cast<Net*>(cpin->net());
}

Term *
ConcreteNetwork::term(const Pin *pin) const
{
  const ConcretePin *cpin = reinterpret_cast<const ConcretePin*>(pin);
  return reinterpret_cast<Term*>(cpin->term());
}

Port *
ConcreteNetwork::port(const Pin *pin) const
{
  const ConcretePin *cpin = reinterpret_cast<const ConcretePin*>(pin);
  return reinterpret_cast<Port*>(cpin->port());
}

PortDirection *
ConcreteNetwork::direction(const Pin *pin) const
{
  const ConcretePin *cpin = reinterpret_cast<const ConcretePin*>(pin);
  const ConcretePort *cport = cpin->port();
  return cport->direction();
}

VertexId
ConcreteNetwork::vertexId(const Pin *pin) const
{
  const ConcretePin *cpin = reinterpret_cast<const ConcretePin*>(pin);
  return cpin->vertexId();
}

void
ConcreteNetwork::setVertexId(Pin *pin,
			     VertexId id)
{
  ConcretePin *cpin = reinterpret_cast<ConcretePin*>(pin);
  cpin->setVertexId(id);
}

////////////////////////////////////////////////////////////////

Net *
ConcreteNetwork::net(const Term *term) const
{
  const ConcreteTerm *cterm = reinterpret_cast<const ConcreteTerm*>(term);
  return reinterpret_cast<Net*>(cterm->net());
}

Pin *
ConcreteNetwork::pin(const Term *term) const
{
  const ConcreteTerm *cterm = reinterpret_cast<const ConcreteTerm*>(term);
  return reinterpret_cast<Pin*>(cterm->pin());
}

////////////////////////////////////////////////////////////////

const char *
ConcreteNetwork::name(const Net *net) const
{
  const ConcreteNet *cnet = reinterpret_cast<const ConcreteNet*>(net);
  return cnet->name();
}

Instance *
ConcreteNetwork::instance(const Net *net) const
{
  const ConcreteNet *cnet = reinterpret_cast<const ConcreteNet*>(net);
  return reinterpret_cast<Instance*>(cnet->instance());
}

bool
ConcreteNetwork::isPower(const Net *net) const
{
  return constant_nets_[int(LogicValue::one)].hasKey(const_cast<Net*>(net));
}

bool
ConcreteNetwork::isGround(const Net *net) const
{
  return constant_nets_[int(LogicValue::zero)].hasKey(const_cast<Net*>(net));
}

NetPinIterator *
ConcreteNetwork::pinIterator(const Net *net) const
{
  const ConcreteNet *cnet = reinterpret_cast<const ConcreteNet*>(net);
  return new ConcreteNetPinIterator(cnet);
}

NetTermIterator *
ConcreteNetwork::termIterator(const Net *net) const
{
  const ConcreteNet *cnet = reinterpret_cast<const ConcreteNet*>(net);
  return new ConcreteNetTermIterator(cnet);
}

void
ConcreteNetwork::mergeInto(Net *net,
			   Net *into_net)
{
  ConcreteNet *cnet = reinterpret_cast<ConcreteNet*>(net);
  ConcreteNet *cinto_net = reinterpret_cast<ConcreteNet*>(into_net);
  cnet->mergeInto(cinto_net);
  clearNetDrvrPinMap();
}

Net *
ConcreteNetwork::mergedInto(Net *net)
{
  ConcreteNet *cnet = reinterpret_cast<ConcreteNet*>(net);
  return reinterpret_cast<Net*>(cnet->mergedInto());
}

////////////////////////////////////////////////////////////////

Cell *
ConcreteInstance::cell() const
{
  return reinterpret_cast<Cell*>(cell_);
}

Instance *
ConcreteNetwork::makeInstance(Cell *cell,
			      const char *name,
			      Instance *parent)
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  return makeConcreteInstance(ccell, name, parent);
}

Instance *
ConcreteNetwork::makeInstance(LibertyCell *cell,
			      const char *name,
			      Instance *parent)
{
  return makeConcreteInstance(cell, name, parent);
}

Instance *
ConcreteNetwork::makeConcreteInstance(ConcreteCell *cell,
				      const char *name,
				      Instance *parent)
{
  ConcreteInstance *cparent =
    reinterpret_cast<ConcreteInstance*>(parent);
  ConcreteInstance *inst = new ConcreteInstance(cell, name, cparent);
  if (parent)
    cparent->addChild(inst);
  return reinterpret_cast<Instance*>(inst);
}

void
ConcreteNetwork::makePins(Instance *inst)
{
  CellPortBitIterator *port_iterator = portBitIterator(cell(inst));
  while (port_iterator->hasNext()) {
    Port *port = port_iterator->next();
    makePin(inst, port, nullptr);
  }
  delete port_iterator;
}

void
ConcreteNetwork::replaceCell(Instance *inst,
			     Cell *cell)
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell);
  int port_count = ccell->portBitCount();
  ConcreteInstance *cinst = reinterpret_cast<ConcreteInstance*>(inst);
  ConcretePin **pins = cinst->pins_;
  ConcretePin **rpins = new ConcretePin*[port_count];
  for (int i = 0; i < port_count; i++)
    rpins[i] = nullptr;
  for (int i = 0; i < port_count; i++) {
    ConcretePin *cpin = pins[i];
    if (cpin) {
      ConcretePort *pin_cport = reinterpret_cast<ConcretePort*>(cpin->port());
      ConcretePort *cport = ccell->findPort(pin_cport->name());
      rpins[cport->pinIndex()] = cpin;
      cpin->port_ = cport;
    }
  }
  delete [] pins;
  cinst->pins_ = rpins;
  cinst->setCell(ccell);
}

void
ConcreteNetwork::deleteInstance(Instance *inst)
{
  ConcreteInstance *cinst = reinterpret_cast<ConcreteInstance*>(inst);

  // Delete nets first (so children pin deletes are not required).
  ConcreteInstanceNetMap::Iterator net_iter(cinst->nets_);
  while (net_iter.hasNext()) {
    ConcreteNet *cnet = net_iter.next();
    Net *net = reinterpret_cast<Net*>(cnet);
    // Delete terminals connected to net.
    NetTermIterator *term_iter = termIterator(net);
    while (term_iter->hasNext()) {
      ConcreteTerm *term = reinterpret_cast<ConcreteTerm*>(term_iter->next());
      delete term;
    }
    delete term_iter;
    deleteNet(net);
  }

  // Delete children.
  InstanceChildIterator *child_iter = childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    deleteInstance(child);
  }
  delete child_iter;

  InstancePinIterator *pin_iter = pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    deletePin(pin);
  }
  delete pin_iter;

  Instance *parent_inst = parent(inst);
  if (parent_inst) {
    ConcreteInstance *cparent =
      reinterpret_cast<ConcreteInstance*>(parent_inst);
    cparent->deleteChild(cinst);
  }
  delete cinst;
}

Pin *
ConcreteNetwork::makePin(Instance *inst,
			 Port *port,
			 Net *net)
{
  ConcreteInstance *cinst = reinterpret_cast<ConcreteInstance*>(inst);
  ConcretePort *cport = reinterpret_cast<ConcretePort*>(port);
  ConcreteNet *cnet = reinterpret_cast<ConcreteNet*>(net);
  ConcretePin *cpin = new ConcretePin(cinst, cport, cnet);
  cinst->addPin(cpin);
  if (cnet)
    connectNetPin(cnet, cpin);
  return reinterpret_cast<Pin*>(cpin);
}

Term *
ConcreteNetwork::makeTerm(Pin *pin,
			  Net *net)
{
  ConcretePin *cpin = reinterpret_cast<ConcretePin*>(pin);
  ConcreteNet *cnet = reinterpret_cast<ConcreteNet*>(net);
  ConcreteTerm *cterm = new ConcreteTerm(cpin, cnet);
  if (cnet)
    cnet->addTerm(cterm);
  cpin->term_ = cterm;
  return reinterpret_cast<Term*>(cterm);
}

Pin *
ConcreteNetwork::connect(Instance *inst,
			 LibertyPort *port,
			 Net *net)
{
  return connect(inst, reinterpret_cast<Port*>(port), net);
}

Pin *
ConcreteNetwork::connect(Instance *inst,
			 Port *port,
			 Net *net)
{
  ConcreteNet *cnet = reinterpret_cast<ConcreteNet*>(net);
  ConcreteInstance *cinst = reinterpret_cast<ConcreteInstance*>(inst);
  ConcretePort *cport = reinterpret_cast<ConcretePort*>(port);
  ConcretePin *cpin = cinst->findPin(port);
  if (cpin) {
    ConcreteNet *prev_net = cpin->net_;
    if (prev_net)
      disconnectNetPin(prev_net, cpin);
  }
  else {
    cpin = new ConcretePin(cinst, cport, cnet);
    cinst->addPin(cpin);
  }
  if (inst == top_instance_) {
    // makeTerm
    ConcreteTerm *cterm = new ConcreteTerm(cpin, cnet);
    cnet->addTerm(cterm);
    cpin->term_ = cterm;
    cpin->net_ = nullptr;
  }
  else {
    cpin->net_ = cnet;
    connectNetPin(cnet, cpin);
  }
  return reinterpret_cast<Pin*>(cpin);
}

void
ConcreteNetwork::connectNetPin(ConcreteNet *cnet,
			       ConcretePin *cpin)
{
  cnet->addPin(cpin);

  // If there are no terminals the net does not span hierarchy levels
  // and it is safe to incrementally update the drivers.
  Pin *pin = reinterpret_cast<Pin*>(cpin);
  if (isDriver(pin)) {
    if (cnet->terms_ == nullptr) {
      Net *net = reinterpret_cast<Net*>(cnet);
      PinSet *drvrs = net_drvr_pin_map_.findKey(net);
      if (drvrs)
	drvrs->insert(pin);
    }
    else
      clearNetDrvrPinMap();
  }
}

void
ConcreteNetwork::disconnectPin(Pin *pin)
{
  ConcretePin *cpin = reinterpret_cast<ConcretePin*>(pin);
  if (reinterpret_cast<Instance*>(cpin->instance()) == top_instance_) {
    ConcreteTerm *cterm = cpin->term_;
    if (cterm) {
      ConcreteNet *cnet = cterm->net_;
      if (cnet) {
	cnet->deleteTerm(cterm);
	clearNetDrvrPinMap();
      }
      cpin->term_ = nullptr;
      delete cterm;
    }
  }
  else {
    ConcreteNet *cnet = cpin->net();
    if (cnet)
      disconnectNetPin(cnet, cpin);
    cpin->net_ = nullptr;
  }
}

void
ConcreteNetwork::disconnectNetPin(ConcreteNet *cnet,
				  ConcretePin *cpin)
{
  cnet->deletePin(cpin);

  Pin *pin = reinterpret_cast<Pin*>(cpin);
  if (isDriver(pin)) {
    ConcreteNet *cnet = cpin->net();
    // If there are no terminals the net does not span hierarchy levels
    // and it is safe to incrementally update the drivers.
    if (cnet->terms_ == nullptr) {
      Net *net = reinterpret_cast<Net*>(cnet);
      PinSet *drvrs = net_drvr_pin_map_.findKey(net);
      if (drvrs)
	drvrs->erase(pin);
    }
    else
      clearNetDrvrPinMap();
  }
}

void
ConcreteNetwork::deletePin(Pin *pin)
{
  ConcretePin *cpin = reinterpret_cast<ConcretePin*>(pin);
  ConcreteNet *cnet = cpin->net();
  if (cnet)
    disconnectNetPin(cnet, cpin);
  ConcreteInstance *cinst =
    reinterpret_cast<ConcreteInstance*>(cpin->instance());
  if (cinst)
    cinst->deletePin(cpin);
  delete cpin;
}

Net *
ConcreteNetwork::makeNet(const char *name,
			 Instance *parent)
{
  ConcreteInstance *cparent = reinterpret_cast<ConcreteInstance*>(parent);
  ConcreteNet *net = new ConcreteNet(name, cparent);
  cparent->addNet(net);
  return reinterpret_cast<Net*>(net);
}

void
ConcreteNetwork::deleteNet(Net *net)
{
  ConcreteNet *cnet = reinterpret_cast<ConcreteNet*>(net);
  ConcreteNetPinIterator pin_iter(cnet);
  while (pin_iter.hasNext()) {
    ConcretePin *pin = reinterpret_cast<ConcretePin*>(pin_iter.next());
    // Do NOT use net->disconnectPin because it would be N^2
    // to delete all of the pins from the net.
    pin->net_ = nullptr;
  }

  constant_nets_[int(LogicValue::zero)].erase(net);
  constant_nets_[int(LogicValue::one)].erase(net);
  PinSet *drvrs = net_drvr_pin_map_.findKey(net);
  if (drvrs) {
    delete drvrs;
    net_drvr_pin_map_.erase(net);
  }

  ConcreteInstance *cinst =
    reinterpret_cast<ConcreteInstance*>(cnet->instance());
  cinst->deleteNet(cnet);
  delete cnet;
}

void
ConcreteNetwork::clearConstantNets()
{
  constant_nets_[int(LogicValue::zero)].clear();
  constant_nets_[int(LogicValue::one)].clear();
}

void
ConcreteNetwork::addConstantNet(Net *net,
				LogicValue value)
{
  constant_nets_[int(value)].insert(net);
}

ConstantPinIterator *
ConcreteNetwork::constantPinIterator()
{
  return new NetworkConstantPinIterator(this,
					constant_nets_[int(LogicValue::zero)],
					constant_nets_[int(LogicValue::one)]);
}

////////////////////////////////////////////////////////////////

// Optimized version of Network::visitConnectedPins.
void
ConcreteNetwork::visitConnectedPins(const Net *net,
				    PinVisitor &visitor,
				    ConstNetSet &visited_nets) const
{
  if (!visited_nets.hasKey(net)) {
    visited_nets.insert(net);
    // Search up from net terminals.
    const ConcreteNet *cnet = reinterpret_cast<const ConcreteNet*>(net);
    for (ConcreteTerm *term = cnet->terms_; term; term = term->net_next_) {
      ConcretePin *above_pin = term->pin_;
      if (above_pin) {
	ConcreteNet *above_net = above_pin->net_;
	if (above_net)
	  visitConnectedPins(reinterpret_cast<Net*>(above_net),
			     visitor, visited_nets);
	else
	  visitor(reinterpret_cast<Pin*>(above_pin));
      }
    }

    // Search down from net pins.
    for (ConcretePin *pin = cnet->pins_; pin; pin = pin->net_next_) {
      visitor(reinterpret_cast<Pin*>(pin));
      ConcreteTerm *below_term = pin->term_;
      if (below_term) {
	ConcreteNet *below_net = below_term->net_;
	if (below_net)
	  visitConnectedPins(reinterpret_cast<Net*>(below_net),
			     visitor, visited_nets);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

ConcreteInstance::ConcreteInstance(ConcreteCell *cell,
				   const char *name,
				   ConcreteInstance *parent) :
  cell_(cell),
  name_(stringCopy(name)),
  parent_(parent),
  children_(nullptr),
  nets_(nullptr)
{
  initPins();
}

void
ConcreteInstance::initPins()
{
  int pin_count = reinterpret_cast<ConcreteCell*>(cell_)->portBitCount();
  if (pin_count) {
    pins_ = new ConcretePin*[pin_count];
    for (int i = 0; i < pin_count; i++)
      pins_[i] = nullptr;
  }
  else
    pins_ = nullptr;
}

ConcreteInstance::~ConcreteInstance()
{
  stringDelete(name_);
  delete [] pins_;
  delete children_;
  delete nets_;
}

Instance *
ConcreteInstance::findChild(const char *name) const
{
  if (children_)
    return reinterpret_cast<Instance*>(children_->findKey(name));
  else
    return nullptr;
}

ConcretePin *
ConcreteInstance::findPin(const char *port_name) const
{
  ConcreteCell *ccell = reinterpret_cast<ConcreteCell*>(cell_);
  const ConcretePort *cport =
    reinterpret_cast<const ConcretePort*>(ccell->findPort(port_name));
  if (cport
      && !cport->isBus())
    return pins_[cport->pinIndex()];
  else
    return nullptr;
}

ConcretePin *
ConcreteInstance::findPin(const Port *port) const
{
  const ConcretePort *cport = reinterpret_cast<const ConcretePort*>(port);
  return pins_[cport->pinIndex()];
}

ConcreteNet *
ConcreteInstance::findNet(const char *net_name) const
{
  ConcreteNet *net = nullptr;
  if (nets_) {
    net = nets_->findKey(net_name);
    // Follow merge pointer to surviving net.
    if (net) {
      while (net->mergedInto())
	net = net->mergedInto();
    }
  }
  return net;
}

void
ConcreteInstance::findNetsMatching(const PatternMatch *pattern,
				   NetSeq *nets) const
{
  if (pattern->hasWildcards()) {
    ConcreteInstanceNetMap::Iterator net_iter(nets_);
    while (net_iter.hasNext()) {
      const char *net_name;
      ConcreteNet *cnet;
      net_iter.next(net_name, cnet);
      if (pattern->match(net_name))
	nets->push_back(reinterpret_cast<Net*>(cnet));
    }
  }
  else {
    ConcreteNet *cnet = findNet(pattern->pattern());
    if (cnet)
      nets->push_back(reinterpret_cast<Net*>(cnet));
  }
}

InstanceNetIterator *
ConcreteInstance::netIterator() const
{
  return reinterpret_cast<InstanceNetIterator*>
    (new ConcreteInstanceNetIterator(nets_));
}

InstanceChildIterator *
ConcreteInstance::childIterator() const
{
  return new ConcreteInstanceChildIterator(children_);
}

void
ConcreteInstance::addChild(ConcreteInstance *child)
{
  if (children_ == nullptr)
    children_ = new ConcreteInstanceChildMap;
  (*children_)[child->name()] = child;
}

void
ConcreteInstance::deleteChild(ConcreteInstance *child)
{
  children_->erase(child->name());
}

void
ConcreteInstance::addPin(ConcretePin *pin)
{
  ConcretePort *cport = reinterpret_cast<ConcretePort *>(pin->port());
  pins_[cport->pinIndex()] = pin;
}

void
ConcreteInstance::deletePin(ConcretePin *pin)
{
  ConcretePort *cport = reinterpret_cast<ConcretePort *>(pin->port());
  pins_[cport->pinIndex()] = nullptr;
}

void
ConcreteInstance::addNet(ConcreteNet *net)
{
  if (nets_ == nullptr)
    nets_ = new ConcreteInstanceNetMap;
  (*nets_)[net->name()] = net;
}

void
ConcreteInstance::addNet(const char *name,
			 ConcreteNet *net)
{
  if (nets_ == nullptr)
    nets_ = new ConcreteInstanceNetMap;
  (*nets_)[name] = net;
}

void
ConcreteInstance::deleteNet(ConcreteNet *net)
{
  nets_->erase(net->name());
}

void
ConcreteInstance::setCell(ConcreteCell *cell)
{
  cell_ = cell;
}

////////////////////////////////////////////////////////////////

ConcretePin::ConcretePin(ConcreteInstance *instance,
			 ConcretePort *port,
			 ConcreteNet *net) :
  instance_(instance),
  port_(port),
  net_(net),
  term_(nullptr),
  net_next_(nullptr),
  net_prev_(nullptr),
  vertex_id_(vertex_id_null)
{
}

const char *
ConcretePin::name() const
{
  return port_->name();
}

void
ConcretePin::setVertexId(VertexId id)
{
  vertex_id_ = id;
}

////////////////////////////////////////////////////////////////

const char *
ConcreteTerm::name() const
{
  ConcretePin *cpin = reinterpret_cast<ConcretePin*>(pin_);
  const ConcretePort *cport =
    reinterpret_cast<const ConcretePort*>(cpin->port());
  return cport->name();
}

ConcreteTerm::ConcreteTerm(ConcretePin *pin,
			   ConcreteNet *net) :
  pin_(pin),
  net_(net),
  net_next_(nullptr)
{
}

////////////////////////////////////////////////////////////////

ConcreteNet::ConcreteNet(const char *name,
			 ConcreteInstance *instance) :
  name_(stringCopy(name)),
  instance_(instance),
  pins_(nullptr),
  terms_(nullptr),
  merged_into_(nullptr)
{
}

ConcreteNet::~ConcreteNet()
{
  stringDelete(name_);
}

// Merged nets are kept around to serve as name aliases.
// Only Instance::findNet and InstanceNetIterator need to know
// the net has been merged.
void
ConcreteNet::mergeInto(ConcreteNet *net)
{
  ConcreteNetPinIterator pin_iter(this);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    ConcretePin *cpin = reinterpret_cast<ConcretePin*>(pin);
    net->addPin(cpin);
    cpin->net_ = net;
  }
  pins_ = nullptr;
  ConcreteNetTermIterator term_iter(this);
  while (term_iter.hasNext()) {
    Term *term = term_iter.next();
    ConcreteTerm *cterm = reinterpret_cast<ConcreteTerm*>(term);
    net->addTerm(cterm);
    cterm->net_ = net;
  }
  terms_ = nullptr;
  // Leave name map pointing to merged net because otherwise a top
  // level merged net has no pointer to it and it is leaked.
  merged_into_ = net;
}

void
ConcreteNet::addPin(ConcretePin *pin)
{
  if (pins_)
    pins_->net_prev_ = pin;
  pin->net_next_ = pins_;
  pin->net_prev_ = nullptr;
  pins_ = pin;
}

void
ConcreteNet::deletePin(ConcretePin *pin)
{
  ConcretePin *prev = pin->net_prev_;
  ConcretePin *next = pin->net_next_;
  if (prev)
    prev->net_next_ = next;
  if (next)
    next->net_prev_ = prev;
  if (pins_ == pin)
    pins_ = next;
}

void
ConcreteNet::addTerm(ConcreteTerm *term)
{
  ConcreteTerm *next = terms_;
  terms_ = term;
  term->net_next_ = next;
}

void
ConcreteNet::deleteTerm(ConcreteTerm *term)
{
  ConcreteTerm *net_prev_term = nullptr;
  for (ConcreteTerm *net_term=terms_;net_term;net_term=net_term->net_next_) {
    if (net_term == term) {
      if (net_prev_term)
	net_prev_term->net_next_ = term->net_next_;
      else
	terms_ = term->net_next_;
      break;
    }
    net_prev_term = net_term;
  }
}

////////////////////////////////////////////////////////////////

typedef Map<Net*, Net*> BindingMap;

// Binding table used for linking/expanding network.
class ConcreteBindingTbl
{
public:
  explicit ConcreteBindingTbl(NetworkEdit *network);
  Net *ensureBinding(Net *proto_net,
		     Instance *parent);
  Net *find(Net *net);
  void bind(Net *proto_net,
	    Net *clone_net);

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteBindingTbl);

  BindingMap map_;
  NetworkEdit *network_;
};

void
ConcreteNetwork::setCellNetworkView(Cell *cell,
				    Instance *inst)
{
  cell_network_view_map_[cell] = inst;
}

Instance *
ConcreteNetwork::cellNetworkView(Cell *cell)
{
  return cell_network_view_map_.findKey(cell);
}

void
ConcreteNetwork::readNetlistBefore()
{
  clearConstantNets();
  deleteTopInstance();
  clearNetDrvrPinMap();
}

void
ConcreteNetwork::setTopInstance(Instance *top_inst)
{
  if (top_instance_) {
    deleteInstance(top_instance_);
    clearConstantNets();
    clearNetDrvrPinMap();
  }
  top_instance_ = top_inst;
}

void
ConcreteNetwork::setLinkFunc(LinkNetworkFunc *link)
{
  link_func_ = link;
}

bool
ConcreteNetwork::linkNetwork(const char *top_cell_name,
			     bool make_black_boxes,
			     Report *report)
{
  if (link_func_) {
    clearConstantNets();
    deleteTopInstance();
    top_instance_ = link_func_(top_cell_name, make_black_boxes, report, this);
    return top_instance_ != nullptr;
  }
  else {
    report->error("cell type %s can not be linked.\n", top_cell_name);
    return false;
  }
}

Instance *
linkReaderNetwork(Cell *top_cell,
		  bool, Report *,
		  NetworkReader *network)
{
  Instance *view = network->cellNetworkView(top_cell);
  if (view) {
    // Seed the recursion for expansion with the top level instance.
    Instance *top_instance = network->makeInstance(top_cell, "", nullptr);
    ConcreteBindingTbl bindings(network);
    makeClonePins(view, top_instance, view, &bindings, nullptr, nullptr, network);
    InstanceChildIterator *child_iter = network->childIterator(view);
    while (child_iter->hasNext()) {
      Instance *child = child_iter->next();
      makeChildNetwork(child, top_instance, &bindings, network);
    }
    delete child_iter;
    network->deleteCellNetworkViews();
    return top_instance;
  }
  return nullptr;
}

static void
makeChildNetwork(Instance *proto,
		 Instance *parent,
		 ConcreteBindingTbl *parent_bindings,
		 NetworkReader *network)
{
  Cell *proto_cell = network->cell(proto);
  Instance *clone = network->makeInstance(proto_cell, network->name(proto),
					  parent);
  if (network->isLeaf(proto_cell))
    makeClonePins(proto, clone, nullptr, nullptr, parent, parent_bindings, network);
  else {
    // Recurse if this isn't a leaf cell.
    ConcreteBindingTbl bindings(network);
    Instance *clone_view = network->cellNetworkView(proto_cell);
    makeClonePins(proto, clone, clone_view, &bindings, parent,
		  parent_bindings, network);
    if (clone_view) {
      InstanceChildIterator *child_iter = network->childIterator(clone_view);
      while (child_iter->hasNext()) {
	Instance *child = child_iter->next();
	makeChildNetwork(child, clone, &bindings, network);
      }
      delete child_iter;
    }
  }
}

static void
makeClonePins(Instance *proto,
	      Instance *clone,
	      Instance *clone_view,
	      ConcreteBindingTbl *bindings,
	      Instance *parent,
	      ConcreteBindingTbl *parent_bindings,
	      NetworkReader *network)
{
  InstancePinIterator *proto_pin_iter = network->pinIterator(proto);
  while (proto_pin_iter->hasNext()) {
    Pin *proto_pin = proto_pin_iter->next();
    Net *proto_net = network->net(proto_pin);
    Port *proto_port = network->port(proto_pin);
    Net *clone_net = nullptr;
    if (proto_net && parent_bindings)
      clone_net = parent_bindings->ensureBinding(proto_net, parent);
    Pin *clone_pin = network->connect(clone, proto_port, clone_net);
    if (clone_view) {
      Pin *clone_proto_pin = network->findPin(clone_view, proto_port);
      Net *clone_proto_net = network->net(clone_proto_pin);
      Net *clone_child_net = nullptr;
      if (clone_proto_net)
	clone_child_net = bindings->ensureBinding(clone_proto_net, clone);
      network->makeTerm(clone_pin, clone_child_net);
    }
  }
  delete proto_pin_iter;
}

////////////////////////////////////////////////////////////////

ConcreteBindingTbl::ConcreteBindingTbl(NetworkEdit *network) :
  network_(network)
{
}

// Follow the merged_into pointers rather than update the
// binding tables up the call tree when nodes are merged
// because the name changes up the hierarchy.
Net *
ConcreteBindingTbl::find(Net *proto_net)
{
  ConcreteNet *net = reinterpret_cast<ConcreteNet*>(map_.findKey(proto_net));
  while (net && net->mergedInto())
    net = net->mergedInto();
  return reinterpret_cast<Net*>(net);
}

void
ConcreteBindingTbl::bind(Net *proto_net,
			 Net *net)
{
  map_[proto_net] = net;
}

Net *
ConcreteBindingTbl::ensureBinding(Net *proto_net,
				  Instance *parent)
{
  Net *net = find(proto_net);
  if (net == nullptr) {
    net = network_->makeNet(network_->name(proto_net), parent);
    map_[proto_net] = net;
  }
  return net;
}

} // namespace
