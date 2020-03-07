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
#include "StringUtil.hh"
#include "PatternMatch.hh"
#include "ParseBus.hh"
#include "SdcNetwork.hh"

namespace sta {

static const char *
escapeDividers(const char *token,
	       const Network *network);
static const char *
escapeBrackets(const char *token,
	       const Network *network);

NetworkNameAdapter::NetworkNameAdapter(Network *network) :
  NetworkEdit(),
  network_(network),
  network_edit_(dynamic_cast<NetworkEdit*>(network))
{
}

bool
NetworkNameAdapter::linkNetwork(const char *top_cell_name,
				bool make_black_boxes,
				Report *report)
{
  return network_->linkNetwork(top_cell_name, make_black_boxes, report);
}

Instance *
NetworkNameAdapter::topInstance() const
{
  return network_->topInstance();
}

LibraryIterator *
NetworkNameAdapter::libraryIterator() const
{
  return network_->libraryIterator();
}

LibertyLibraryIterator *
NetworkNameAdapter::libertyLibraryIterator() const
{
  return network_->libertyLibraryIterator();
}

Library *
NetworkNameAdapter::findLibrary(const char *name)
{
  return network_->findLibrary(name);
}

LibertyLibrary *
NetworkNameAdapter::findLiberty(const char *name)
{
  return network_->findLiberty(name);
}

LibertyLibrary *
NetworkNameAdapter::findLibertyFilename(const char *filename)
{
  return network_->findLibertyFilename(filename);
}

const char *
NetworkNameAdapter::name(const Library *library) const
{
  return network_->name(library);
}

Cell *
NetworkNameAdapter::findCell(const Library *library,
			     const char *name) const
{
  return network_->findCell(library, name);
}

void
NetworkNameAdapter::findCellsMatching(const Library *library,
				      const PatternMatch *pattern,
				      CellSeq *cells) const
{
  network_->findCellsMatching(library, pattern, cells);
}

////////////////////////////////////////////////////////////////

const char *
NetworkNameAdapter::name(const Cell *cell) const
{
  return network_->name(cell);
}

Library *
NetworkNameAdapter::library(const Cell *cell) const
{
  return network_->library(cell);
}

const char *
NetworkNameAdapter::filename(const Cell *cell)
{
  return network_->filename(cell);
}

LibertyCell *
NetworkNameAdapter::libertyCell(Cell *cell) const
{
  return network_->libertyCell(cell);
}

const LibertyCell *
NetworkNameAdapter::libertyCell(const Cell *cell) const
{
  return network_->libertyCell(cell);
}

Cell *
NetworkNameAdapter::cell(LibertyCell *cell) const
{
  return network_->cell(cell);
}

const Cell *
NetworkNameAdapter::cell(const LibertyCell *cell) const
{
  return network_->cell(cell);
}

Port *
NetworkNameAdapter::findPort(const Cell *cell,
			     const char *name) const
{
  return network_->findPort(cell, name);
}

void
NetworkNameAdapter::findPortsMatching(const Cell *cell,
				      const PatternMatch *pattern,
				      PortSeq *ports) const
{
  network_->findPortsMatching(cell, pattern, ports);
}

bool
NetworkNameAdapter::isLeaf(const Cell *cell) const
{
  return network_->isLeaf(cell);
}

CellPortIterator *
NetworkNameAdapter::portIterator(const Cell *cell) const
{
  return network_->portIterator(cell);
}

CellPortBitIterator *
NetworkNameAdapter::portBitIterator(const Cell *cell) const
{
  return network_->portBitIterator(cell);
}

int
NetworkNameAdapter::portBitCount(const Cell *cell) const
{
  return network_->portBitCount(cell);
}

////////////////////////////////////////////////////////////////

const char *
NetworkNameAdapter::name(const Port *port) const
{
  return network_->name(port);
}

Cell *
NetworkNameAdapter::cell(const Port *port) const
{
  return network_->cell(port);
}

LibertyPort *
NetworkNameAdapter::libertyPort(const Port *port) const
{
  return network_->libertyPort(port);
}

PortDirection *
NetworkNameAdapter::direction(const Port *port) const
{
  return network_->direction(port);
}

VertexId
NetworkNameAdapter::vertexId(const Pin *pin) const
{
  return network_->vertexId(pin);
}

void
NetworkNameAdapter::setVertexId(Pin *pin,
				VertexId id)
{
  network_->setVertexId(pin, id);
}

bool
NetworkNameAdapter::isBundle(const Port *port) const
{
  return network_->isBundle(port);
}

bool
NetworkNameAdapter::isBus(const Port *port) const
{
  return network_->isBus(port);
}

const char *
NetworkNameAdapter::busName(const Port *port) const
{
  return network_->busName(port);
}

Port *
NetworkNameAdapter::findBusBit(const Port *port,
			       int index) const
{
  return network_->findMember(port, index);
}

int
NetworkNameAdapter::size(const Port *port) const
{
  return network_->size(port);
}

int
NetworkNameAdapter::fromIndex(const Port *port) const
{
  return network_->fromIndex(port);
}

int
NetworkNameAdapter::toIndex(const Port *port) const
{
  return network_->toIndex(port);
}

bool
NetworkNameAdapter::hasMembers(const Port *port) const
{
  return network_->hasMembers(port);
}

Port *
NetworkNameAdapter::findMember(const Port *port,
			       int index) const
{
  return network_->findMember(port, index);
}

PortMemberIterator *
NetworkNameAdapter::memberIterator(const Port *port) const
{
  return network_->memberIterator(port);
}

////////////////////////////////////////////////////////////////

Cell *
NetworkNameAdapter::cell(const Instance *instance) const
{
  return network_->cell(instance);
}

Instance *
NetworkNameAdapter::parent(const Instance *instance) const
{
  return network_->parent(instance);
}

bool
NetworkNameAdapter::isLeaf(const Instance *instance) const
{
  return network_->isLeaf(instance);
}

Pin *
NetworkNameAdapter::findPin(const Instance *instance,
			    const Port *port) const
{
  return network_->findPin(instance, port);
}

Pin *
NetworkNameAdapter::findPin(const Instance *instance,
			    const LibertyPort *port) const
{
  return network_->findPin(instance, port);
}

InstanceChildIterator *
NetworkNameAdapter::childIterator(const Instance *instance) const
{
  return network_->childIterator(instance);
}

InstancePinIterator *
NetworkNameAdapter::pinIterator(const Instance *instance) const
{
  return network_->pinIterator(instance);
}

InstanceNetIterator *
NetworkNameAdapter::netIterator(const Instance *instance) const
{
  return network_->netIterator(instance);
}

Port *
NetworkNameAdapter::port(const Pin *pin) const
{
  return network_->port(pin);
}

Instance *
NetworkNameAdapter::instance(const Pin *pin) const
{
  return network_->instance(pin);
}

Net *
NetworkNameAdapter::net(const Pin *pin) const
{
  return network_->net(pin);
}

Term *
NetworkNameAdapter::term(const Pin *pin) const
{
  return network_->term(pin);
}

PortDirection *
NetworkNameAdapter::direction(const Pin *pin) const
{
  return network_->direction(pin);
}

Net *
NetworkNameAdapter::net(const Term *term) const
{
  return network_->net(term);
}

Pin *
NetworkNameAdapter::pin(const Term *term) const
{
  return network_->pin(term);
}

Instance *
NetworkNameAdapter::instance(const Net *net) const
{
  return network_->instance(net);
}

NetPinIterator *
NetworkNameAdapter::pinIterator(const Net *net) const
{
  return network_->pinIterator(net);
}

NetTermIterator *
NetworkNameAdapter::termIterator(const Net *net) const
{
  return network_->termIterator(net);
}

bool
NetworkNameAdapter::isPower(const Net *net) const
{
  return network_->isPower(net);
}

bool
NetworkNameAdapter::isGround(const Net *net) const
{
  return network_->isGround(net);
}

ConstantPinIterator *
NetworkNameAdapter::constantPinIterator()
{
  return network_->constantPinIterator();
}

char
NetworkNameAdapter::pathDivider() const
{
  return network_->pathDivider();
}

void
NetworkNameAdapter::setPathDivider(char divider)
{
  network_->setPathDivider(divider);
}

char
NetworkNameAdapter::pathEscape() const
{
  return network_->pathEscape();
}

void
NetworkNameAdapter::setPathEscape(char escape)
{
  network_->setPathEscape(escape);
}

bool
NetworkNameAdapter::isEditable() const
{
  return network_->isEditable();
}


LibertyLibrary *
NetworkNameAdapter::makeLibertyLibrary(const char *name,
				       const char *filename)
{
  return network_edit_->makeLibertyLibrary(name, filename);
}

Instance *
NetworkNameAdapter::makeInstance(LibertyCell *cell,
				 const char *name,
				 Instance *parent)
{
  return network_edit_->makeInstance(cell, name, parent);
}

void
NetworkNameAdapter::makePins(Instance *inst)
{
  network_edit_->makePins(inst);
}

void
NetworkNameAdapter::mergeInto(Net *net,
			      Net *into_net)
{
  network_edit_->mergeInto(net, into_net);
}

Net *
NetworkNameAdapter::mergedInto(Net *net)
{
  return network_edit_->mergedInto(net);
}

Net *
NetworkNameAdapter::makeNet(const char *name,
			    Instance *parent)
{
  return network_edit_->makeNet(name, parent);
}

void
NetworkNameAdapter::replaceCell(Instance *inst,
				Cell *to_cell)
{
  network_edit_->replaceCell(inst, to_cell);
}

Pin *
NetworkNameAdapter::connect(Instance *inst,
			    Port *port,
			    Net *net)
{
  return network_edit_->connect(inst, port, net);
}

Pin *
NetworkNameAdapter::connect(Instance *inst,
			    LibertyPort *port,
			    Net *net)
{
  return network_edit_->connect(inst, port, net);
}

void
NetworkNameAdapter::disconnectPin(Pin *pin)
{
  network_edit_->disconnectPin(pin);
}

void
NetworkNameAdapter::deleteNet(Net *net)
{
  network_edit_->deleteNet(net);
}

void
NetworkNameAdapter::deletePin(Pin *pin)
{
  network_edit_->deletePin(pin);
}

void
NetworkNameAdapter::deleteInstance(Instance *inst)
{
  network_edit_->deleteInstance(inst);
}

////////////////////////////////////////////////////////////////

Network *
makeSdcNetwork(Network *network)
{
  return new SdcNetwork(network);
}

SdcNetwork::SdcNetwork(Network *network) :
  NetworkNameAdapter(network)
{
}

// Translate sta namespace to sdc namespace.
// Remove all escapes.
const char *
SdcNetwork::staToSdc(const char *sta_name) const
{
  char escape = pathEscape();
  char *sdc_name = makeTmpString(strlen(sta_name) + 1);
  char *d = sdc_name;
  for (const char *s = sta_name; *s; s++) {
    char ch = s[0];
    if (ch == escape) {
      char next_ch = s[1];
      // Escaped escape.
      if (next_ch == escape) {
	*d++ = ch;
	*d++ = next_ch;
	s++;
      }
    }
    else
      // Non escape.
      *d++ = ch;
  }
  *d++ = '\0';
  return sdc_name;
}

Port *
SdcNetwork::findPort(const Cell *cell,
		     const char *name) const
{
  Port *port = network_->findPort(cell, name);
  if (port == nullptr) {
    // Look for matches after escaping brackets.
    char *bus_name;
    int index;
    parseBusName(name, '[', ']', pathEscape(), bus_name, index);
    if (bus_name) {
      const char *escaped1 = escapeBrackets(name, this);
      port = network_->findPort(cell, escaped1);
      if (port == nullptr
	  && bus_name[strlen(bus_name) - 1] == ']') {
	// Try escaping base foo\[0\][1]
	const char *escaped2 = stringPrintTmp("%s[%d]",
					      escapeBrackets(bus_name, this),
					      index);
	port = network_->findPort(cell, escaped2);
      }
      stringDelete(bus_name);
    }
  }
  return port;
}

void
SdcNetwork::findPortsMatching(const Cell *cell,
			      const PatternMatch *pattern,
			      PortSeq *ports) const
{
  network_->findPortsMatching(cell, pattern, ports);
  if (ports->empty()) {
    // Look for matches after escaping brackets.
    char *bus_name;
    int index;
    parseBusName(pattern->pattern(), '[', ']', pathEscape(), bus_name, index);
    if (bus_name) {
      const char *escaped1 = escapeBrackets(pattern->pattern(), this);
      PatternMatch escaped_pattern1(escaped1, pattern);
      network_->findPortsMatching(cell, &escaped_pattern1, ports);
      if (ports->empty()
	  && bus_name[strlen(bus_name) - 1] == ']') {
	// Try escaping base foo\[0\][1]
	const char *escaped2 = stringPrintTmp("%s[%d]",
					      escapeBrackets(bus_name, this),
					      index);
	PatternMatch escaped_pattern2(escaped2, pattern);
	network_->findPortsMatching(cell, &escaped_pattern2, ports);
      }
      stringDelete(bus_name);
    }
  }
}

const char *
SdcNetwork::name(const Port *port) const
{
  return staToSdc(network_->name(port));
}

const char *
SdcNetwork::busName(const Port *port) const
{
  return staToSdc(network_->busName(port));
}

const char *
SdcNetwork::name(const Instance *instance) const
{
  return staToSdc(network_->name(instance));
}

const char *
SdcNetwork::pathName(const Instance *instance) const
{
  return staToSdc(network_->pathName(instance));
}

const char *
SdcNetwork::pathName(const Pin *pin) const
{
  return staToSdc(network_->pathName(pin));
}

const char *
SdcNetwork::portName(const Pin *pin) const
{
  return staToSdc(network_->portName(pin));
}

const char *
SdcNetwork::name(const Net *net) const
{
  return staToSdc(network_->name(net));
}

const char *
SdcNetwork::pathName(const Net *net) const
{
  return staToSdc(network_->pathName(net));
}

////////////////////////////////////////////////////////////////

Instance *
SdcNetwork::findInstance(const char *path_name) const
{
  const char *child_name;
  Instance *parent;
  parsePath(path_name, parent, child_name);
  if (parent == nullptr)
    parent = network_->topInstance();
  Instance *child = findChild(parent, child_name);
  if (child == nullptr)
    child = findChild(parent, escapeDividers(child_name, this));
  return child;
}

void
SdcNetwork::findInstancesMatching(const Instance *context,
				  const PatternMatch *pattern,
				  InstanceSeq *insts) const
{
  visitMatches(context, pattern,
	       [&](const Instance *instance,
		   const PatternMatch *tail)
	       {
		 size_t match_count = insts->size();
		 network_->findChildrenMatching(instance, tail, insts);
		 return insts->size() != match_count;
	       });
}

Instance *
SdcNetwork::findChild(const Instance *parent,
		      const char *name) const
{
  Instance *child = network_->findChild(parent, name);
  if (child == nullptr) {
    const char *escaped = escapeBrackets(name, this);
    child = network_->findChild(parent, escaped);
  }
  return child;
}

////////////////////////////////////////////////////////////////

Net *
SdcNetwork::findNet(const char *path_name) const
{
  const char *net_name;
  Instance *inst;
  parsePath(path_name, inst, net_name);
  if (inst == nullptr)
    inst = network_->topInstance();
  return findNet(inst, net_name);
}

Net *
SdcNetwork::findNet(const Instance *instance,
		    const char *net_name) const
{
  Net *net = network_->findNet(instance, net_name);
  if (net == nullptr) {
    const char *net_name_ = escapeBrackets(net_name, this);
    net = network_->findNet(instance, net_name_);
  }
  return net;
}

void
SdcNetwork::findNetsMatching(const Instance *parent,
			     const PatternMatch *pattern,
			     NetSeq *nets) const
{
  visitMatches(parent, pattern,
	       [&](const Instance *instance,
		   const PatternMatch *tail)
	       {
		 size_t match_count = nets->size();
		 network_->findInstNetsMatching(instance, tail, nets);
		 return nets->size() != match_count;
	       });
}

void
SdcNetwork::findInstNetsMatching(const Instance *instance,
				 const PatternMatch *pattern,
				 NetSeq *nets) const
{
  network_->findInstNetsMatching(instance, pattern, nets);
  if (nets->empty()) {
    // Look for matches after escaping path dividers.
    const PatternMatch escaped_dividers(escapeDividers(pattern->pattern(),
						       this),
					pattern);
    network_->findInstNetsMatching(instance, &escaped_dividers, nets);
    if (nets->empty()) {
      // Look for matches after escaping brackets.
      const PatternMatch escaped_brkts(escapeBrackets(pattern->pattern(),this),
				       pattern);
      network_->findInstNetsMatching(instance, &escaped_brkts, nets);
    }
  }
}

////////////////////////////////////////////////////////////////

Pin *
SdcNetwork::findPin(const char *path_name) const
{
  const char *port_name;
  Instance *inst;
  parsePath(path_name, inst, port_name);
  if (inst == nullptr)
    inst = network_->topInstance();
  return findPin(inst, port_name);
}

Pin *
SdcNetwork::findPin(const Instance *instance,
		    const char *port_name) const
{
  Pin *pin = network_->findPin(instance, port_name);
  if (pin == nullptr) {
    // Look for match after escaping brackets.
    char *bus_name;
    int index;
    parseBusName(port_name, '[', ']', pathEscape(), bus_name, index);
    if (bus_name) {
      const char *escaped1 = escapeBrackets(port_name, this);
      pin = network_->findPin(instance, escaped1);
      if (pin == nullptr
	  && bus_name[strlen(bus_name) - 1] == ']') {
	// Try escaping base foo\[0\][1]
	const char *escaped2 = stringPrintTmp("%s[%d]",
					      escapeBrackets(bus_name, this),
					      index);
	pin = network_->findPin(instance, escaped2);
      }
      stringDelete(bus_name);
    }
  }
  return pin;
}

// Top level ports are not considered pins by get_pins.
void
SdcNetwork::findPinsMatching(const Instance *instance,
			     const PatternMatch *pattern,
			     PinSeq *pins) const
{
  if (stringEq(pattern->pattern(), "*")) {
    // Pattern of '*' matches all child instance pins.
    InstanceChildIterator *child_iter = childIterator(instance);
    while (child_iter->hasNext()) {
      Instance *child = child_iter->next();
      InstancePinIterator *pin_iter = pinIterator(child);
      while (pin_iter->hasNext()) {
	Pin *pin = pin_iter->next();
	pins->push_back(pin);
      }
      delete pin_iter;
    }
    delete child_iter;
  }
  else
    visitMatches(instance, pattern,
		 [&](const Instance *instance,
		     const PatternMatch *tail)
		 {
		   return visitPinTail(instance, tail, pins);
		 });
}

bool
SdcNetwork::visitPinTail(const Instance *instance,
			 const PatternMatch *tail,
			 PinSeq *pins) const
{
  bool found_match = false;
  if (instance != network_->topInstance()) {
    Cell *cell = network_->cell(instance);
    CellPortIterator *port_iter = network_->portIterator(cell);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      const char *port_name = network_->name(port);
      if (network_->hasMembers(port)) {
	bool bus_matches = tail->match(port_name)
	  || tail->match(escapeDividers(port_name, network_));
	PortMemberIterator *member_iter = network_->memberIterator(port);
	while (member_iter->hasNext()) {
	  Port *member_port = member_iter->next();
	  Pin *pin = network_->findPin(instance, member_port);
	  if (pin) {
	    if (bus_matches) {
	      pins->push_back(pin);
	      found_match = true;
	    }
	    else {
	      const char *member_name = network_->name(member_port);
	      if (tail->match(member_name)
		  || tail->match(escapeDividers(member_name, network_))) {
		pins->push_back(pin);
		found_match = true;
	      }
	    }
	  }
	}
	delete member_iter;
      }
      else if (tail->match(port_name)
	       || tail->match(escapeDividers(port_name, network_))) {
	Pin *pin = network_->findPin(instance, port);
	if (pin) {
	  pins->push_back(pin);
	  found_match = true;
	}
      }
    }
    delete port_iter;
  }
  return found_match;
}

Instance *
SdcNetwork::makeInstance(LibertyCell *cell,
			 const char *name,
			 Instance *parent)
{
  const char *escaped_name = escapeDividers(name, this);
  return network_edit_->makeInstance(cell, escaped_name, parent);
}

Net *
SdcNetwork::makeNet(const char *name,
		    Instance *parent)
{
  const char *escaped_name = escapeDividers(name, this);
  return network_edit_->makeNet(escaped_name, parent);
}

////////////////////////////////////////////////////////////////

// Helper to parse an instance path (with optional net/port tail).
// Since dividers are not escaped in SDC, look for an instance for
// each sub-section of the path.  If none is found, escape the divider
// and keep looking.  For the path a/b/c this looks for instances
//  a
//  a\/b
//  a\/b\/c
void
SdcNetwork::parsePath(const char *path,
		      // Return values.
		      Instance *&inst,
		      const char *&path_tail) const
{
  int divider_count, path_length;
  scanPath(path, divider_count, path_length);
  if (divider_count > 0)
    parsePath(path, divider_count, path_length, inst, path_tail);
  else {
    inst = nullptr;
    path_tail = path;
  }
}

// Scan the path for unescaped dividers.
void
SdcNetwork::scanPath(const char *path,
		     // Return values.
		     // Unescaped divider count.
		     int &divider_count,
		     int &path_length) const
{
  divider_count = 0;
  path_length = 0;
  for (const char *s = path; *s; s++) {
    char ch = *s;
    if (ch == escape_) {
      // Make sure we don't skip the null if escape is the last char.
      if (s[1] != '\0') {
	s++;
	path_length++;
      }
    }
    else if (ch == divider_)
      divider_count++;
    path_length++;
  }
}

void
SdcNetwork::parsePath(const char *path,
		      int divider_count,
		      int path_length,
		      // Return values.
		      Instance *&inst,
		      const char *&path_tail) const
{
  Instance *parent = topInstance();
  // Leave room to escape all the dividers and '\0'.
  int inst_path_length = path_length + divider_count + 1;
  char *inst_path = new char[inst_path_length];
  inst = nullptr;
  path_tail = path;
  char *p = inst_path;
  for (const char *s = path; *s; s++) {
    char ch = *s;
    if (ch == escape_) {
      // Make sure we don't skip the null if escape is the last char.
      if (s[1] != '\0') {
	*p++ = ch;
	*p++ = s[1];
	s++;
      }
    }
    else if (ch == divider_) {
      // Terminate the sub-path up to this divider.
      *p = '\0';
      Instance *child = findChild(parent, inst_path);
      if (child) {
	// Found an instance for the sub-path up to this divider.
	parent = inst = child;
	// Reset the instance path.
	p = inst_path;
	path_tail = s + 1;
      }
      else {
	// No match for sub-path.  Escape the divider and keep looking.
	*p++ = escape_;
	*p++ = divider_;
      }
    }
    else
      *p++ = ch;
    if (p - inst_path + 1 > inst_path_length)
      internalError("inst path string lenth estimate busted");
  }
  *p = '\0';
  stringDelete(inst_path);
}

// Helper to visit instance path matches.
// Since dividers are not escaped in SDC, look for instance matches for
// each sub-section of the path.  If none are found, escape the divider
// and keep looking.  For the path a/b/c this looks for instances
//  a
//  a\/b
//  a\/b\/c
bool
SdcNetwork::visitMatches(const Instance *parent,
			 const PatternMatch *pattern,
			 const std::function<bool (const Instance *instance,
						   const PatternMatch *tail)>
			 visit_tail) const
{
  int divider_count, path_length;
  scanPath(pattern->pattern(), divider_count, path_length);

  // Leave room to escape all the dividers and '\0'.
  int inst_path_length = path_length + divider_count + 1;
  char *inst_path = new char[inst_path_length];
  char *p = inst_path;
  bool has_brkts = false;
  bool found_match = false;
  for (const char *s = pattern->pattern(); *s; s++) {
    char ch = *s;
    if (ch == escape_) {
      // Make sure we don't skip the null if escape is the last char.
      if (s[1] != '\0') {
	*p++ = ch;
	*p++ = s[1];
	s++;
      }
    }
    else if (ch == divider_) {
      // Terminate the sub-path up to this divider.
      *p = '\0';
      PatternMatch matcher(inst_path, pattern);
      InstanceSeq matches;
      findChildrenMatching(parent, &matcher, &matches);
      if (has_brkts && matches.empty()) {
	// Look for matches after escaping brackets.
	const PatternMatch escaped_brkts(escapeBrackets(inst_path, this),
					 pattern); 
	network_->findChildrenMatching(parent, &escaped_brkts, &matches);
      }
      if (!matches.empty()) {
	// Found instance matches for the sub-path up to this divider.
	const PatternMatch tail_pattern(s + 1, pattern);
	InstanceSeq::Iterator match_iter(matches);
	while (match_iter.hasNext()) {
	  Instance *match = match_iter.next();
	  // Recurse to save the iterator state so we can iterate over
	  // multiple nested partial matches.
	  found_match |= visitMatches(match, &tail_pattern, visit_tail);
	}
      }
      // Escape the divider and keep looking.
      *p++ = escape_;
      *p++ = divider_;
    }
    else {
      if (ch == '[' || ch == ']')
	has_brkts = true;
      *p++ = ch;
    }
    if (p - inst_path + 1 > inst_path_length)
      internalError("inst path string lenth estimate busted");
  }
  *p = '\0';
  if (!found_match) {
    PatternMatch tail_pattern(inst_path, pattern);
    found_match |= visit_tail(parent, &tail_pattern);
    if (!found_match && has_brkts) {
      // Look for matches after escaping brackets.
      char *escaped_path = stringCopy(escapeBrackets(inst_path, this));
      const PatternMatch escaped_tail(escaped_path, pattern);
      found_match |= visit_tail(parent, &escaped_tail);
      stringDelete(escaped_path);
    }
  }
  stringDelete(inst_path);
  return found_match;
}

////////////////////////////////////////////////////////////////

static const char *
escapeDividers(const char *token,
	       const Network *network)
{
  return escapeChars(token, network->pathDivider(), '\0',
		     network->pathEscape());
}

static const char *
escapeBrackets(const char *token,
	       const Network *network)
{
  return escapeChars(token, '[', ']', network->pathEscape());
}

} // namespace
