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

%module network

%{
#include "Network.hh"
%}

////////////////////////////////////////////////////////////////
//
// Empty class definitions to make swig happy.
// Private constructor/destructor so swig doesn't emit them.
//
////////////////////////////////////////////////////////////////

class Library
{
private:
  Library();
  ~Library();
};

class LibraryIterator
{
private:
  LibraryIterator();
  ~LibraryIterator();
};

class Cell
{
private:
  Cell();
  ~Cell();
};

class CellPortIterator
{
private:
  CellPortIterator();
  ~CellPortIterator();
};

class Port
{
private:
  Port();
  ~Port();
};

class PortMemberIterator
{
private:
  PortMemberIterator();
  ~PortMemberIterator();
};

class Instance
{
private:
  Instance();
  ~Instance();
};

class Pin
{
private:
  Pin();
  ~Pin();
};

class Term
{
private:
  Term();
  ~Term();
};

class InstanceChildIterator
{
private:
  InstanceChildIterator();
  ~InstanceChildIterator();
};

class InstancePinIterator
{
private:
  InstancePinIterator();
  ~InstancePinIterator();
};

class InstanceNetIterator
{
private:
  InstanceNetIterator();
  ~InstanceNetIterator();
};

class LeafInstanceIterator
{
private:
  LeafInstanceIterator();
  ~LeafInstanceIterator();
};

class Net
{
private:
  Net();
  ~Net();
};

class NetPinIterator
{
private:
  NetPinIterator();
  ~NetPinIterator();
};

class NetTermIterator
{
private:
  NetTermIterator();
  ~NetTermIterator();
};

class NetConnectedPinIterator
{
private:
  NetConnectedPinIterator();
  ~NetConnectedPinIterator();
};

class PinConnectedPinIterator
{
private:
  PinConnectedPinIterator();
  ~PinConnectedPinIterator();
};

%inline %{

bool
network_is_linked()
{
  return Sta::sta()->cmdNetwork()->isLinked();
}

void
set_path_divider(char divider)
{
  cmdNetwork()->setPathDivider(divider);
}

void
set_current_instance(Instance *inst)
{
  Sta::sta()->setCurrentInstance(inst);
}

// Includes top instance.
int
network_instance_count()
{
  Network *network = cmdNetwork();
  return network->instanceCount();
}

int
network_pin_count()
{
  Network *network = cmdNetwork();
  return network->pinCount();
}

int
network_net_count()
{
  Network *network = cmdNetwork();
  return network->netCount();
}

int
network_leaf_instance_count()
{
  Network *network = cmdNetwork();
  return network->leafInstanceCount();
}

int
network_leaf_pin_count()
{
  Network *network = cmdNetwork();
  return network->leafPinCount();
}

Library *
find_library(const char *name)
{
  return cmdNetwork()->findLibrary(name);
}

LibraryIterator *
library_iterator()
{
  return cmdNetwork()->libraryIterator();
}

CellSeq
find_cells_matching(const char *pattern,
		    bool regexp,
		    bool nocase)
{
  Network *network = cmdNetwork();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  CellSeq matches;
  LibraryIterator *lib_iter = network->libraryIterator();
  while (lib_iter->hasNext()) {
    Library *lib = lib_iter->next();
    CellSeq lib_matches = network->findCellsMatching(lib, &matcher);
    for (Cell *match : lib_matches)
      matches.push_back(match);
  }
  delete lib_iter;
  return matches;
}

void
set_cmd_namespace_cmd(const char *namespc)
{
  Sta *sta = Sta::sta();
  if (stringEq(namespc, "sdc"))
    sta->setCmdNamespace(CmdNamespace::sdc);
  else if (stringEq(namespc, "sta"))
    sta->setCmdNamespace(CmdNamespace::sta);
  else
    sta->report()->warn(2120, "unknown namespace");
}

bool
link_design_cmd(const char *top_cell_name,
                bool make_black_boxes)
{
  return Sta::sta()->linkDesign(top_cell_name, make_black_boxes);
}

Instance *
top_instance()
{
  return cmdLinkedNetwork()->topInstance();
}

LeafInstanceIterator *
leaf_instance_iterator()
{
  return cmdLinkedNetwork()->leafInstanceIterator();
}

const char *
port_direction(const Port *port)
{
  return cmdLinkedNetwork()->direction(port)->name();
}
	     
const char *
pin_direction(const Pin *pin)
{
  return cmdLinkedNetwork()->direction(pin)->name();
}

PortSeq
find_ports_matching(const char *pattern,
		    bool regexp,
		    bool nocase)
{
  Network *network = cmdLinkedNetwork();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  Cell *top_cell = network->cell(network->topInstance());
  PortSeq matches1 = network->findPortsMatching(top_cell, &matcher);
  // Expand bus/bundle ports.
  PortSeq matches;
  for (const Port *port : matches1) {
    if (network->isBus(port)
	|| network->isBundle(port)) {
      PortMemberIterator *member_iter = network->memberIterator(port);
      while (member_iter->hasNext()) {
	Port *member = member_iter->next();
	matches.push_back(member);
      }
      delete member_iter;
    }
    else
      matches.push_back(port);
  }
  return matches;
}

PinSeq
find_port_pins_matching(const char *pattern,
			bool regexp,
			bool nocase)
{
  Network *network = cmdLinkedNetwork();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  Instance *top_inst = network->topInstance();
  Cell *top_cell = network->cell(top_inst);
  PortSeq ports = network->findPortsMatching(top_cell, &matcher);
  PinSeq pins;
  for (const Port *port : ports) {
    if (network->isBus(port)
	|| network->isBundle(port)) {
      PortMemberIterator *member_iter = network->memberIterator(port);
      while (member_iter->hasNext()) {
	Port *member = member_iter->next();
	Pin *pin = network->findPin(top_inst, member);
	if (pin)
	  pins.push_back(pin);
      }
      delete member_iter;
    }
    else {
      Pin *pin = network->findPin(top_inst, port);
      if (pin)
	pins.push_back(pin);
    }
  }
  return pins;
}

Pin *
find_pin(const char *path_name)
{
  return cmdLinkedNetwork()->findPin(path_name);
}

Pin *
get_port_pin(const Port *port)
{
  Network *network = cmdLinkedNetwork();
  return network->findPin(network->topInstance(), port);
}

PinSeq
find_pins_matching(const char *pattern,
		   bool regexp,
		   bool nocase)
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  Instance *current_instance = sta->currentInstance();
  PinSeq matches = network->findPinsMatching(current_instance, &matcher);
  return matches;
}

PinSeq
find_pins_hier_matching(const char *pattern,
			bool regexp,
			bool nocase)
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  Instance *current_instance = sta->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  PinSeq matches = network->findPinsHierMatching(current_instance, &matcher);
  return matches;
}

Instance *
find_instance(char *path_name)
{
  return cmdLinkedNetwork()->findInstance(path_name);
}

InstanceSeq
network_leaf_instances()
{
  return cmdLinkedNetwork()->leafInstances();
}

InstanceSeq
find_instances_matching(const char *pattern,
			bool regexp,
			bool nocase)
{
  Sta *sta = Sta::sta();
  Instance *current_instance = sta->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, sta->tclInterp());
  Network *network = cmdLinkedNetwork();
  InstanceSeq matches = network->findInstancesMatching(current_instance, &matcher);
  return matches;
}

InstanceSeq
find_instances_hier_matching(const char *pattern,
			     bool regexp,
			     bool nocase)
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  Instance *current_instance = sta->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, sta->tclInterp());
  InstanceSeq matches = network->findInstancesHierMatching(current_instance, &matcher);
  return matches;
}

InstanceSet
find_register_instances(ClockSet *clks,
			const RiseFallBoth *clk_tr,
			bool edge_triggered,
			bool latches)
{
  cmdLinkedNetwork();
  InstanceSet insts = Sta::sta()->findRegisterInstances(clks, clk_tr,
                                                        edge_triggered,
                                                        latches);
  delete clks;
  return insts;
}

PinSet
find_register_data_pins(ClockSet *clks,
			const RiseFallBoth *clk_tr,
			bool edge_triggered,
			bool latches)
{
  cmdLinkedNetwork();
  PinSet pins = Sta::sta()->findRegisterDataPins(clks, clk_tr,
                                                 edge_triggered, latches);
  delete clks;
  return pins;
}

PinSet
find_register_clk_pins(ClockSet *clks,
		       const RiseFallBoth *clk_tr,
		       bool edge_triggered,
		       bool latches)
{
  cmdLinkedNetwork();
  PinSet pins = Sta::sta()->findRegisterClkPins(clks, clk_tr,
                                                edge_triggered, latches);
  delete clks;
  return pins;
}

PinSet
find_register_async_pins(ClockSet *clks,
			 const RiseFallBoth *clk_tr,
			 bool edge_triggered,
			 bool latches)
{
  cmdLinkedNetwork();
  PinSet pins = Sta::sta()->findRegisterAsyncPins(clks, clk_tr,
                                                  edge_triggered, latches);
  delete clks;
  return pins;
}

PinSet
find_register_output_pins(ClockSet *clks,
			  const RiseFallBoth *clk_tr,
			  bool edge_triggered,
			  bool latches)
{
  cmdLinkedNetwork();
  PinSet pins = Sta::sta()->findRegisterOutputPins(clks, clk_tr,
                                                   edge_triggered, latches);
  delete clks;
  return pins;
}

Net *
find_net(char *path_name)
{
  return cmdLinkedNetwork()->findNet(path_name);
}

NetSeq
find_nets_matching(const char *pattern,
		   bool regexp,
		   bool nocase)
{
  Network *network = cmdLinkedNetwork();
  Instance *current_instance = Sta::sta()->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  NetSeq matches = network->findNetsMatching(current_instance, &matcher);
  return matches;
}

NetSeq
find_nets_hier_matching(const char *pattern,
			bool regexp,
			bool nocase)
{
  Network *network = cmdLinkedNetwork();
  Instance *current_instance = Sta::sta()->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  NetSeq matches = network->findNetsHierMatching(current_instance, &matcher);
  return matches;
}

PinSet
net_driver_pins(Net *net)
{
  Network *network = cmdLinkedNetwork();
  PinSet pins(network);
  NetConnectedPinIterator *pin_iter = network->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network->isDriver(pin))
      pins.insert(pin);
  }
  delete pin_iter;
  return pins;
}

PinSet
net_load_pins(Net *net)
{
  Network *network = cmdLinkedNetwork();
  PinSet pins(network);
  NetConnectedPinIterator *pin_iter = network->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network->isLoad(pin))
      pins.insert(pin);
  }
  delete pin_iter;
  return pins;
}

const char *
pin_location(const Pin *pin)
{
  Network *network = cmdNetwork();
  double x, y;
  bool exists;
  network->location(pin, x, y, exists);
  // return x/y as tcl list
  if (exists)
    return sta::stringPrintTmp("%f %f", x, y);
  else
    return "";
}

const char *
port_location(const Port *port)
{
  Network *network = cmdNetwork();
  const Pin *pin = network->findPin(network->topInstance(), port);
  return pin_location(pin);
}

%} // inline

////////////////////////////////////////////////////////////////
//
// Object Methods
//
////////////////////////////////////////////////////////////////

%extend Library {
const char *name()
{
  return cmdNetwork()->name(self);
}

Cell *
find_cell(const char *name)
{
  return cmdNetwork()->findCell(self, name);
}

CellSeq
find_cells_matching(const char *pattern,
		    bool regexp,
		    bool nocase)
{
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  CellSeq matches = cmdNetwork()->findCellsMatching(self, &matcher);
  return matches;
}

} // Library methods

%extend LibraryIterator {
bool has_next() { return self->hasNext(); }
Library *next() { return self->next(); }
void finish() { delete self; }
} // LibraryIterator methods

%extend Cell {
const char *name() { return cmdNetwork()->name(self); }
Library *library() { return cmdNetwork()->library(self); }
LibertyCell *liberty_cell() { return cmdNetwork()->libertyCell(self); }
bool is_leaf() { return cmdNetwork()->isLeaf(self); }
CellPortIterator *
port_iterator() { return cmdNetwork()->portIterator(self); }
string get_attribute(const char *key) { return cmdNetwork()->getAttribute(self, key); }

Port *
find_port(const char *name)
{
  return cmdNetwork()->findPort(self, name);
}

PortSeq
find_ports_matching(const char *pattern,
		    bool regexp,
		    bool nocase)
{
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  return cmdNetwork()->findPortsMatching(self, &matcher);
}

} // Cell methods

%extend CellPortIterator {
bool has_next() { return self->hasNext(); }
Port *next() { return self->next(); }
void finish() { delete self; }
} // CellPortIterator methods

%extend Port {
const char *bus_name() { return cmdNetwork()->busName(self); }
Cell *cell() { return cmdNetwork()->cell(self); }
LibertyPort *liberty_port() { return cmdNetwork()->libertyPort(self); }
bool is_bus() { return cmdNetwork()->isBus(self); }
PortMemberIterator *
member_iterator() { return cmdNetwork()->memberIterator(self); }

} // Port methods

%extend PortMemberIterator {
bool has_next() { return self->hasNext(); }
Port *next() { return self->next(); }
void finish() { delete self; }
} // PortMemberIterator methods

%extend Instance {
Instance *parent() { return cmdLinkedNetwork()->parent(self); }
Cell *cell() { return cmdLinkedNetwork()->cell(self); }
LibertyCell *liberty_cell() { return cmdLinkedNetwork()->libertyCell(self); }
bool is_leaf() { return cmdLinkedNetwork()->isLeaf(self); }
InstanceChildIterator *
child_iterator() { return cmdLinkedNetwork()->childIterator(self); }
InstancePinIterator *
pin_iterator() { return cmdLinkedNetwork()->pinIterator(self); }
InstanceNetIterator *
net_iterator() { return cmdLinkedNetwork()->netIterator(self); }
Pin *
find_pin(const char *name)
{
  return cmdLinkedNetwork()->findPin(self, name);
}
string get_attribute(const char *key) { return cmdNetwork()->getAttribute(self, key); }
} // Instance methods

%extend InstanceChildIterator {
bool has_next() { return self->hasNext(); }
Instance *next() { return self->next(); }
void finish() { delete self; }
} // InstanceChildIterator methods

%extend LeafInstanceIterator {
bool has_next() { return self->hasNext(); }
Instance *next() { return self->next(); }
void finish() { delete self; }
} // LeafInstanceIterator methods

%extend InstancePinIterator {
bool has_next() { return self->hasNext(); }
Pin *next() { return self->next(); }
void finish() { delete self; }
} // InstancePinIterator methods

%extend InstanceNetIterator {
bool has_next() { return self->hasNext(); }
Net *next() { return self->next(); }
void finish() { delete self; }
} // InstanceNetIterator methods

%extend Pin {
const char *port_name() { return cmdLinkedNetwork()->portName(self); }
Instance *instance() { return cmdLinkedNetwork()->instance(self); }
Net *net() { return cmdLinkedNetwork()->net(self); }
Port *port() { return cmdLinkedNetwork()->port(self); }
Term *term() { return cmdLinkedNetwork()->term(self); }
LibertyPort *liberty_port() { return cmdLinkedNetwork()->libertyPort(self); }
bool is_driver() { return cmdLinkedNetwork()->isDriver(self); }
bool is_load() { return cmdLinkedNetwork()->isLoad(self); }
bool is_leaf() { return cmdLinkedNetwork()->isLeaf(self); }
bool is_hierarchical() { return cmdLinkedNetwork()->isHierarchical(self); }
bool is_top_level_port() { return cmdLinkedNetwork()->isTopLevelPort(self); }
PinConnectedPinIterator *connected_pin_iterator()
{ return cmdLinkedNetwork()->connectedPinIterator(self); }

Vertex **
vertices()
{
  Vertex *vertex, *vertex_bidirect_drvr;
  static Vertex *vertices[3];

  cmdGraph()->pinVertices(self, vertex, vertex_bidirect_drvr);
  vertices[0] = vertex;
  vertices[1] = vertex_bidirect_drvr;
  vertices[2] = nullptr;
  return vertices;
}

} // Pin methods

%extend PinConnectedPinIterator {
bool has_next() { return self->hasNext(); }
const Pin *next() { return self->next(); }
void finish() { delete self; }
} // PinConnectedPinIterator methods

%extend Term {
Net *net() { return cmdLinkedNetwork()->net(self); }
Pin *pin() { return cmdLinkedNetwork()->pin(self); }
} // Term methods

%extend Net {
Instance *instance() { return cmdLinkedNetwork()->instance(self); }
const Net *highest_connected_net()
{ return cmdLinkedNetwork()->highestConnectedNet(self); }
NetPinIterator *pin_iterator() { return cmdLinkedNetwork()->pinIterator(self);}
NetTermIterator *term_iterator() {return cmdLinkedNetwork()->termIterator(self);}
NetConnectedPinIterator *connected_pin_iterator()
{ return cmdLinkedNetwork()->connectedPinIterator(self); }
bool is_power() { return cmdLinkedNetwork()->isPower(self);}
bool is_ground() { return cmdLinkedNetwork()->isGround(self);}

float
capacitance(Corner *corner,
	    const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  Sta::sta()->connectedCap(self, corner, min_max, pin_cap, wire_cap);
  return pin_cap + wire_cap;
}

float
pin_capacitance(Corner *corner,
		const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  Sta::sta()->connectedCap(self, corner, min_max, pin_cap, wire_cap);
  return pin_cap;
}

float
wire_capacitance(Corner *corner,
		 const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  Sta::sta()->connectedCap(self, corner, min_max, pin_cap, wire_cap);
  return wire_cap;
}

// get_ports -of_objects net
PortSeq
ports()
{
  Network *network = cmdLinkedNetwork();
  PortSeq ports;
  if (network->isTopInstance(network->instance(self))) {
    NetTermIterator *term_iter = network->termIterator(self);
    while (term_iter->hasNext()) {
      Term *term = term_iter->next();
      Port *port = network->port(network->pin(term));
      ports.push_back(port);
    }
    delete term_iter;
  }
  return ports;
}

} // Net methods

%extend NetPinIterator {
bool has_next() { return self->hasNext(); }
const Pin *next() { return self->next(); }
void finish() { delete self; }
} // NetPinIterator methods

%extend NetTermIterator {
bool has_next() { return self->hasNext(); }
const Term *next() { return self->next(); }
void finish() { delete self; }
} // NetTermIterator methods

%extend NetConnectedPinIterator {
bool has_next() { return self->hasNext(); }
const Pin *next() { return self->next(); }
void finish() { delete self; }
} // NetConnectedPinIterator methods

