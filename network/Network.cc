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
#include "StringUtil.hh"
#include "PatternMatch.hh"
#include "PortDirection.hh"
#include "Liberty.hh"
#include "Network.hh"

namespace sta {

Network::Network() :
  default_liberty_(nullptr),
  divider_('/'),
  escape_('\\')
{
}

Network::~Network()
{
  net_drvr_pin_map_.deleteContents();
}

void
Network::clear()
{
  default_liberty_ = nullptr;
  clearNetDrvrPinMap();
}

bool
Network::isLinked() const
{
  return topInstance() != nullptr;
}

LibertyLibrary *
Network::libertyLibrary(const Cell *cell) const
{
  return libertyCell(cell)->libertyLibrary();
}

LibertyLibrary *
Network::libertyLibrary(const Instance *instance) const
{
  return libertyCell(instance)->libertyLibrary();
}

void
Network::readLibertyAfter(LibertyLibrary *)
{
}

LibertyCell *
Network::findLibertyCell(const char *name) const
{
  LibertyLibraryIterator *iter = libertyLibraryIterator();
  while (iter->hasNext()) {
    LibertyLibrary *lib = iter->next();
    LibertyCell *cell = lib->findLibertyCell(name);
    if (cell) {
      delete iter;
      return cell;
    }
  }
  delete iter;
  return nullptr;
}

LibertyLibrary *
Network::defaultLibertyLibrary() const
{
  return default_liberty_;
}

void
Network::setDefaultLibertyLibrary(LibertyLibrary *library)
{
  default_liberty_ = library;
}

// Only used by Sta::setMinLibrary so linear search is acceptable.
LibertyLibrary *
Network::findLibertyFilename(const char *filename)
{
  LibertyLibraryIterator *lib_iter = libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    if (stringEq(lib->filename(), filename)) {
      delete lib_iter;
      return lib;
    }
  }
  delete lib_iter;
  return nullptr;
}

LibertyCell *
Network::libertyCell(const Instance *instance) const
{
  return libertyCell(cell(instance));
}

LibertyPort *
Network::libertyPort(const Pin *pin) const
{
  return libertyPort(port(pin));
}

bool
Network::busIndexInRange(const Port *port,
			 int index)
{
  int from_index = fromIndex(port);
  int to_index = toIndex(port);
  return (from_index <= to_index
	  && index <= to_index
	  && index >= from_index)
    || (from_index > to_index
	&& index >= to_index
	&& index <= from_index);
}

bool
Network::hasMembers(const Port *port) const
{
  return isBus(port) || isBundle(port);
}

const char *
Network::pathName(const Instance *instance) const
{
  ConstInstanceSeq inst_path;
  path(instance, inst_path);
  size_t name_length = 0;
  ConstInstanceSeq::Iterator path_iter1(inst_path);
  while (path_iter1.hasNext()) {
    const Instance *inst = path_iter1.next();
    name_length += strlen(name(inst)) + 1;
  }
  char *path_name = makeTmpString(name_length);
  char *path_ptr = path_name;
  // Top instance has null string name, so terminate the string here.
  *path_name = '\0';
  while (inst_path.size()) {
    const Instance *inst = inst_path.back();
    const char *inst_name = name(inst);
    strcpy(path_ptr, inst_name);
    path_ptr += strlen(inst_name);
    inst_path.pop_back();
    if (inst_path.size())
      *path_ptr++ = pathDivider();
    *path_ptr = '\0';
  }
  return path_name;
}

bool
Network::pathNameLess(const Instance *inst1,
		      const Instance *inst2) const
{
  return pathNameCmp(inst1, inst2) < 0;
}

int
Network::pathNameCmp(const Instance *inst1,
		     const Instance *inst2) const
{
  if (inst1 == nullptr && inst2)
    return -1;
  else if (inst1 && inst2 == nullptr)
    return 1;
  else if (inst1 == inst2)
    return 0;
  else {
    ConstInstanceSeq path1;
    ConstInstanceSeq path2;
    path(inst1, path1);
    path(inst2, path2);
    while (!path1.empty() && !path2.empty()) {
      const Instance *inst1 = path1.back();
      const Instance *inst2 = path2.back();
      int cmp = strcmp(name(inst1), name(inst2));
      if (cmp != 0)
	return cmp;
      path1.pop_back();
      path2.pop_back();
    }
    if (path1.empty() && !path2.empty())
      return -1;
    else if (path1.empty() && path2.empty())
      return 0;
    else
      return 1;
  }
}

void
Network::path(const Instance *inst,
	      // Return value.
	      ConstInstanceSeq &path) const
{
  while (!isTopInstance(inst)) {
    path.push_back(inst);
    inst = parent(inst);
  }
}

bool
Network::isTopInstance(const Instance *inst) const
{
  return inst == topInstance();
}

bool
Network::isInside(const Instance *inst,
		  const Instance *hier_inst) const
{
  while (inst) {
    if (inst == hier_inst)
      return true;
    inst = parent(inst);
  }
  return false;
}

bool
Network::isHierarchical(const Instance *instance) const
{
  return !isLeaf(instance);
}

////////////////////////////////////////////////////////////////

const char *
Network::name(const Pin *pin) const
{
  return pathName(pin);
}

const char *
Network::portName(const Pin *pin) const
{
  return name(port(pin));
}

const char *
Network::pathName(const Pin *pin) const
{
  const Instance *inst = instance(pin);
  if (inst && inst != topInstance()) {
    const char *inst_name = pathName(inst);
    size_t inst_name_length = strlen(inst_name);
    const char *port_name = portName(pin);
    size_t port_name_length = strlen(port_name);
    size_t path_name_length = inst_name_length + port_name_length + 2;
    char *path_name = makeTmpString(path_name_length);
    char *path_ptr = path_name;
    strcpy(path_ptr, inst_name);
    path_ptr += inst_name_length;
    *path_ptr++ = pathDivider();
    strcpy(path_ptr, port_name);
    return path_name;
  }
  else
    return portName(pin);
}

bool
Network::pathNameLess(const Pin *pin1,
		      const Pin *pin2) const
{
  return pathNameCmp(pin1, pin2) < 0;
}

int
Network::pathNameCmp(const Pin *pin1,
		     const Pin *pin2) const
{
  int inst_cmp = pathNameCmp(instance(pin1), instance(pin2));
  if (inst_cmp == 0)
    return strcmp(portName(pin1), portName(pin2));
  else
    return inst_cmp;
}

bool
Network::isInside(const Net *net,
		  const Instance *hier_inst) const
{
  return isInside(instance(net), hier_inst);
}

bool
Network::isLeaf(const Pin *pin) const
{
  return isLeaf(instance(pin));
}

bool
Network::isHierarchical(const Pin *pin) const
{
  return (!isLeaf(pin) && !isTopLevelPort(pin));
}

bool
Network::isTopLevelPort(const Pin *pin) const
{
  return (parent(instance(pin)) == nullptr);
}

bool
Network::isInside(const Pin *pin,
		  const Pin *hier_pin) const
{
  return isInside(pin, instance(hier_pin));
}

bool
Network::isInside(const Pin *pin,
		  const Instance *hier_inst) const
{
  return isInside(instance(pin), hier_inst);
}

bool
Network::pinLess(const Pin *pin1,
		 const Pin *pin2) const
{
  return pathNameLess(pin1, pin2);
}

////////////////////////////////////////////////////////////////

const char *
Network::pathName(const Net *net) const
{
  const Instance *inst = instance(net);
  if (inst && inst != topInstance()) {
    const char *inst_name = pathName(inst);
    size_t inst_name_length = strlen(inst_name);
    const char *net_name = name(net);
    size_t net_name_length = strlen(net_name);
    size_t path_name_length = inst_name_length + net_name_length + 2;
    char *path_name = makeTmpString(path_name_length);
    char *path_ptr = path_name;
    strcpy(path_ptr, inst_name);
    path_ptr += inst_name_length;
    *path_ptr++ = pathDivider();
    strcpy(path_ptr, net_name);
    return path_name;
  }
  else
    return name(net);
}

bool
Network::pathNameLess(const Net *net1,
		      const Net *net2) const
{
  return pathNameCmp(net1, net2) < 0;
}

int
Network::pathNameCmp(const Net *net1,
		     const Net *net2) const
{
  int inst_cmp = pathNameCmp(instance(net1), instance(net2));
  if (inst_cmp == 0)
    return strcmp(name(net1), name(net2));
  else
    return inst_cmp;
}

Net *
Network::highestNetAbove(Net *net) const
{
  Net *highest_net = net;
  // Search up from net terminals.
  NetTermIterator *term_iter = termIterator(net);
  while (term_iter->hasNext()) {
    Term *term = term_iter->next();
    Pin *above_pin = pin(term);
    if (above_pin) {
      Net *above_net = this->net(above_pin);
      if (above_net) {
	highest_net = highestNetAbove(above_net);
	break;
      }
    }
  }
  delete term_iter;
  return highest_net;
}

Net *
Network::highestConnectedNet(Net *net) const
{
  NetSet nets;
  connectedNets(net, &nets);
  Net *highest_net = net;
  int highest_level = hierarchyLevel(net);
  NetSet::Iterator net_iter(nets);
  while (net_iter.hasNext()) {
    Net *net1 = net_iter.next();
    int level = hierarchyLevel(net1);
    if (level < highest_level
	|| (level == highest_level
	    && stringLess(pathName(net1), pathName(highest_net)))) {
      highest_net = net1;
      highest_level = level;
    }
  }
  return highest_net;
}

void
Network::connectedNets(Net *net,
		       NetSet *nets) const
{
  if (!nets->hasKey(net)) {
    nets->insert(net);
    // Search up from net terminals.
    NetTermIterator *term_iter = termIterator(net);
    while (term_iter->hasNext()) {
      Term *term = term_iter->next();
      Pin *above_pin = pin(term);
      if (above_pin) {
	Net *above_net = this->net(above_pin);
	if (above_net)
	  connectedNets(above_net, nets);
      }
    }
    delete term_iter;

    // Search down from net pins.
    NetPinIterator *pin_iter = pinIterator(net);
    while (pin_iter->hasNext()) {
      Pin *pin1 = pin_iter->next();
      Term *below_term = term(pin1);
      if (below_term) {
	Net *below_net = this->net(below_term);
	if (below_net)
	  connectedNets(below_net, nets);
      }
    }
    delete pin_iter;
  }
}

void
Network::connectedNets(const Pin *pin,
		       NetSet *nets) const
{
  Net *net = this->net(pin);
  if (net)
    connectedNets(net, nets);
  else {
    Term *term = this->term(pin);
    if (term) {
      Net *below_net = this->net(term);
      if (below_net)
	connectedNets(below_net, nets);
    }
  }
}

int
Network::hierarchyLevel(const Net *net) const
{
  NetTermIterator *term_iter = network_->termIterator(net);
  while (term_iter->hasNext()) {
    Term *term = term_iter->next();
    Pin *pin = network_->pin(term);
    if (pin) {
      Net *above_net = network_->net(pin);
      if (above_net) {
	delete term_iter;
	return hierarchyLevel(above_net) + 1;
      }
    }
  }
  delete term_iter;
  return 0;
}

bool
Network::isDriver(const Pin *pin) const
{
  PortDirection *dir = direction(pin);
  const Instance *inst = instance(pin);
  return (isLeaf(inst) && dir->isAnyOutput())
    // isTopLevelPort(pin)
    || (isTopInstance(inst) && dir->isAnyInput());
}

bool
Network::isLoad(const Pin *pin) const
{
  PortDirection *dir = direction(pin);
  const Instance *inst = instance(pin);
  return (isLeaf(inst) && dir->isAnyInput())
    // isTopLevelPort(pin)
    || (isTopInstance(inst) && dir->isAnyOutput());
}

bool
Network::isRegClkPin(const Pin *pin) const
{
  const LibertyPort *port = libertyPort(pin);
  return port && port->isRegClk();
}

bool
Network::isCheckClk(const Pin *pin) const
{
  const LibertyPort *port = libertyPort(pin);
  return port && port->isCheckClk();
}

bool
Network::isLatchData(const Pin *pin) const
{
  LibertyPort *port = libertyPort(pin);
  if (port)
    return port->libertyCell()->isLatchData(port);
  else
    return false;
}

////////////////////////////////////////////////////////////////

const char *
Network::name(const Term *term) const
{
  return name(pin(term));
}

const char *
Network::pathName(const Term *term) const
{
  return pathName(pin(term));
}

const char *
Network::portName(const Term *term) const
{
  return portName(pin(term));
}

////////////////////////////////////////////////////////////////

const char *
Network::cellName(const Instance *inst) const
{
  return name(cell(inst));
}

Instance *
Network::findInstance(const char *path_name) const
{
  return findInstanceRelative(topInstance(), path_name);
}

Instance *
Network::findInstanceRelative(const Instance *inst,
			      const char *path_name) const
{
  char *first, *tail;
  pathNameFirst(path_name, first, tail);
  if (first) {
    Instance *inst1 = findChild(inst, first);
    stringDelete(first);
    while (inst1 && tail) {
      char *next_tail;
      pathNameFirst(tail, first, next_tail);
      if (first) {
	inst1 = findChild(inst1, first);
	stringDelete(first);
      }
      else
	inst1 = findChild(inst1, tail);
      stringDelete(tail);
      tail = next_tail;
    }
    stringDelete(tail);
    return inst1;
  }
  else
    return findChild(inst, path_name);
}

void
Network::findInstancesMatching(const Instance *context,
			       const PatternMatch *pattern,
			       InstanceSeq *insts) const
{
  if (pattern->hasWildcards()) {
    size_t context_name_length = 0;
    if (context != topInstance())
      // Add one for the trailing divider.
      context_name_length = strlen(pathName(context)) + 1;
    findInstancesMatching1(context, context_name_length, pattern, insts);
  }
  else {
    Instance *inst = findInstanceRelative(context, pattern->pattern());
    if (inst)
      insts->push_back(inst);
  }
}

void
Network::findInstancesMatching1(const Instance *context,
				size_t context_name_length,
				const PatternMatch *pattern,
				InstanceSeq *insts) const
{
  InstanceChildIterator *child_iter = childIterator(context);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    const char *child_name = pathName(child);
    // Remove context prefix from the name.
    const char *child_context_name = &child_name[context_name_length];
    if (pattern->match(child_context_name))
      insts->push_back(child);
    if (!isLeaf(child))
      findInstancesMatching1(child, context_name_length, pattern, insts);
  }
  delete child_iter;
}

void
Network::findInstancesHierMatching(const Instance *instance,
				   const PatternMatch *pattern,
				   // Return value.
				   InstanceSeq *insts) const
{
  InstanceChildIterator *child_iter = childIterator(instance);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    if (pattern->match(name(child)))
      insts->push_back(child);
    if (!isLeaf(child))
      findInstancesHierMatching(child, pattern, insts);
  }
  delete child_iter;
}

void
Network::findChildrenMatching(const Instance *parent,
			      const PatternMatch *pattern,
			      InstanceSeq *insts) const
{
  if (pattern->hasWildcards()) {
    InstanceChildIterator *child_iter = childIterator(parent);
    while (child_iter->hasNext()) {
      Instance *child = child_iter->next();
      if (pattern->match(name(child)))
	insts->push_back(child);
    }
    delete child_iter;
  }
  else {
    Instance *child = findChild(parent, pattern->pattern());
    if (child)
      insts->push_back(child);
  }
}

Pin *
Network::findPin(const char *path_name) const
{
  return findPinRelative(topInstance(), path_name);
}

Pin *
Network::findPinRelative(const Instance *inst,
			 const char *path_name) const
{
  char *inst_path, *port_name;
  pathNameLast(path_name, inst_path, port_name);
  if (inst_path) {
    Instance *pin_inst = findInstanceRelative(inst, inst_path);
    if (pin_inst) {
      Pin *pin = findPin(pin_inst, port_name);
      stringDelete(inst_path);
      stringDelete(port_name);
      return pin;
    }
    stringDelete(inst_path);
    stringDelete(port_name);
    return nullptr;
  }
  else
    // Top level pin.
    return findPin(inst, path_name);
}

Pin *
Network::findPinLinear(const Instance *instance,
		       const char *port_name) const
{
  InstancePinIterator *pin_iter = pinIterator(instance);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (stringEq(port_name, portName(pin))) {
      delete pin_iter;
      return pin;
    }
  }
  delete pin_iter;
  return nullptr;
}

Pin *
Network::findPin(const Instance *instance,
		 const Port *port) const
{
  return findPin(instance, name(port));
}

Pin *
Network::findPin(const Instance *instance,
		 const LibertyPort *port) const
{
  return findPin(instance, port->name());
}

Net *
Network::findNet(const char *path_name) const
{
  return findNetRelative(topInstance(), path_name);
}

Net *
Network::findNetRelative(const Instance *inst,
			 const char *path_name) const
{
  char *inst_path, *net_name;
  pathNameLast(path_name, inst_path, net_name);
  if (inst_path) {
    Instance *net_inst = findInstanceRelative(inst, inst_path);
    if (net_inst) {
      Net *net = findNet(net_inst, net_name);
      stringDelete(inst_path);
      stringDelete(net_name);
      return net;
    }
    stringDelete(inst_path);
    stringDelete(net_name);
    return nullptr;
  }
  else
    // Top level net.
    return findNet(inst, path_name);
}

Net *
Network::findNetLinear(const Instance *instance,
		       const char *net_name) const
{
  InstanceNetIterator *net_iter = netIterator(instance);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if (stringEq(name(net), net_name)) {
      delete net_iter;
      return net;
    }
  }
  delete net_iter;
  return nullptr;
}

void
Network::findNetsMatching(const Instance *context,
			  const PatternMatch *pattern,
			  NetSeq *nets) const
{
  if (pattern->hasWildcards()) {
    char *inst_path, *net_name;
    pathNameLast(pattern->pattern(), inst_path, net_name);
    if (inst_path) {
      PatternMatch inst_pattern(inst_path, pattern);
      PatternMatch net_pattern(net_name, pattern);
      InstanceSeq insts;
      findInstancesMatching(context, &inst_pattern, &insts);
      InstanceSeq::Iterator inst_iter(insts);
      while (inst_iter.hasNext()) {
	Instance *inst = inst_iter.next();
	findNetsMatching(inst, &net_pattern, nets);
      }
      stringDelete(inst_path);
      stringDelete(net_name);
    }
    else
      // Top level net.
      findInstNetsMatching(context, pattern, nets);
  }
  else {
    Net *net = findNet(pattern->pattern());
    if (net)
      nets->push_back(net);
  }
}

void
Network::findNetsHierMatching(const Instance *instance,
			      const PatternMatch *pattern,
			      // Return value.
			      NetSeq *nets) const
{
  findInstNetsMatching(instance, pattern, nets);
  InstanceChildIterator *child_iter = childIterator(instance);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    findNetsHierMatching(child, pattern, nets);
  }
  delete child_iter;
}

void
Network::findNetsMatchingLinear(const Instance *instance,
				const PatternMatch *pattern,
				NetSeq *nets) const
{
  InstanceNetIterator *net_iter = netIterator(instance);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if (pattern->match(name(net)))
      nets->push_back(net);
  }
  delete net_iter;
}

void
Network::findPinsMatching(const Instance *instance,
			  const PatternMatch *pattern,
			  PinSeq *pins) const
{
  if (pattern->hasWildcards()) {
    char *inst_path, *port_name;
    pathNameLast(pattern->pattern(), inst_path, port_name);
    if (inst_path) {
      PatternMatch inst_pattern(inst_path, pattern);
      PatternMatch port_pattern(port_name, pattern);
      InstanceSeq insts;
      findInstancesMatching(instance, &inst_pattern, &insts);
      InstanceSeq::Iterator inst_iter(insts);
      while (inst_iter.hasNext()) {
	Instance *inst = inst_iter.next();
 	findInstPinsMatching(inst, &port_pattern, pins);
      }
      stringDelete(inst_path);
      stringDelete(port_name);
    }
    else
      // Top level pin.
      findInstPinsMatching(instance, pattern, pins);
  }
  else {
    Pin *pin = findPin(pattern->pattern());
    if (pin)
      pins->push_back(pin);
  }
}

void
Network::findPinsHierMatching(const Instance *instance,
			      const PatternMatch *pattern,
			      // Return value.
			      PinSeq *pins) const
{
  InstanceChildIterator *child_iter = childIterator(instance);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    findInstPinsHierMatching(child, pattern, pins);
    findPinsHierMatching(child, pattern, pins);
  }
  delete child_iter;
}

void
Network::findInstPinsHierMatching(const Instance *instance,
				  const PatternMatch *pattern,
				  // Return value.
				  PinSeq *pins) const
{
  const char *inst_name = name(instance);
  InstancePinIterator *pin_iter = pinIterator(instance);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    const char *port_name = name(port(pin));
    string pin_name;
    stringPrint(pin_name, "%s%c%s", inst_name,divider_, port_name);
    if (pattern->match(pin_name.c_str()))
      pins->push_back(pin);
  }
  delete pin_iter;
}

void
Network::findInstPinsMatching(const Instance *instance,
			      const PatternMatch *pattern,
			      PinSeq *pins) const
{
  if (pattern->hasWildcards()) {
    InstancePinIterator *pin_iter = pinIterator(instance);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      if (pattern->match(name(pin)))
	pins->push_back(pin);
    }
    delete pin_iter;
  }
  else {
    Pin *pin = findPin(instance, pattern->pattern());
    if (pin)
      pins->push_back(pin);
  }
}

int
Network::instanceCount(Instance *inst)
{
  int count = 1;
  InstanceChildIterator *child_iter = childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    count += instanceCount(child);
  }
  delete child_iter;
  return count;
}

int
Network::instanceCount()
{
  return instanceCount(topInstance());
}

int
Network::pinCount(Instance *inst)
{
  int count = 0;
  InstancePinIterator *pin_iter = pinIterator(inst);
  while (pin_iter->hasNext()) {
    pin_iter->next();
    count++;
  }
  delete pin_iter;

  InstanceChildIterator *child_iter = childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    count += pinCount(child);
  }
  delete child_iter;
  return count;
}

int
Network::pinCount()
{
  return pinCount(topInstance());
}

int
Network::netCount(Instance *inst)
{
  int count = 0;
  InstanceNetIterator *net_iter = netIterator(inst);
  while (net_iter->hasNext()) {
    net_iter->next();
    count++;
  }
  delete net_iter;

  InstanceChildIterator *child_iter = childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    count += netCount(child);
  }
  delete child_iter;
  return count;
}

int
Network::netCount()
{
  return netCount(topInstance());
}

int
Network::leafInstanceCount()
{
  int count = 0;
  LeafInstanceIterator *leaf_iter = leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    leaf_iter->next();
    count++;
  }
  delete leaf_iter;
  return count;
}

int
Network::leafPinCount()
{
  int count = 0;
  LeafInstanceIterator *leaf_iter = leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *leaf = leaf_iter->next();
    InstancePinIterator *pin_iter = pinIterator(leaf);
    while (pin_iter->hasNext()) {
      pin_iter->next();
      count++;
    }
    delete pin_iter;
  }
  delete leaf_iter;
  return count;
}

void
Network::setPathDivider(char divider)
{
  divider_ = divider;
}

void
Network::setPathEscape(char escape)
{
  escape_ = escape;
}

////////////////////////////////////////////////////////////////

typedef Vector<InstanceChildIterator *> InstanceChildIteratorSeq; 

class LeafInstanceIterator1 : public LeafInstanceIterator
{
public:
  LeafInstanceIterator1(const Instance *inst,
			const Network *network);
  bool hasNext() { return next_; }
  Instance *next();

private:
  DISALLOW_COPY_AND_ASSIGN(LeafInstanceIterator1);
  void nextInst();

  const Network *network_;
  InstanceChildIteratorSeq pending_child_iters_;
  InstanceChildIterator *child_iter_;
  Instance *next_;

};

LeafInstanceIterator1::LeafInstanceIterator1(const Instance *inst,
					     const Network *network) :
  network_(network),
  child_iter_(network->childIterator(inst)),
  next_(nullptr)
{
  pending_child_iters_.reserve(8);
  nextInst();
}

Instance *
LeafInstanceIterator1::next()
{
  Instance *next = next_;
  nextInst();
  return next;
}

void
LeafInstanceIterator1::nextInst()
{
  next_ = nullptr;
  while (child_iter_) {
    while (child_iter_->hasNext()) {
      next_ = child_iter_->next();
      if (network_->isLeaf(next_))
        return;
      else {
        pending_child_iters_.push_back(child_iter_);
        child_iter_ = network_->childIterator(next_);
        next_ = nullptr;
      }
    }
    delete child_iter_;

    if (pending_child_iters_.empty())
      child_iter_ = nullptr;
    else {
      child_iter_ = pending_child_iters_.back();
      pending_child_iters_.pop_back();
    }
  }
}

LeafInstanceIterator *
Network::leafInstanceIterator() const
{
  return new LeafInstanceIterator1(topInstance(), this);
}

LeafInstanceIterator *
Network::leafInstanceIterator(const Instance *hier_inst) const
{
  return new LeafInstanceIterator1(hier_inst, this);
}

////////////////////////////////////////////////////////////////

void
Network::visitConnectedPins(Pin *pin,
			    PinVisitor &visitor) const
{
  ConstNetSet visited_nets;
  Net *pin_net = net(pin);
  Term *pin_term = term(pin);
  if (pin_net)
    visitConnectedPins(pin_net, visitor, visited_nets);
  else if (pin_term == nullptr)
    // Unconnected or internal pin.
    visitor(pin);

  // Search down from pin terminal.
  if (pin_term) {
    Net *term_net = net(pin_term);
    if (term_net)
      visitConnectedPins(term_net, visitor, visited_nets);
  }
}

void
Network::visitConnectedPins(const Net *net,
			    PinVisitor &visitor) const
{
  ConstNetSet visited_nets;
  visitConnectedPins(net, visitor, visited_nets);
}

void
Network::visitConnectedPins(const Net *net,
			    PinVisitor &visitor,
			    ConstNetSet &visited_nets) const
{
  if (!visited_nets.hasKey(net)) {
    visited_nets.insert(net);
    // Search up from net terminals.
    NetTermIterator *term_iter = termIterator(net);
    while (term_iter->hasNext()) {
      Term *term = term_iter->next();
      Pin *above_pin = pin(term);
      if (above_pin) {
	Net *above_net = this->net(above_pin);
	if (above_net)
	  visitConnectedPins(above_net, visitor, visited_nets);
	else
	  visitor(above_pin);
      }
    }
    delete term_iter;

    // Search down from net pins.
    NetPinIterator *pin_iter = pinIterator(net);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      visitor(pin);
      Term *below_term = term(pin);
      if (below_term) {
	Net *below_net = this->net(below_term);
	if (below_net)
	  visitConnectedPins(below_net, visitor, visited_nets);
      }
    }
    delete pin_iter;
  }
}

////////////////////////////////////////////////////////////////

class ConnectedPinIterator1 : public ConnectedPinIterator
{
public:
  explicit ConnectedPinIterator1(PinSet *pins);
  virtual ~ConnectedPinIterator1();
  virtual bool hasNext();
  virtual Pin *next();

protected:
  PinSet::Iterator pin_iter_;

private:
  DISALLOW_COPY_AND_ASSIGN(ConnectedPinIterator1);
};

ConnectedPinIterator1::ConnectedPinIterator1(PinSet *pins) :
  pin_iter_(pins)
{
}

ConnectedPinIterator1::~ConnectedPinIterator1()
{
  delete pin_iter_.container();
}

bool
ConnectedPinIterator1::hasNext()
{
  return pin_iter_.hasNext();
}

Pin *
ConnectedPinIterator1::next()
{
  return pin_iter_.next();
}

class FindConnectedPins : public PinVisitor
{
public:
  explicit FindConnectedPins(PinSet *pins);
  virtual void operator()(Pin *pin);

protected:
  PinSet *pins_;

private:
  DISALLOW_COPY_AND_ASSIGN(FindConnectedPins);
};

FindConnectedPins::FindConnectedPins(PinSet *pins) :
  PinVisitor(),
  pins_(pins)
{
}

void
FindConnectedPins::operator()(Pin *pin)
{
  pins_->insert(pin);
}

NetConnectedPinIterator *
Network::connectedPinIterator(const Net *net) const
{
  PinSet *pins = new PinSet;
  FindConnectedPins visitor(pins);
  visitConnectedPins(net, visitor);
  return new ConnectedPinIterator1(pins);
}

PinConnectedPinIterator *
Network::connectedPinIterator(const Pin *pin) const
{
  PinSet *pins = new PinSet;
  pins->insert(const_cast<Pin*>(pin));

  FindConnectedPins visitor(pins);
  Net *pin_net = net(pin);
  if (pin_net)
    visitConnectedPins(pin_net, visitor);

  // Search down from pin terminal.
  Term *pin_term = term(pin);
  if (pin_term) {
    Net *term_net = net(pin_term);
    if (term_net)
      visitConnectedPins(term_net, visitor);
  }
  return new ConnectedPinIterator1(pins);
}

////////////////////////////////////////////////////////////////

bool
Network::isConnected(const Net *net,
		     const Pin *pin) const
{
  if (this->net(pin) == net)
    return true;
  else {
    ConstNetSet nets;
    return isConnected(net, pin, nets);
  }
}

bool
Network::isConnected(const Net *net,
		     const Pin *pin,
		     ConstNetSet &nets) const
{
  if (!nets.hasKey(net)) {
    nets.insert(net);
    // Search up from net terminals.
    NetTermIterator *term_iter = termIterator(net);
    while (term_iter->hasNext()) {
      Term *term = term_iter->next();
      Pin *above_pin = this->pin(term);
      if (above_pin) {
	if (above_pin == pin) {
	  delete term_iter;
	  return true;
	}
	else {
	  Net *above_net = this->net(above_pin);
	  if (above_net && isConnected(above_net, pin, nets)) {
	    delete term_iter;
	    return true;
	  }
	}
      }
    }
    delete term_iter;

    // Search down from net pins.
    NetPinIterator *pin_iter = pinIterator(net);
    while (pin_iter->hasNext()) {
      Pin *pin1 = pin_iter->next();
      if (pin1 == pin) {
	delete pin_iter;
	return true;
      }
      else {
	Term *below_term = term(pin1);
	if (below_term) {
	  Net *below_net = this->net(below_term);
	  if (below_net && isConnected(below_net, pin, nets)) {
	    delete pin_iter;
	    return true;
	  }
	}
      }
    }
    delete pin_iter;
  }
  return false;
}

bool
Network::isConnected(const Net *net1,
		     const Net *net2) const
{
  ConstNetSet nets;
  return isConnected(net1, net2, nets);
}

bool
Network::isConnected(const Net *net1,
		     const Net *net2,
		     ConstNetSet &nets) const
{
  if (net1 == net2)
    return true;
  else if (!nets.hasKey(net1)) {
    nets.insert(net1);
    // Search up from net terminals.
    NetTermIterator *term_iter = termIterator(net1);
    while (term_iter->hasNext()) {
      Term *term = term_iter->next();
      Pin *above_pin = pin(term);
      if (above_pin) {
	Net *above_net = net(above_pin);
	if (above_net && isConnected(above_net, net2, nets)) {
	  delete term_iter;
	  return true;
	}
      }
    }
    delete term_iter;

    // Search down from net pins.
    NetPinIterator *pin_iter = pinIterator(net1);
    while (pin_iter->hasNext()) {
      Pin *pin1 = pin_iter->next();
      Term *below_term = term(pin1);
      if (below_term) {
	Net *below_net = net(below_term);
	if (below_net && isConnected(below_net, net2, nets)) {
	  delete pin_iter;
	  return true;
	}
      }
    }
    delete pin_iter;
  }
  return false;
}

////////////////////////////////////////////////////////////////

class FindDrvrPins : public PinVisitor
{
public:
  explicit FindDrvrPins(PinSet *pins,
			const Network *network);
  virtual void operator()(Pin *pin);

protected:
  PinSet *pins_;
  const Network *network_;

private:
  DISALLOW_COPY_AND_ASSIGN(FindDrvrPins);
};

FindDrvrPins::FindDrvrPins(PinSet *pins,
			   const Network *network) :
  PinVisitor(),
  pins_(pins),
  network_(network)
{
}

void
FindDrvrPins::operator()(Pin *pin)
{
  if (network_->isDriver(pin))
    pins_->insert(pin);
}

PinSet *
Network::drivers(const Pin *pin)
{
  const Net *net = this->net(pin);
  if (net)
    return drivers(net);
  else
    return nullptr;
}

void
Network::clearNetDrvrPinMap()
{
  net_drvr_pin_map_.deleteContentsClear();
}

PinSet *
Network::drivers(const Net *net)
{
  PinSet *drvrs = net_drvr_pin_map_.findKey(net);
  if (drvrs == nullptr) {
    drvrs = new PinSet;
    FindDrvrPins visitor(drvrs, this);
    visitConnectedPins(net, visitor);
    net_drvr_pin_map_[net] = drvrs;
  }
  return drvrs;
}

////////////////////////////////////////////////////////////////

void
Network::pathNameFirst(const char *path_name,
		       char *&first,
		       char *&tail) const
{
  char escape = pathEscape();
  char divider = pathDivider();
  const char *d = strchr(path_name, divider);
  // Skip escaped dividers.
  while (d != nullptr
	 && d > path_name
	 && d[-1] == escape)
    d = strchr(d + 1, divider);
  if (d) {
    first = new char[d - path_name + 1];
    strncpy(first, path_name, d - path_name);
    first[d - path_name] = '\0';

    tail = new char[strlen(d)];
    // Chop off the leading divider.
    strcpy(tail, d + 1);
  }
  else {
    // No divider in path_name.
    first = nullptr;
    tail = nullptr;
  }
}

void
Network::pathNameLast(const char *path_name,
		      char *&head,
		      char *&last) const
{
  char escape = pathEscape();
  char divider = pathDivider();
  const char *d = strrchr(path_name, divider);
  // Search for a non-escaped divider.
  if (d) {
    while (d > path_name
	   && (d[0] != divider
	       || (d[0] == divider
		   && d > &path_name[1]
		   && d[-1] == escape)))
      d--;
  }
  if (d && d != path_name) {
    head = new char[d - path_name + 1];
    strncpy(head, path_name, d - path_name);
    head[d - path_name] = '\0';

    last = new char[strlen(d)];
    // Chop off the last divider.
    strcpy(last, d + 1);
  }
  else {
    // No divider in path_name.
    head = nullptr;
    last = nullptr;
  }
}

////////////////////////////////////////////////////////////////

NetworkEdit::NetworkEdit() :
  Network()
{
}

void
NetworkEdit::connectPin(Pin *pin,
			Net *net)
{
  connect(instance(pin), port(pin), net);
}

////////////////////////////////////////////////////////////////

NetworkConstantPinIterator::
NetworkConstantPinIterator(const Network *network,
			   NetSet &zero_nets,
			   NetSet &one_nets) :
  ConstantPinIterator(),
  network_(network)
{
  findConstantPins(zero_nets, constant_pins_[0]);
  findConstantPins(one_nets, constant_pins_[1]);
  value_ = LogicValue::zero;
  pin_iter_ = new PinSet::Iterator(constant_pins_[0]);
}

NetworkConstantPinIterator::~NetworkConstantPinIterator()
{
  delete pin_iter_;
}

void
NetworkConstantPinIterator::findConstantPins(NetSet &nets,
					     PinSet &pins)
{
  NetSet::Iterator net_iter(nets);
  while (net_iter.hasNext()) {
    const Net *net = net_iter.next();
    NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      pins.insert(pin);
    }
    delete pin_iter;
  }
}

bool
NetworkConstantPinIterator::hasNext()
{
  if (pin_iter_->hasNext())
    return true;
  else if (value_ == LogicValue::zero) {
    delete pin_iter_;
    value_ = LogicValue::one;
    pin_iter_ = new PinSet::Iterator(constant_pins_[1]);
    return pin_iter_->hasNext();
  }
  else
    return false;
}

void
NetworkConstantPinIterator::next(Pin *&pin,
				 LogicValue &value)
{
  pin = pin_iter_->next();
  value = value_;
}

////////////////////////////////////////////////////////////////

FindNetDrvrLoads::FindNetDrvrLoads(Pin *drvr_pin,
				   PinSet &visited_drvrs,
				   PinSeq &loads,
				   PinSeq &drvrs,
				   const Network *network) :
  drvr_pin_(drvr_pin),
  visited_drvrs_(visited_drvrs),
  loads_(loads),
  drvrs_(drvrs),
  network_(network)
{
}

void
FindNetDrvrLoads::operator()(Pin *pin)
{
  if (network_->isLoad(pin))
    loads_.push_back(pin);
  if (network_->isDriver(pin)) {
    drvrs_.push_back(pin);
    if (pin != drvr_pin_)
      visited_drvrs_.insert(pin);
  }
}

////////////////////////////////////////////////////////////////

static void
visitPinsAboveNet1(const Pin *hpin,
		   Net *above_net,
		   NetSet &visited,
		   PinSet &above_drvrs,
		   PinSet &above_loads,
		   const Network *network)
{
  visited.insert(above_net);
  // Visit above net pins.
  NetPinIterator *pin_iter = network->pinIterator(above_net);
  while (pin_iter->hasNext()) {
    Pin *above_pin = pin_iter->next();
    if (above_pin != hpin) {
      if (network->isDriver(above_pin))
	above_drvrs.insert(above_pin);
      if (network->isLoad(above_pin))
	above_loads.insert(above_pin);
      Term *above_term = network->term(above_pin);
      if (above_term) {
	Net *above_net1 = network->net(above_term);
	if (above_net1 && !visited.hasKey(above_net1))
	  visitPinsAboveNet1(above_pin, above_net1, visited,
			     above_drvrs, above_loads, network);
      }
    }
  }
  delete pin_iter;

  // Search up from net terminals.
  NetTermIterator *term_iter = network->termIterator(above_net);
  while (term_iter->hasNext()) {
    Term *term = term_iter->next();
    Pin *above_pin = network->pin(term);
    if (above_pin
	&& above_pin != hpin) {
      Net *above_net1 = network->net(above_pin);
      if (above_net1 && !visited.hasKey(above_net1))
	visitPinsAboveNet1(above_pin, above_net1, visited,
			   above_drvrs, above_loads, network);
      if (network->isDriver(above_pin))
	above_drvrs.insert(above_pin);
      if (network->isLoad(above_pin))
	above_loads.insert(above_pin);
    }
  }
  delete term_iter;
}

static void
visitPinsBelowNet1(const Pin *hpin,
		   Net *below_net,
		   NetSet &visited,
		   PinSet &below_drvrs,
		   PinSet &below_loads,
		   const Network *network)
{
  visited.insert(below_net);
  // Visit below net pins.
  NetPinIterator *pin_iter = network->pinIterator(below_net);
  while (pin_iter->hasNext()) {
    Pin *below_pin = pin_iter->next();
    if (below_pin != hpin) {
      NetSet visited_above;
      if (network->isDriver(below_pin))
	below_drvrs.insert(below_pin);
      if (network->isLoad(below_pin))
	below_loads.insert(below_pin);
      if (network->isHierarchical(below_pin)) {
	Term *term = network->term(below_pin);
	if (term) {
	  Net *below_net1 = network->net(term);
	  if (below_net1 && !visited.hasKey(below_net1))
	    visitPinsBelowNet1(below_pin, below_net1, visited,
			       below_drvrs, below_loads, network);
	}
      }
    }
  }
  delete pin_iter;
}

static void
visitDrvrLoads(PinSet drvrs,
	       PinSet loads,
	       HierPinThruVisitor *visitor)
{
  PinSet::Iterator drvr_iter(drvrs);
  while (drvr_iter.hasNext()) {
    Pin *drvr = drvr_iter.next();
    PinSet::Iterator load_iter(loads);
    while (load_iter.hasNext()) {
      Pin *load = load_iter.next();
      visitor->visit(drvr, load);
    }
  }
}

void
visitDrvrLoadsThruHierPin(const Pin *hpin,
			  const Network *network,
			  HierPinThruVisitor *visitor)
{
  Net *above_net = network->net(hpin);
  if (above_net) {
    // Search down from hpin terminal.
    Term *term = network->term(hpin);
    if (term) {
      Net *below_net = network->net(term);
      if (below_net) {
	NetSet visited;
	PinSet above_drvrs;
	PinSet above_loads;
	visitPinsAboveNet1(hpin, above_net, visited,
			   above_drvrs, above_loads, network);
	PinSet below_drvrs;
	PinSet below_loads;
	visitPinsBelowNet1(hpin, below_net, visited,
			   below_drvrs, below_loads, network);
	visitDrvrLoads(above_drvrs, below_loads, visitor);
	visitDrvrLoads(below_drvrs, above_loads, visitor);
      }
    }
  }
}

void
visitDrvrLoadsThruNet(Net *net,
		      const Network *network,
		      HierPinThruVisitor *visitor)
{
  NetSet visited;
  PinSet above_drvrs;
  PinSet above_loads;
  PinSet below_drvrs;
  PinSet below_loads;
  PinSet net_drvrs;
  PinSet net_loads;
  NetPinIterator *pin_iter = network->pinIterator(net);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network->isHierarchical(pin)) {
      // Search down from pin terminal.
      const Term *term = network->term(pin);
      if (term) {
	Net *below_net = network->net(term);
	if (below_net) {
	  visitPinsBelowNet1(pin, below_net, visited,
			     below_drvrs, below_loads, network);
	}
      }
    }
    else {
      if (network->isDriver(pin))
	net_drvrs.insert(pin);
      if (network->isLoad(pin))
	net_loads.insert(pin);
    }
  }
  delete pin_iter;

  NetTermIterator *term_iter = network->termIterator(net);
  while (term_iter->hasNext()) {
    Term *term = term_iter->next();
    Pin *above_pin = network->pin(term);
    if (above_pin) {
      if (network->isDriver(above_pin))
	above_drvrs.insert(above_pin);
      if (network->isLoad(above_pin))
	above_loads.insert(above_pin);
      Net *above_net = network->net(above_pin);
      if (above_net)
	visitPinsAboveNet1(above_pin, above_net, visited,
			   above_drvrs, above_loads, network);
    }
  }
  delete term_iter;
  visitDrvrLoads(above_drvrs, below_loads, visitor);
  visitDrvrLoads(above_drvrs, net_loads, visitor);
  visitDrvrLoads(below_drvrs, above_loads, visitor);
  visitDrvrLoads(below_drvrs, net_loads, visitor);
  visitDrvrLoads(net_drvrs, above_loads, visitor);
  visitDrvrLoads(net_drvrs, below_loads, visitor);
  visitDrvrLoads(net_drvrs, net_loads, visitor);
}

////////////////////////////////////////////////////////////////

char
logicValueString(LogicValue value)
{
  static char str[] = "01X^v";
  return str[int(value)];
}

bool
PortPairLess::operator()(const PortPair *pair1,
			 const PortPair *pair2) const
{
  return pair1->first < pair2->first
    || (pair1->first == pair2->first
	&& pair1->second < pair2->second);
}

} // namespace
