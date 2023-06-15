// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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
  ~ConcreteNetwork();
  void clear() override;
  bool linkNetwork(const char *top_cell_name,
                   bool make_black_boxes,
                   Report *report) override;
  Instance *topInstance() const override;

  const char *name(const Library *library) const override;
  ObjectId id(const Library *library) const override;
  LibraryIterator *libraryIterator() const override;
  LibertyLibraryIterator *libertyLibraryIterator() const override;
  Library *findLibrary(const char *name) override;
  LibertyLibrary *findLiberty(const char *name) override;
  Cell *findCell(const Library *library,
                 const char *name) const override;
  Cell *findAnyCell(const char *name) override;
  CellSeq findCellsMatching(const Library *library,
                            const PatternMatch *pattern) const override;

  const char *name(const Cell *cell) const override;
  ObjectId id(const Cell *cell) const override;
  Library *library(const Cell *cell) const override;
  LibertyCell *libertyCell(Cell *cell) const override;
  const LibertyCell *libertyCell(const Cell *cell) const override;
  Cell *cell(LibertyCell *cell) const override;
  const Cell *cell(const LibertyCell *cell) const override;
  const char *filename(const Cell *cell) override;
  Port *findPort(const Cell *cell,
                 const char *name) const override;
  bool isLeaf(const Cell *cell) const override;
  CellPortIterator *portIterator(const Cell *cell) const override;
  CellPortBitIterator *portBitIterator(const Cell *cell) const override;
  int portBitCount(const Cell *cell) const override;

  const char *name(const Port *port) const override;
  ObjectId id(const Port *port) const override;
  Cell *cell(const Port *port) const override;
  LibertyPort *libertyPort(const Port *port) const override;
  PortDirection *direction(const Port *port) const override;
  bool isBundle(const Port *port) const override;
  bool hasMembers(const Port *port) const override;

  bool isBus(const Port *port) const override;
  int size(const Port *port) const override;
  const char *busName(const Port *port) const override;
  Port *findBusBit(const Port *port,
                   int index) const override;
  int fromIndex(const Port *port) const override;
  int toIndex(const Port *port) const override;
  Port *findMember(const Port *port,
                   int index) const override;
  PortMemberIterator *memberIterator(const Port *port) const override;

  const char *name(const Instance *instance) const override;
  ObjectId id(const Instance *instance) const override;
  Cell *cell(const Instance *instance) const override;
  Instance *parent(const Instance *instance) const override;
  bool isLeaf(const Instance *instance) const override;
  Instance *findChild(const Instance *parent,
                      const char *name) const override;
  Pin *findPin(const Instance *instance,
               const char *port_name) const override;
  Pin *findPin(const Instance *instance,
               const Port *port) const override;

  InstanceChildIterator *
  childIterator(const Instance *instance) const override;
  InstancePinIterator *
  pinIterator(const Instance *instance) const override;
  InstanceNetIterator *
  netIterator(const Instance *instance) const override;

  ObjectId id(const Pin *pin) const override;
  Instance *instance(const Pin *pin) const override;
  Net *net(const Pin *pin) const override;
  Term *term(const Pin *pin) const override;
  Port *port(const Pin *pin) const override;
  PortDirection *direction(const Pin *pin) const override;
  VertexId vertexId(const Pin *pin) const override;
  void setVertexId(Pin *pin,
                   VertexId id) override;

  ObjectId id(const Term *term) const override;
  Net *net(const Term *term) const override;
  Pin *pin(const Term *term) const override;

  const char *name(const Net *net) const override;
  ObjectId id(const Net *net) const override;
  Net *findNet(const Instance *instance,
               const char *net_name) const override;
  void findInstNetsMatching(const Instance *instance,
                            const PatternMatch *pattern,
                            NetSeq &matches) const override;
  Instance *instance(const Net *net) const override;
  bool isPower(const Net *net) const override;
  bool isGround(const Net *net) const override;
  NetPinIterator *pinIterator(const Net *net) const override;
  NetTermIterator *termIterator(const Net *net) const override;
  void mergeInto(Net *net,
                 Net *into_net) override;
  Net *mergedInto(Net *net) override;

  ConstantPinIterator *constantPinIterator() override;
  void addConstantNet(Net *net,
		      LogicValue value) override;

  // Edit methods.
  Library *makeLibrary(const char *name,
                       const char *filename) override;
  LibertyLibrary *makeLibertyLibrary(const char *name,
                                     const char *filename) override;
  void deleteLibrary(Library *library) override;
  Cell *makeCell(Library *library,
                 const char *name,
                 bool is_leaf,
                 const char *filename) override;
  void deleteCell(Cell *cell) override;
  void setName(Cell *cell,
               const char *name) override;
  void setIsLeaf(Cell *cell,
                 bool is_leaf) override;
  Port *makePort(Cell *cell,
                 const char *name) override;
  Port *makeBusPort(Cell *cell,
                    const char *name,
                    int from_index,
                    int to_index) override;
  void groupBusPorts(Cell *cell,
                     std::function<bool(const char*)> port_msb_first) override;
  Port *makeBundlePort(Cell *cell,
                       const char *name,
                       PortSeq *members) override;
  void setDirection(Port *port,
                    PortDirection *dir) override;
  // For NetworkEdit.
  Instance *makeInstance(LibertyCell *cell,
                         const char *name,
                         Instance *parent) override;
  void makePins(Instance *inst) override;
  // For linking.
  Instance *makeInstance(Cell *cell,
                         const char *name,
                         Instance *parent) override;
  void replaceCell(Instance *inst,
                   Cell *cell) override;
  void deleteInstance(Instance *inst) override;
  Pin *connect(Instance *inst,
               Port *port,
               Net *net) override;
  Pin *connect(Instance *inst,
               LibertyPort *port,
               Net *net) override;
  void disconnectPin(Pin *pin) override;
  void deletePin(Pin *pin) override;
  Net *makeNet(const char *name,
               Instance *parent) override;
  void deleteNet(Net *net) override;

  // For NetworkReader API.
  Term *makeTerm(Pin *pin,
                 Net *net) override;
  Pin *makePin(Instance *inst,
               Port *port,
               Net *net) override;

  // Instance is the network view for cell.
  void setCellNetworkView(Cell *cell,
                          Instance *inst) override;
  Instance *cellNetworkView(Cell *cell) override;
  void deleteCellNetworkViews() override;

  void readNetlistBefore() override;
  void setLinkFunc(LinkNetworkFunc *link) override;
  static ObjectId nextObjectId();

  // Used by external tools.
  void setTopInstance(Instance *top_inst);
  void deleteTopInstance();

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
  void setName(const char *name);
  void clearConstantNets();
  void visitConnectedPins(const Net *net,
                          PinVisitor &visitor,
                          NetSet &visited_nets) const override;
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
  static ObjectId object_id_;

private:
  friend class ConcreteLibertyLibraryIterator;
};

class ConcreteInstance
{
public:
  const char *name() const { return name_; }
  ObjectId id() const { return id_; }
  Cell *cell() const;
  ConcreteInstance *parent() const { return parent_; }
  ConcretePin *findPin(const char *port_name) const;
  ConcretePin *findPin(const Port *port) const;
  ConcreteNet *findNet(const char *net_name) const;
  void findNetsMatching(const PatternMatch *pattern,
                        NetSeq &matches) const;
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
  ConcreteInstance(const char *name,
		   ConcreteCell *cell,
                   ConcreteInstance *parent);
  ~ConcreteInstance();

  const char *name_;
  ObjectId id_;
  ConcreteCell *cell_;
  ConcreteInstance *parent_;
  // Array of pins indexed by pin->port->index().
  ConcretePin **pins_;
  ConcreteInstanceChildMap *children_;
  ConcreteInstanceNetMap *nets_;

private:
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
  ObjectId id() const { return id_; }
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
  ObjectId id_;
  // Doubly linked list of net pins.
  ConcretePin *net_next_;
  ConcretePin *net_prev_;
  VertexId vertex_id_;

private:
  friend class ConcreteNetwork;
  friend class ConcreteNet;
  friend class ConcreteNetPinIterator;
};

class ConcreteTerm
{
public:
  const char *name() const;
  ObjectId id() const { return id_; }
  ConcreteNet *net() const { return net_; }
  ConcretePin *pin() const { return pin_; }

protected:
  ~ConcreteTerm() {}
  ConcreteTerm(ConcretePin *pin,
	       ConcreteNet *net);

  ConcretePin *pin_;
  ConcreteNet *net_;
  ObjectId id_;
  // Linked list of net terms.
  ConcreteTerm *net_next_;

private:
  friend class ConcreteNetwork;
  friend class ConcreteNet;
  friend class ConcreteNetTermIterator;
};

class ConcreteNet
{
public:
  const char *name() const { return name_; }
  ObjectId id() const { return id_; }
  ConcreteInstance *instance() const { return instance_; }
  void addPin(ConcretePin *pin);
  void deletePin(ConcretePin *pin);
  void addTerm(ConcreteTerm *term);
  void deleteTerm(ConcreteTerm *term);
  void mergeInto(ConcreteNet *net);
  ConcreteNet *mergedInto() { return merged_into_; }

protected:
  ConcreteNet(const char *name,
	      ConcreteInstance *instance);
  ~ConcreteNet();
  const char *name_;
  ObjectId id_;
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
