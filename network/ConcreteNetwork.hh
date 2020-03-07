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
#include "Set.hh"
#include "StringUtil.hh"
#include "Network.hh"
#include "LibertyClass.hh"

namespace  sta {

class Report;
class ConcreteLibrary;
class ConcreteCell;
class ConcretePin;
class ConcreteInstance;
class ConcreteNet;
class ConcreteTerm;
class ConcretePort;
class ConcreteBindingTbl;
class ConcreteLibertyLibraryIterator;

typedef Vector<ConcreteLibrary*> ConcreteLibrarySeq;
typedef Map<const char*, ConcreteLibrary*, CharPtrLess> ConcreteLibraryMap;
typedef ConcreteLibrarySeq::ConstIterator ConcreteLibraryIterator;
typedef Map<const char *, ConcreteInstance*,
	    CharPtrLess> ConcreteInstanceChildMap;
typedef Map<const char *, ConcreteNet*, CharPtrLess> ConcreteInstanceNetMap;
typedef Vector<ConcreteNet*> ConcreteNetSeq;
typedef Map<Cell*, Instance*> CellNetworkViewMap;
typedef Set<const ConcreteNet*> ConcreteNetSet;

// This adapter implements the network api for the concrete network.
// A superset of the Network api methods are implemented in the interface.
class ConcreteNetwork : public NetworkReader
{
public:
  ConcreteNetwork();
  virtual ~ConcreteNetwork();
  virtual void clear();
  virtual bool linkNetwork(const char *top_cell_name,
			   bool make_black_boxes,
			   Report *report);
  virtual Instance *topInstance() const;

  virtual LibraryIterator *libraryIterator() const;
  virtual LibertyLibraryIterator *libertyLibraryIterator() const ;
  virtual Library *findLibrary(const char *name);
  virtual const char *name(const Library *library) const;
  virtual LibertyLibrary *findLiberty(const char *name);
  virtual LibertyLibrary *libertyLibrary(Library *library) const;
  virtual Cell *findCell(const Library *library,
			 const char *name) const;
  virtual Cell *findAnyCell(const char *name);
  virtual void findCellsMatching(const Library *library,
				 const PatternMatch *pattern,
				 CellSeq *cells) const;

  virtual const char *name(const Cell *cell) const;
  virtual Library *library(const Cell *cell) const;
  virtual LibertyCell *libertyCell(Cell *cell) const;
  virtual const LibertyCell *libertyCell(const Cell *cell) const;
  virtual Cell *cell(LibertyCell *cell) const;
  virtual const Cell *cell(const LibertyCell *cell) const;
  virtual const char *filename(const Cell *cell);
  virtual Port *findPort(const Cell *cell,
			 const char *name) const;
  virtual void findPortsMatching(const Cell *cell,
				 const PatternMatch *pattern,
				 PortSeq *ports) const;
  virtual bool isLeaf(const Cell *cell) const;
  virtual CellPortIterator *portIterator(const Cell *cell) const;
  virtual CellPortBitIterator *portBitIterator(const Cell *cell) const;
  virtual int portBitCount(const Cell *cell) const;

  virtual const char *name(const Port *port) const;
  virtual Cell *cell(const Port *port) const;
  virtual LibertyPort *libertyPort(const Port *port) const;
  virtual PortDirection *direction(const Port *port) const;
  virtual bool isBundle(const Port *port) const;
  virtual bool hasMembers(const Port *port) const;

  virtual bool isBus(const Port *port) const;
  virtual int size(const Port *port) const;
  virtual const char *busName(const Port *port) const;
  virtual Port *findBusBit(const Port *port,
			   int index) const;
  virtual int fromIndex(const Port *port) const;
  virtual int toIndex(const Port *port) const;
  virtual Port *findMember(const Port *port,
			   int index) const;
  virtual PortMemberIterator *memberIterator(const Port *port) const;

  virtual const char *name(const Instance *instance) const;
  virtual Cell *cell(const Instance *instance) const;
  virtual Instance *parent(const Instance *instance) const;
  virtual bool isLeaf(const Instance *instance) const;
  virtual Instance *findChild(const Instance *parent,
			      const char *name) const;
  virtual Pin *findPin(const Instance *instance,
		       const char *port_name) const;
  virtual Pin *findPin(const Instance *instance,
		       const Port *port) const;

  virtual InstanceChildIterator *
  childIterator(const Instance *instance) const;
  virtual InstancePinIterator *
  pinIterator(const Instance *instance) const;
  virtual InstanceNetIterator *
  netIterator(const Instance *instance) const;

  virtual Instance *instance(const Pin *pin) const;
  virtual Net *net(const Pin *pin) const;
  virtual Term *term(const Pin *pin) const;
  virtual Port *port(const Pin *pin) const;
  virtual PortDirection *direction(const Pin *pin) const;
  virtual VertexId vertexId(const Pin *pin) const;
  virtual void setVertexId(Pin *pin,
			   VertexId id);

  virtual Net *net(const Term *term) const;
  virtual Pin *pin(const Term *term) const;

  virtual Net *findNet(const Instance *instance,
		       const char *net_name) const;
  virtual void findInstNetsMatching(const Instance *instance,
				    const PatternMatch *pattern,
				    NetSeq *nets) const;
  virtual const char *name(const Net *net) const;
  virtual Instance *instance(const Net *net) const;
  virtual bool isPower(const Net *net) const;
  virtual bool isGround(const Net *net) const;
  virtual NetPinIterator *pinIterator(const Net *net) const;
  virtual NetTermIterator *termIterator(const Net *net) const;
  virtual void mergeInto(Net *net,
			 Net *into_net);
  virtual Net *mergedInto(Net *net);

  virtual ConstantPinIterator *constantPinIterator();
  void addConstantNet(Net *net,
		      LogicValue value);

  // Edit methods.
  virtual Library *makeLibrary(const char *name,
			       const char *filename);
  virtual LibertyLibrary *makeLibertyLibrary(const char *name,
					     const char *filename);
  virtual Cell *makeCell(Library *library,
			 const char *name,
			 bool is_leaf,
			 const char *filename);
  virtual void deleteCell(Cell *cell);
  virtual void setName(Cell *cell,
		       const char *name);
  virtual void setIsLeaf(Cell *cell,
			 bool is_leaf);
  virtual Port *makePort(Cell *cell,
			 const char *name);
  virtual Port *makeBusPort(Cell *cell,
			    const char *name,
			    int from_index,
			    int to_index);
  virtual void groupBusPorts(Cell *cell);
  virtual Port *makeBundlePort(Cell *cell,
			       const char *name,
			       PortSeq *members);
  virtual void setDirection(Port *port,
			    PortDirection *dir);
  // For NetworkEdit.
  virtual Instance *makeInstance(LibertyCell *cell,
				 const char *name,
				 Instance *parent);
  void makePins(Instance *inst);
  // For linking.
  virtual Instance *makeInstance(Cell *cell,
				 const char *name,
				 Instance *parent);
  virtual void replaceCell(Instance *inst,
			   Cell *cell);
  virtual void deleteInstance(Instance *inst);
  virtual Pin *connect(Instance *inst,
		       Port *port,
		       Net *net);
  virtual Pin *connect(Instance *inst,
		       LibertyPort *port,
		       Net *net);
  virtual void disconnectPin(Pin *pin);
  virtual void deletePin(Pin *pin);
  virtual Net *makeNet(const char *name,
		       Instance *parent);
  virtual void deleteNet(Net *net);

  // For NetworkReader API.
  virtual Term *makeTerm(Pin *pin,
			 Net *net);
  virtual Pin *makePin(Instance *inst,
		       Port *port,
		       Net *net);

  // Instance is the network view for cell.
  virtual void setCellNetworkView(Cell *cell,
				  Instance *inst);
  virtual Instance *cellNetworkView(Cell *cell);
  virtual void deleteCellNetworkViews();
  void deleteTopInstance();

  virtual void readNetlistBefore();
  virtual void setLinkFunc(LinkNetworkFunc *link);
  void setTopInstance(Instance *top_inst);

  using Network::netIterator;
  using Network::findPin;
  using Network::findNet;
  using Network::findNetsMatching;
  using Network::libertyLibrary;
  using Network::libertyCell;
  using Network::libertyPort;
  using Network::isLeaf;

protected:
  void addLibrary(ConcreteLibrary *library);
  void deleteLibrary(ConcreteLibrary *library);
  void setName(const char *name);
  void clearConstantNets();
  virtual void visitConnectedPins(const Net *net,
				  PinVisitor &visitor,
				  ConstNetSet &visited_nets) const;
  Instance *makeConcreteInstance(ConcreteCell *cell,
				 const char *name,
				 Instance *parent);
  void disconnectNetPin(ConcreteNet *cnet,
			ConcretePin *cpin);
  void connectNetPin(ConcreteNet *cnet,
		     ConcretePin *cpin);

  // Cell lookup search order sequence.
  ConcreteLibrarySeq library_seq_;
  ConcreteLibraryMap library_map_;
  Instance *top_instance_;
  NetSet constant_nets_[2];  // LogicValue::zero/one
  LinkNetworkFunc *link_func_;
  CellNetworkViewMap cell_network_view_map_;

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteNetwork);

  friend class ConcreteLibertyLibraryIterator;
};

class ConcreteInstance
{
public:
  const char *name() const { return name_; }
  Cell *cell() const;
  ConcreteInstance *parent() const { return parent_; }
  ConcretePin *findPin(const char *port_name) const;
  ConcretePin *findPin(const Port *port) const;
  ConcreteNet *findNet(const char *net_name) const;
  void findNetsMatching(const PatternMatch *pattern,
			NetSeq *nets) const;
  InstanceNetIterator *netIterator() const;
  Instance *findChild(const char *name) const;
  InstanceChildIterator *childIterator() const;
  void addChild(ConcreteInstance *child);
  void deleteChild(ConcreteInstance *child);
  void addPin(ConcretePin *pin);
  void deletePin(ConcretePin *pin);
  void addNet(ConcreteNet *net);
  void addNet(const char *name,
	      ConcreteNet *net);
  void deleteNet(ConcreteNet *net);
  void setCell(ConcreteCell *cell);
  void initPins();

protected:
  ConcreteInstance(ConcreteCell *cell,
		   const char *name,
		   ConcreteInstance *parent);
  ~ConcreteInstance();

  ConcreteCell *cell_;
  const char *name_;
  ConcreteInstance *parent_;
  // Array of pins indexed by pin->port->index().
  ConcretePin **pins_;
  ConcreteInstanceChildMap *children_;
  ConcreteInstanceNetMap *nets_;

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteInstance);

  friend class ConcreteNetwork;
  friend class ConcreteInstancePinIterator;
};

class ConcretePin
{
public:
  const char *name() const;
  ConcreteInstance *instance() const { return instance_; }
  ConcreteNet *net() const { return net_; }
  ConcretePort *port() const { return port_; }
  ConcreteTerm *term() const { return term_; }
  VertexId vertexId() const { return vertex_id_; }
  void setVertexId(VertexId id);

protected:
  ~ConcretePin() {}
  ConcretePin(ConcreteInstance *instance,
	      ConcretePort *port,
	      ConcreteNet *net);

  ConcreteInstance *instance_;
  ConcretePort *port_;
  ConcreteNet *net_;
  ConcreteTerm *term_;
  // Doubly linked list of net pins.
  ConcretePin *net_next_;
  ConcretePin *net_prev_;
  VertexId vertex_id_;

private:
  DISALLOW_COPY_AND_ASSIGN(ConcretePin);

  friend class ConcreteNetwork;
  friend class ConcreteNet;
  friend class ConcreteNetPinIterator;
};

class ConcreteTerm
{
public:
  const char *name() const;
  ConcreteNet *net() const { return net_; }
  ConcretePin *pin() const { return pin_; }

protected:
  ~ConcreteTerm() {}
  ConcreteTerm(ConcretePin *pin,
	       ConcreteNet *net);

  ConcretePin *pin_;
  ConcreteNet *net_;
  // Linked list of net terms.
  ConcreteTerm *net_next_;

private:
  DISALLOW_COPY_AND_ASSIGN(ConcreteTerm);

  friend class ConcreteNetwork;
  friend class ConcreteNet;
  friend class ConcreteNetTermIterator;
};

class ConcreteNet
{
public:
  const char *name() const { return name_; }
  ConcreteInstance *instance() const { return instance_; }
  void addPin(ConcretePin *pin);
  void deletePin(ConcretePin *pin);
  void addTerm(ConcreteTerm *term);
  void deleteTerm(ConcreteTerm *term);
  void mergeInto(ConcreteNet *net);
  ConcreteNet *mergedInto() { return merged_into_; }

protected:
  DISALLOW_COPY_AND_ASSIGN(ConcreteNet);
  ConcreteNet(const char *name,
	      ConcreteInstance *instance);
  ~ConcreteNet();
  const char *name_;
  ConcreteInstance *instance_;
  // Pointer to head of linked list of pins.
  ConcretePin *pins_;
  // Pointer to head of linked list of terminals.
  // These terminals correspond to the pins attached to the instance that
  // contains this net in the hierarchy level above.
  ConcreteTerm *terms_;
  ConcreteNet *merged_into_;

  friend class ConcreteNetwork;
  friend class ConcreteNetTermIterator;
  friend class ConcreteNetPinIterator;
};

} // namespace
