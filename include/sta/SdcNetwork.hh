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

#include "Network.hh"

namespace sta {

// Base class to encapsulate a network to map names to/from a namespace.
// It forwards member functions to the encapsulated network.
class NetworkNameAdapter : public NetworkEdit
{
public:
  NetworkNameAdapter(Network *network);
  bool linkNetwork(const char *top_cell_name,
                   bool make_black_boxes,
                   Report *report) override;

  const char *name(const Library *library) const override;
  ObjectId id(const Library *library) const override;
  LibraryIterator *libraryIterator() const override;
  LibertyLibraryIterator *libertyLibraryIterator() const override;
  Library *findLibrary(const char *name) override;
  LibertyLibrary *findLibertyFilename(const char *filename) override;
  LibertyLibrary *findLiberty(const char *name) override;
  Cell *findCell(const Library *library,
                 const char *name) const override;
  CellSeq findCellsMatching(const Library *library,
                            const PatternMatch *pattern) const override;

  const char *name(const Cell *cell) const override;
  string getAttribute(const Cell *cell,
                                 const string &key) const override;
  ObjectId id(const Cell *cell) const override;
  Library *library(const Cell *cell) const override;
  LibertyCell *libertyCell(Cell *cell) const override;
  const LibertyCell *libertyCell(const Cell *cell) const override;
  Cell *cell(LibertyCell *cell) const override;
  const Cell *cell(const LibertyCell *cell) const override;
  const char *filename(const Cell *cell) override;
  Port *findPort(const Cell *cell,
                 const char *name) const override;
  PortSeq findPortsMatching(const Cell *cell,
                            const PatternMatch *pattern) const override;
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
  bool hasMembers(const Port *port) const override;

  ObjectId id(const Instance *instance) const override;
  string getAttribute(const Instance *inst,
                      const string &key) const override;
  Instance *topInstance() const override;
  Cell *cell(const Instance *instance) const override;
  Instance *parent(const Instance *instance) const override;
  bool isLeaf(const Instance *instance) const override;
  Pin *findPin(const Instance *instance,
               const Port *port) const override;
  Pin *findPin(const Instance *instance,
               const LibertyPort *port) const override;

  InstanceChildIterator *
  childIterator(const Instance *instance) const override;
  InstancePinIterator *
  pinIterator(const Instance *instance) const override;
  InstanceNetIterator *
  netIterator(const Instance *instance) const override;

  ObjectId id(const Pin *pin) const override;
  Port *port(const Pin *pin) const override;
  Instance *instance(const Pin *pin) const override;
  Net *net(const Pin *pin) const override;
  Term *term(const Pin *pin) const override;
  PortDirection *direction(const Pin *pin) const override;
  VertexId vertexId(const Pin *pin) const override;
  void setVertexId(Pin *pin,
                   VertexId id) override;
  void location(const Pin *pin,
                // Return values.
                double &x,
                double &y,
                bool &exists) const override;

  ObjectId id(const Term *term) const override;
  Net *net(const Term *term) const override;
  Pin *pin(const Term *term) const override;

  ObjectId id(const Net *net) const override;
  Instance *instance(const Net *net) const override;
  bool isPower(const Net *net) const override;
  bool isGround(const Net *net) const override;
  NetPinIterator *pinIterator(const Net *net) const override;
  NetTermIterator *termIterator(const Net *net) const override;

  ConstantPinIterator *constantPinIterator() override;

  char pathDivider() const override;
  void setPathDivider(char divider) override;
  char pathEscape() const override;
  void setPathEscape(char escape) override;

  bool isEditable() const override;
  LibertyLibrary *makeLibertyLibrary(const char *name,
                                     const char *filename) override;
  Instance *makeInstance(LibertyCell *cell,
                         const char *name,
                         Instance *parent) override;
  void makePins(Instance *inst) override;
  void replaceCell(Instance *inst,
                   Cell *to_cell) override;
  Net *makeNet(const char *name,
               Instance *parent) override;
  Pin *connect(Instance *inst,
               Port *port,
               Net *net) override;
  Pin *connect(Instance *inst,
               LibertyPort *port,
               Net *net) override;
  void disconnectPin(Pin *pin) override;
  void deleteNet(Net *net) override;
  void deletePin(Pin *pin) override;
  void deleteInstance(Instance *inst) override;
  void mergeInto(Net *net,
                 Net *into_net) override;
  Net *mergedInto(Net *net) override;

  // Any overloaded functions from the base class must be listed here.
  using Network::name;
  using Network::id;
  using Network::isLeaf;
  using Network::netIterator;
  using Network::findPin;
  using Network::libertyLibrary;
  using Network::libertyCell;
  using Network::libertyPort;

protected:
  // Network that is being adapted to the sdc namespace.
  Network *network_;
  // network_ if it supports edits.
  NetworkEdit *network_edit_;
};

////////////////////////////////////////////////////////////////

// Encapsulate a network to map names to/from the sdc namespace.
class SdcNetwork : public NetworkNameAdapter
{
public:
  SdcNetwork(Network *network);

  Port *findPort(const Cell *cell,
                 const char *name) const override;
  PortSeq findPortsMatching(const Cell *cell,
                            const PatternMatch *pattern) const override;
  const char *name(const Port *port) const override;
  const char *busName(const Port *port) const override;

  const char *name(const Instance *instance) const override;
  const char *pathName(const Instance *instance) const override;
  const char *pathName(const Pin *pin) const override;
  const char *portName(const Pin *pin) const override;

  const char *name(const Net *net) const override;
  const char *pathName(const Net *net) const override;

  Instance *findInstance(const char *path_name) const override;
  InstanceSeq findInstancesMatching(const Instance *context,
                                    const PatternMatch *pattern) const override;
  Net *findNet(const char *path_name) const override;
  Net *findNet(const Instance *instance,
               const char *net_name) const override;
  NetSeq findNetsMatching(const Instance *parent,
                          const PatternMatch *pattern) const override;
  void findInstNetsMatching(const Instance *instance,
                            const PatternMatch *pattern,
                            NetSeq &nets) const override;
  Instance *findChild(const Instance *parent,
                      const char *name) const override;
  Pin *findPin(const char *path_name) const override;
  Pin *findPin(const Instance *instance,
               const char *port_name) const override;
  PinSeq findPinsMatching(const Instance *instance,
                          const PatternMatch *pattern) const override;

  Instance *makeInstance(LibertyCell *cell,
                         const char *name,
                         Instance *parent) override;
  Net *makeNet(const char *name,
               Instance *parent) override;

  // The following member functions are inherited from the
  // Network class work as is:
  //  findInstPinsMatching(instance, pattern)

  using Network::name;
  using Network::id;
  using Network::findPin;

protected:
  void parsePath(const char *path,
		 // Return values.
		 Instance *&inst,
		 const char *&path_tail) const;
  void scanPath(const char *path,
		// Return values.
		// Unescaped divider count.
		int &divider_count,
		int &path_length) const;
  void parsePath(const char *path,
		 int divider_count,
		 int path_length,
		 // Return values.
		 Instance *&inst,
		 const char *&path_tail) const;
  bool visitMatches(const Instance *parent,
		    const PatternMatch *pattern,
		    std::function<bool (const Instance *instance,
					const PatternMatch *tail)>
		    visit_tail) const;
  bool visitPinTail(const Instance *instance,
		    const PatternMatch *tail,
		    PinSeq &matches) const;
  void findInstancesMatching1(const Instance *context,
                              const PatternMatch *pattern,
                              InstanceSeq &matches) const;

  const char *staToSdc(const char *sta_name) const;
};

// Encapsulate a network to map names to/from the sdc namespace.
Network *
makeSdcNetwork(Network *network);

} // namespace
