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

#include <functional>
#include "DisallowCopyAssign.hh"
#include "Network.hh"

namespace sta {

// Base class to encapsulate a network to map names to/from a namespace.
// It forwards member functions to the encapsulated network.
class NetworkNameAdapter : public NetworkEdit
{
public:
  NetworkNameAdapter(Network *network);
  virtual bool linkNetwork(const char *top_cell_name,
			   bool make_black_boxes,
			   Report *report);

  virtual LibraryIterator *libraryIterator() const;
  virtual LibertyLibraryIterator *libertyLibraryIterator() const;
  virtual Library *findLibrary(const char *name);
  virtual LibertyLibrary *findLibertyFilename(const char *filename);
  virtual LibertyLibrary *findLiberty(const char *name);
  virtual const char *name(const Library *library) const;
  virtual Cell *findCell(const Library *library,
			 const char *name) const;
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
  virtual bool hasMembers(const Port *port) const;

  virtual Instance *topInstance() const;
  virtual Cell *cell(const Instance *instance) const;
  virtual Instance *parent(const Instance *instance) const;
  virtual bool isLeaf(const Instance *instance) const;
  virtual Pin *findPin(const Instance *instance,
		       const Port *port) const;
  virtual Pin *findPin(const Instance *instance,
		       const LibertyPort *port) const;

  virtual InstanceChildIterator *
  childIterator(const Instance *instance) const;
  virtual InstancePinIterator *
  pinIterator(const Instance *instance) const;
  virtual InstanceNetIterator *
  netIterator(const Instance *instance) const;

  virtual Port *port(const Pin *pin) const;
  virtual Instance *instance(const Pin *pin) const;
  virtual Net *net(const Pin *pin) const;
  virtual Term *term(const Pin *pin) const;
  virtual PortDirection *direction(const Pin *pin) const;
  virtual VertexId vertexId(const Pin *pin) const;
  virtual void setVertexId(Pin *pin,
			   VertexId id);

  virtual Net *net(const Term *term) const;
  virtual Pin *pin(const Term *term) const;

  virtual Instance *instance(const Net *net) const;
  virtual bool isPower(const Net *net) const;
  virtual bool isGround(const Net *net) const;
  virtual NetPinIterator *pinIterator(const Net *net) const;
  virtual NetTermIterator *termIterator(const Net *net) const;

  virtual ConstantPinIterator *constantPinIterator();

  virtual char pathDivider() const;
  virtual void setPathDivider(char divider);
  virtual char pathEscape() const;
  virtual void setPathEscape(char escape);


  virtual bool isEditable() const;
  virtual LibertyLibrary *makeLibertyLibrary(const char *name,
					     const char *filename);
  virtual Instance *makeInstance(LibertyCell *cell,
				 const char *name,
				 Instance *parent);
  virtual void makePins(Instance *inst);
  virtual void replaceCell(Instance *inst,
			   Cell *to_cell);
  virtual Net *makeNet(const char *name,
		       Instance *parent);
  virtual Pin *connect(Instance *inst,
		       Port *port,
		       Net *net);
  virtual Pin *connect(Instance *inst,
		       LibertyPort *port,
		       Net *net);
  virtual void disconnectPin(Pin *pin);
  virtual void deleteNet(Net *net);
  virtual void deletePin(Pin *pin);
  virtual void deleteInstance(Instance *inst);
  virtual void mergeInto(Net *net,
			 Net *into_net);
  virtual Net *mergedInto(Net *net);

  // Any overloaded functions from the base class must be listed here.
  using Network::name;
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

private:
  DISALLOW_COPY_AND_ASSIGN(NetworkNameAdapter);
};

////////////////////////////////////////////////////////////////

// Encapsulate a network to map names to/from the sdc namespace.
class SdcNetwork : public NetworkNameAdapter
{
public:
  SdcNetwork(Network *network);

  virtual Port *findPort(const Cell *cell,
			 const char *name) const;
  virtual void findPortsMatching(const Cell *cell,
				 const PatternMatch *pattern,
				 PortSeq *ports) const;
  virtual const char *name(const Port *port) const;
  virtual const char *busName(const Port *port) const;

  virtual const char *name(const Instance *instance) const;
  virtual const char *pathName(const Instance *instance) const;
  virtual const char *pathName(const Pin *pin) const;
  virtual const char *portName(const Pin *pin) const;
  virtual const char *name(const Net *net) const;
  virtual const char *pathName(const Net *net) const;

  virtual Instance *findInstance(const char *path_name) const;
  virtual void findInstancesMatching(const Instance *context,
				     const PatternMatch *pattern,
				     InstanceSeq *insts) const;
  virtual Net *findNet(const char *path_name) const;
  virtual Net *findNet(const Instance *instance,
		       const char *net_name) const;
  virtual void findNetsMatching(const Instance *parent,
				const PatternMatch *pattern,
				NetSeq *nets) const;
  virtual void findInstNetsMatching(const Instance *instance,
				    const PatternMatch *pattern,
				    NetSeq *nets) const;
  virtual Instance *findChild(const Instance *parent,
			      const char *name) const;
  virtual Pin *findPin(const char *path_name) const;
  virtual Pin *findPin(const Instance *instance,
		       const char *port_name) const;
  virtual void findPinsMatching(const Instance *instance,
				const PatternMatch *pattern,
				PinSeq *pins) const;

  virtual Instance *makeInstance(LibertyCell *cell,
				 const char *name,
				 Instance *parent);
  virtual Net *makeNet(const char *name,
		       Instance *parent);

  // The following member functions are inherited from the
  // Network class work as is:
  //  findInstPinsMatching(instance, pattern)

  using Network::name;
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
		    PinSeq *pins) const;

  const char *staToSdc(const char *sta_name) const;

private:
  DISALLOW_COPY_AND_ASSIGN(SdcNetwork);
};

// Encapsulate a network to map names to/from the sdc namespace.
Network *
makeSdcNetwork(Network *network);

} // namespace
