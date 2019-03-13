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

////////////////////////////////////////////////////////////////

// Helper to parse an instance path (with optional net/port tail).
// Since dividers are not escaped in SDC, look for an instance for
// each sub-section of the path.  If none is found, escape the divider
// and keep looking.  For the path a/b/c this looks for instances
//  a
//  a\/b
//  a\/b\/c
class SdcPathParser
{
public:
  SdcPathParser(const char *path,
		const Network *network);
  ~SdcPathParser();
  Instance *instance() const { return inst_; }
  const char *pathTail() const { return path_tail_; }

protected:
  void initialScan(const char *path);
  void parsePath(const char *path);

  int path_length_;
  const Network *network_;
  char divider_;
  char escape_;
  // Unescaped divider count.
  int divider_count_;
  char *inst_path_;
  Instance *inst_;
  const char *path_tail_;

private:
  DISALLOW_COPY_AND_ASSIGN(SdcPathParser);
};

SdcPathParser::SdcPathParser(const char *path,
			     const Network *network) :
  network_(network),
  divider_(network->pathDivider()),
  escape_(network->pathEscape()),
  inst_path_(nullptr),
  inst_(nullptr)
{
  initialScan(path);
  if (divider_count_ > 0)
    parsePath(path);
  else
    path_tail_ = path;
}

SdcPathParser::~SdcPathParser()
{
  stringDelete(inst_path_);
}

// Scan the path for unescaped dividers.
void
SdcPathParser::initialScan(const char *path)
{
  divider_count_ = 0;
  path_length_ = 0;
  for (const char *s = path; *s; s++) {
    char ch = *s;
    if (ch == escape_) {
      // Make sure we don't skip the null if escape is the last char.
      if (s[1] != '\0') {
	s++;
	path_length_++;
      }
    }
    else if (ch == divider_)
      divider_count_++;
    path_length_++;
  }
}

void
SdcPathParser::parsePath(const char *path)
{
  Instance *parent = network_->topInstance();
  // Leave room to escape all the dividers and '\0'.
  int inst_path_length = path_length_ + divider_count_ + 1;
  inst_path_ = new char[inst_path_length];
  path_tail_ = inst_path_;
  char *p = inst_path_;
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
      Instance *child = network_->findChild(parent, inst_path_);
      if (child) {
	// Found an instance for the sub-path up to this divider.
	parent = inst_ = child;
	// Reset the instance path.
	path_tail_ = p = inst_path_;
      }
      else {
	// No match for sub-path.  Escape the divider and keep looking.
	*p++ = escape_;
	*p++ = divider_;
      }
    }
    else
      *p++ = ch;
    if (p - inst_path_ + 1 > inst_path_length)
      internalError("inst path string lenth estimate busted");
  }
  *p = '\0';
}

////////////////////////////////////////////////////////////////

// Helper to visit an instance path matches.
// Since dividers are not escaped in SDC, look for instance matches for
// each sub-section of the path.  If none are found, escape the divider
// and keep looking.  For the path a/b/c this looks for instances
// This base class is specialized by defining visitTail.
//  a
//  a\/b
//  a\/b\/c
class SdcPathMatcher
{
public:
  SdcPathMatcher(const Network *network);
  void findMatches(const Instance *parent,
		   const PatternMatch *pattern);
  virtual bool visitTail(const Instance *instance,
			 const PatternMatch *tail) = 0;

protected:
  void initialScan(const PatternMatch *pattern);
  bool visitMatches(const Instance *parent,
		    const PatternMatch *tail);

  int path_length_;
  const Network *network_;
  char divider_;
  char escape_;
  int divider_count_;

private:
  DISALLOW_COPY_AND_ASSIGN(SdcPathMatcher);
};

SdcPathMatcher::SdcPathMatcher(const Network *network) :
  network_(network),
  divider_(network->pathDivider()),
  escape_(network->pathEscape())
{
}

void
SdcPathMatcher::findMatches(const Instance *parent,
			    const PatternMatch *pattern)
{
  initialScan(pattern);
  visitMatches(parent, pattern);
}

// Scan the path for unescaped dividers.
void
SdcPathMatcher::initialScan(const PatternMatch *pattern)
{
  divider_count_ = 0;
  path_length_ = 0;
  for (const char *s = pattern->pattern(); *s; s++) {
    char ch = *s;
    if (ch == escape_) {
      // Make sure we don't skip the null if escape is the last char.
      if (s[1] != '\0') {
	s++;
	path_length_++;
      }
    }
    else if (ch == divider_)
      divider_count_++;
    path_length_++;
  }
}

bool
SdcPathMatcher::visitMatches(const Instance *parent,
			     const PatternMatch *tail)
{
  // Leave room to escape all the dividers and '\0'.
  int inst_path_length = path_length_ + divider_count_ + 1;
  char *inst_path = new char[inst_path_length];
  char *p = inst_path;
  bool has_brkts = false;
  bool found_match = false;
  for (const char *s = tail->pattern(); *s; s++) {
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
      PatternMatch matcher(inst_path, tail);
      InstanceSeq matches;
      network_->findChildrenMatching(parent, &matcher, &matches);
      if (has_brkts && matches.empty()) {
	// Look for matches after escaping brackets.
	const PatternMatch escaped_brkts(escapeBrackets(inst_path, network_),
					 tail); 
	network_->findChildrenMatching(parent, &escaped_brkts, &matches);
      }
      if (!matches.empty()) {
	// Found instance matches for the sub-path up to this divider.
	const PatternMatch tail_pattern(s + 1, tail);
	InstanceSeq::Iterator match_iter(matches);
	while (match_iter.hasNext()) {
	  Instance *match = match_iter.next();
	  // Recurse to save the iterator state so we can iterate over
	  // multiple nested partial matches.
	  found_match |= visitMatches(match, &tail_pattern);
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
    PatternMatch tail_pattern(inst_path, tail);
    found_match |= visitTail(parent, &tail_pattern);
    if (!found_match && has_brkts) {
      // Look for matches after escaping brackets.
      char *escaped_path = stringCopy(escapeBrackets(inst_path, network_));
      const PatternMatch escaped_tail(escaped_path, tail);
      found_match |= visitTail(parent, &escaped_tail);
      stringDelete(escaped_path);
    }
  }
  stringDelete(inst_path);
  return found_match;
}

////////////////////////////////////////////////////////////////

class SdcInstanceMatcher : public SdcPathMatcher
{
public:
  SdcInstanceMatcher(const Network *network,
		     InstanceSeq *insts);
  virtual bool visitTail(const Instance *instance,
			 const PatternMatch *tail);

protected:
  InstanceSeq *insts_;

private:
  DISALLOW_COPY_AND_ASSIGN(SdcInstanceMatcher);
};

SdcInstanceMatcher::SdcInstanceMatcher(const Network *network,
				       InstanceSeq *insts) :
  SdcPathMatcher(network),
  insts_(insts)
{
}

bool
SdcInstanceMatcher::visitTail(const Instance *instance,
			      const PatternMatch *tail)
{
  size_t match_count = insts_->size();
  network_->findChildrenMatching(instance, tail, insts_);
  return insts_->size() != match_count;
}

void
SdcNetwork::findInstancesMatching(const Instance *context,
				  const PatternMatch *pattern,
				  InstanceSeq *insts) const
{
  SdcInstanceMatcher matcher(network_, insts);
  matcher.findMatches(context, pattern);
}

////////////////////////////////////////////////////////////////

class SdcNetMatcher : public SdcPathMatcher
{
public:
  SdcNetMatcher(const Network *network,
		NetSeq *nets);
  virtual bool visitTail(const Instance *instance,
			 const PatternMatch *tail);

protected:
  DISALLOW_COPY_AND_ASSIGN(SdcNetMatcher);

  NetSeq *nets_;
};

SdcNetMatcher::SdcNetMatcher(const Network *network,
			     NetSeq *nets) :
  SdcPathMatcher(network),
  nets_(nets)
{
}

bool
SdcNetMatcher::visitTail(const Instance *instance,
			 const PatternMatch *tail)
{
  size_t match_count = nets_->size();
  network_->findInstNetsMatching(instance, tail, nets_);
  return nets_->size() != match_count;
}

void
SdcNetwork::findNetsMatching(const Instance *parent,
			     const PatternMatch *pattern,
			     NetSeq *nets) const
{
  SdcNetMatcher matcher(this, nets);
  matcher.findMatches(parent, pattern);
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

class SdcPinMatcher : public SdcPathMatcher
{
public:
  SdcPinMatcher(const Network *network,
		PinSeq *pins);
  virtual bool visitTail(const Instance *instance,
			 const PatternMatch *tail);

protected:
  DISALLOW_COPY_AND_ASSIGN(SdcPinMatcher);

  PinSeq *pins_;
};

SdcPinMatcher::SdcPinMatcher(const Network *network,
			     PinSeq *pins) :
  SdcPathMatcher(network),
  pins_(pins)
{
}

bool
SdcPinMatcher::visitTail(const Instance *instance,
			 const PatternMatch *tail)
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
	      pins_->push_back(pin);
	      found_match = true;
	    }
	    else {
	      const char *member_name = network_->name(member_port);
	      if (tail->match(member_name)
		  || tail->match(escapeDividers(member_name, network_))) {
		pins_->push_back(pin);
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
	  pins_->push_back(pin);
	  found_match = true;
	}
      }
    }
    delete port_iter;
  }
  return found_match;
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
  else {
    SdcPinMatcher matcher(network_, pins);
    matcher.findMatches(instance, pattern);
  }
}

////////////////////////////////////////////////////////////////

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
NetworkNameAdapter::libertyPort(Port *port) const
{
  return network_->libertyPort(port);
}

PortDirection *
NetworkNameAdapter::direction(const Port *port) const
{
  return network_->direction(port);
}

VertexIndex
NetworkNameAdapter::vertexIndex(const Pin *pin) const
{
  return network_->vertexIndex(pin);
}

void
NetworkNameAdapter::setVertexIndex(Pin *pin,
				   VertexIndex index)
{
  network_->setVertexIndex(pin, index);
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
NetworkNameAdapter::swapCell(Instance *inst,
			     LibertyCell *cell)
{
  network_edit_->swapCell(inst, cell);
}

Pin *
NetworkNameAdapter::connect(Instance *inst,
			    Port *port,
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
    const char *port_name_ = escapeBrackets(name, this);
    port = network_->findPort(cell, port_name_);
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
    PatternMatch escaped(escapeBrackets(pattern->pattern(), this), pattern);
    network_->findPortsMatching(cell, &escaped, ports);
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

bool
SdcNetwork::hasMembers(const Port *port) const
{
  return network_->hasMembers(port);
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

Instance *
SdcNetwork::findInstance(const char *path_name) const
{
  SdcPathParser path_parser(path_name, this);
  Instance *parent = path_parser.instance();
  if (parent == nullptr)
    parent = network_->topInstance();
  const char *child_name = path_parser.pathTail();
  return findChild(parent, child_name);
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

Net *
SdcNetwork::findNet(const char *path_name) const
{
  SdcPathParser path_parser(path_name, this);
  const Instance *inst = path_parser.instance();
  if (inst == nullptr)
    inst = network_->topInstance();
  const char *net_name = path_parser.pathTail();
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

////////////////////////////////////////////////////////////////

Pin *
SdcNetwork::findPin(const char *path_name) const
{
  SdcPathParser path_parser(path_name, this);
  const Instance *inst = path_parser.instance();
  if (inst == nullptr)
    inst = network_->topInstance();
  const char *port_name = path_parser.pathTail();
  return findPin(inst, port_name);
}

Pin *
SdcNetwork::findPin(const Instance *instance,
		    const char *port_name) const
{
  Pin *pin = network_->findPin(instance, port_name);
  if (pin == nullptr) {
    // Look for match after escaping brackets.
    const char *port_name_ = escapeBrackets(port_name, this);
    pin = network_->findPin(instance, port_name_);
  }
  return pin;
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
