// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.


#include "Network.hh"

#include <compare>
#include <set>

#include "ContainerHelpers.hh"
#include "StringUtil.hh"
#include "PatternMatch.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Scene.hh"
#include "ParseBus.hh"

namespace sta {

Network::Network() :
  default_liberty_(nullptr),
  divider_('/'),
  escape_('\\')
{
}

Network::~Network()
{
  deleteContents(net_drvr_pin_map_);
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

PortSeq
Network::findPortsMatching(const Cell *cell,
                           const PatternMatch *pattern) const
{
  PortSeq matches;
  bool is_bus, is_range, subscript_wild;
  std::string bus_name;
  int from, to;
  parseBusName(pattern->pattern(), '[', ']', '\\',
               is_bus, is_range, bus_name, from, to, subscript_wild);
  if (is_bus) {
    PatternMatch bus_pattern(bus_name, pattern);
    CellPortIterator *port_iter = portIterator(cell);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      if (isBus(port)
          && bus_pattern.match(name(port))) {
        if (is_range) {
          // bus[8:0]
          if (from > to)
            std::swap(from, to);
          for (int bit = from; bit <= to; bit++) {
            Port *port_bit = findBusBit(port, bit);
            matches.push_back(port_bit);
          }
        }
        else {
          if (subscript_wild) {
            PortMemberIterator *member_iter = memberIterator(port);
            while (member_iter->hasNext()) {
              Port *port_bit = member_iter->next();
              matches.push_back(port_bit);
            }
            delete member_iter;
          }
          else {
            // bus[0]
            Port *port_bit = findBusBit(port, from);
            if (port_bit != nullptr)
              matches.push_back(port_bit);
          }
        }
      }
    }
    delete port_iter;
  }
  else {
    CellPortIterator *port_iter = portIterator(cell);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      if (pattern->match(name(port)))
        matches.push_back(port);
    }
    delete port_iter;
  }
  return matches;
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
Network::findLibertyCell(std::string_view name) const
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

void
Network::checkLibertyScenes()
{
  if (multiScene()) {
    LibertyLibraryIterator *lib_iter = libertyLibraryIterator();
    LibertyCellSet cells;
    while (lib_iter->hasNext()) {
      LibertyLibrary *lib = lib_iter->next();
      LibertyCellIterator cell_iter(lib);
      while (cell_iter.hasNext()) {
        LibertyCell *cell = cell_iter.next();
        LibertyCell *link_cell = findLibertyCell(cell->name());
        cells.insert(link_cell);
      }
    }
    delete lib_iter;

    for (LibertyCell *cell : cells)
      LibertyLibrary::checkScenes(cell, scenes_, report_);
  }
}

void
Network::checkNetworkLibertyScenes()
{
  if (multiScene()) {
    LibertyCellSet network_cells;
    LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
    while (leaf_iter->hasNext()) {
      const Instance *inst = leaf_iter->next();
      LibertyCell *cell = libertyCell(inst);
      if (cell)
        network_cells.insert(cell);
    }
    delete leaf_iter;

    for (LibertyCell *cell : network_cells)
      LibertyLibrary::checkScenes(cell, scenes_, report_);
  }
}

LibertyLibrary *
Network::findLibertyFilename(std::string_view filename)
{
  LibertyLibraryIterator *lib_iter = libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    if (lib->filename() == filename) {
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
  Port *port = this->port(pin);
  if (port)
    return libertyPort(port);
  else
    return nullptr;
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

std::string
Network::pathName(const Instance *instance) const
{
  InstanceSeq inst_path;
  path(instance, inst_path);
  std::string path_name;
  while (inst_path.size()) {
    const Instance *inst = inst_path.back();
    path_name += name(inst);
    inst_path.pop_back();
    if (!inst_path.empty())
      path_name += pathDivider();
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
    InstanceSeq path1;
    InstanceSeq path2;
    path(inst1, path1);
    path(inst2, path2);
    while (!path1.empty() && !path2.empty()) {
      const Instance *inst1 = path1.back();
      const Instance *inst2 = path2.back();
      auto cmp = name(inst1) <=> name(inst2);
      if (cmp != 0)
        return cmp < 0 ? -1 : 1;
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
              InstanceSeq &path) const
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

std::string
Network::name(const Pin *pin) const
{
  return pathName(pin);
}

std::string
Network::portName(const Pin *pin) const
{
  return name(port(pin));
}

std::string
Network::pathName(const Pin *pin) const
{
  const Instance *inst = instance(pin);
  if (inst && inst != topInstance()) {
    std::string path_name(pathName(inst));
    path_name += pathDivider();
    path_name += portName(pin);
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
  if (inst_cmp == 0) {
    auto cmp = portName(pin1) <=> portName(pin2);
    if (cmp < 0)
      return -1;
    if (cmp > 0)
      return 1;
    return 0;
  }
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

std::string
Network::pathName(const Net *net) const
{
  const Instance *inst = instance(net);
  if (inst && inst != topInstance()) {
    std::string path_name(pathName(inst));
    path_name += pathDivider();
    path_name += name(net);
    return path_name;
  }
  else
    return std::string(name(net));
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
  if (inst_cmp == 0) {
    auto cmp = name(net1) <=> name(net2);
    if (cmp < 0)
      return -1;
    if (cmp > 0)
      return 1;
    return 0;
  }
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

const Net *
Network::highestConnectedNet(Net *net) const
{
  NetSet nets(this);
  connectedNets(net, &nets);
  const Net *highest_net = net;
  int highest_level = hierarchyLevel(net);
  for (const Net *net1 : nets) {
    int level = hierarchyLevel(net1);
    if (level < highest_level
        || (level == highest_level
            && pathName(net1) < pathName(highest_net))) {
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
  if (!nets->contains(net)) {
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
      const Pin *pin1 = pin_iter->next();
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
    || (isTopInstance(inst) && dir->isAnyOutput())
    // Black box unknown ports are treated as loads.
    || dir->isUnknown();
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
    return port->isLatchData();
  else
    return false;
}

////////////////////////////////////////////////////////////////

std::string
Network::name(const Term *term) const
{
  return name(pin(term));
}

std::string
Network::pathName(const Term *term) const
{
  return pathName(pin(term));
}

std::string
Network::portName(const Term *term) const
{
  return portName(pin(term));
}

////////////////////////////////////////////////////////////////

std::string
Network::cellName(const Instance *inst) const
{
  return name(cell(inst));
}

Instance *
Network::findInstance(std::string_view path_name) const
{
  return findInstanceRelative(topInstance(), path_name);
}

Instance *
Network::findInstanceRelative(const Instance *inst,
                              std::string_view path_name) const
{
  std::string first, tail;
  pathNameFirst(path_name, first, tail);
  if (!first.empty()) {
    Instance *inst1 = findChild(inst, first);
    while (inst1 && !tail.empty()) {
      std::string next_tail;
      pathNameFirst(tail, first, next_tail);
      if (!first.empty())
        inst1 = findChild(inst1, first);
      else
        inst1 = findChild(inst1, tail);
      tail = next_tail;
    }
    return inst1;
  }
  else
    return findChild(inst, path_name);
}

InstanceSeq
Network::findInstancesMatching(const Instance *context,
                               const PatternMatch *pattern) const
{
  InstanceSeq matches;
  if (pattern->hasWildcards()) {
    size_t context_name_length = 0;
    if (context != topInstance())
      // Add one for the trailing divider.
      context_name_length = pathName(context).size() + 1;
    findInstancesMatching1(context, context_name_length, pattern, matches);
  }
  else {
    Instance *inst = findInstanceRelative(context, pattern->pattern());
    if (inst)
      matches.push_back(inst);
  }
  return matches;
}

void
Network::findInstancesMatching1(const Instance *context,
                                size_t context_name_length,
                                const PatternMatch *pattern,
                                InstanceSeq &matches) const
{
  InstanceChildIterator *child_iter = childIterator(context);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    std::string child_name = pathName(child);
    // Remove context prefix from the name.
    std::string_view child_context_name =
      std::string_view(child_name).substr(context_name_length);
    if (pattern->match(child_context_name))
      matches.push_back(child);
    if (!isLeaf(child))
      findInstancesMatching1(child, context_name_length, pattern, matches);
  }
  delete child_iter;
}

InstanceSeq
Network::findInstancesHierMatching(const Instance *instance,
                                   const PatternMatch *pattern) const
{
  InstanceSeq matches;
  findInstancesHierMatching1(instance, pattern, matches);
  return matches;
}

void
Network::findInstancesHierMatching1(const Instance *instance,
                                    const PatternMatch *pattern,
                                    InstanceSeq &matches) const
{
  InstanceChildIterator *child_iter = childIterator(instance);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    if (pattern->match(name(child)))
      matches.push_back(child);
    if (!isLeaf(child))
      findInstancesHierMatching1(child, pattern, matches);
  }
  delete child_iter;
}

void
Network::findChildrenMatching(const Instance *parent,
                              const PatternMatch *pattern,
                              InstanceSeq &matches) const
{
  if (pattern->hasWildcards()) {
    InstanceChildIterator *child_iter = childIterator(parent);
    while (child_iter->hasNext()) {
      Instance *child = child_iter->next();
      if (pattern->match(name(child)))
        matches.push_back(child);
    }
    delete child_iter;
  }
  else {
    Instance *child = findChild(parent, pattern->pattern());
    if (child)
      matches.push_back(child);
  }
}

Pin *
Network::findPin(std::string_view path_name) const
{
  return findPinRelative(topInstance(), path_name);
}

Pin *
Network::findPinRelative(const Instance *inst,
                         std::string_view path_name) const
{
  std::string inst_path, port_name;
  std::string path_storage(path_name);
  pathNameLast(path_storage, inst_path, port_name);
  if (!inst_path.empty()) {
    Instance *pin_inst = findInstanceRelative(inst, inst_path);
    if (pin_inst)
      return findPin(pin_inst, port_name);
    return nullptr;
  }
  else
    // Top level pin.
    return findPin(inst, path_name);
}

Pin *
Network::findPinLinear(const Instance *instance,
                       std::string_view port_name) const
{
  InstancePinIterator *pin_iter = pinIterator(instance);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (port_name == portName(pin)) {
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
Network::findNet(std::string_view path_name) const
{
  return findNetRelative(topInstance(), path_name);
}

Net *
Network::findNetRelative(const Instance *inst,
                         std::string_view path_name) const
{
  std::string inst_path, net_name;
  std::string path_storage(path_name);
  pathNameLast(path_storage, inst_path, net_name);
  if (!inst_path.empty()) {
    Instance *net_inst = findInstanceRelative(inst, inst_path);
    if (net_inst)
      return findNet(net_inst, net_name);
    return nullptr;
  }
  else
    // Top level net.
    return findNet(inst, path_name);
}

Net *
Network::findNetLinear(const Instance *instance,
                       std::string_view net_name) const
{
  InstanceNetIterator *net_iter = netIterator(instance);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if (name(net) == net_name) {
      delete net_iter;
      return net;
    }
  }
  delete net_iter;
  return nullptr;
}

NetSeq
Network::findNetsMatching(const Instance *context,
                          const PatternMatch *pattern) const
{
  NetSeq matches;
  findNetsMatching(context, pattern, matches);
  return matches;
}

void
Network::findNetsMatching(const Instance *context,
                          const PatternMatch *pattern,
                          NetSeq &matches) const
{
  if (pattern->hasWildcards()) {
    std::string inst_path, net_name;
    pathNameLast(pattern->pattern(), inst_path, net_name);
    if (!inst_path.empty()) {
      PatternMatch inst_pattern(inst_path, pattern);
      PatternMatch net_pattern(net_name, pattern);
      InstanceSeq insts = findInstancesMatching(context, &inst_pattern);
      for (const Instance *inst : insts)
        findNetsMatching(inst, &net_pattern, matches);
    }
    else
      // Top level net.
      findInstNetsMatching(context, pattern, matches);
  }
  else {
    Net *net = findNet(pattern->pattern());
    if (net)
      matches.push_back(net);
  }
}

NetSeq
Network::findNetsHierMatching(const Instance *instance,
                              const PatternMatch *pattern) const
{
  NetSeq matches;
  findNetsHierMatching(instance, pattern, matches);
  return matches;
}

void
Network::findNetsHierMatching(const Instance *instance,
                              const PatternMatch *pattern,
                              NetSeq &matches) const
{
  findInstNetsMatching(instance, pattern, matches);
  InstanceChildIterator *child_iter = childIterator(instance);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    findNetsHierMatching(child, pattern, matches);
  }
  delete child_iter;
}

NetSeq
Network::findNetsMatchingLinear(const Instance *instance,
                                const PatternMatch *pattern) const
{
  NetSeq matches;
  InstanceNetIterator *net_iter = netIterator(instance);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if (pattern->match(name(net)))
      matches.push_back(net);
  }
  delete net_iter;
  return matches;
}

PinSeq
Network::findPinsMatching(const Instance *instance,
                          const PatternMatch *pattern) const
{
  PinSeq matches;
  if (pattern->hasWildcards()) {
    std::string inst_path, port_name;
    pathNameLast(pattern->pattern(), inst_path, port_name);
    if (!inst_path.empty()) {
      PatternMatch inst_pattern(inst_path, pattern);
      PatternMatch port_pattern(port_name, pattern);
      InstanceSeq insts = findInstancesMatching(instance, &inst_pattern);
      for (const Instance *inst : insts)
        findInstPinsMatching(inst, &port_pattern, matches);
    }
    else
      // Top level pin.
      findInstPinsMatching(instance, pattern, matches);
  }
  else {
    Pin *pin = findPin(pattern->pattern());
    if (pin)
      matches.push_back(pin);
  }
  return matches;
}

PinSeq
Network::findPinsHierMatching(const Instance *instance,
                              const PatternMatch *pattern) const
{
  PinSeq matches;
  findPinsHierMatching(instance, pattern, matches);
  return matches;
}

void
Network::findPinsHierMatching(const Instance *instance,
                              const PatternMatch *pattern,
                              // Return value.
                              PinSeq &matches) const
{
  InstanceChildIterator *child_iter = childIterator(instance);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    findInstPinsHierMatching(child, pattern, matches);
    findPinsHierMatching(child, pattern, matches);
  }
  delete child_iter;
}

void
Network::findInstPinsHierMatching(const Instance *instance,
                                  const PatternMatch *pattern,
                                  // Return value.
                                  PinSeq &matches) const
{
  std::string inst_name(name(instance));
  InstancePinIterator *pin_iter = pinIterator(instance);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    std::string port_name = name(port(pin));
    std::string pin_name = inst_name + divider_ + std::string(port_name);
      if (pattern->match(pin_name))
      matches.push_back(pin);
  }
  delete pin_iter;
}

void
Network::findInstPinsMatching(const Instance *instance,
                              const PatternMatch *pattern,
                              PinSeq &matches) const
{
  if (pattern->hasWildcards()) {
    InstancePinIterator *pin_iter = pinIterator(instance);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      if (pattern->match(name(pin)))
        matches.push_back(pin);
    }
    delete pin_iter;
  }
  else {
    Pin *pin = findPin(instance, pattern->pattern());
    if (pin)
      matches.push_back(pin);
  }
}

void
Network::location(const Pin *,
                  // Return values.
                  double &x,
                  double &y,
                  bool &exists) const
{
  x = y = 0.0;
  exists = false;
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

InstanceSeq
Network::leafInstances()
{
  InstanceSeq insts;
  LeafInstanceIterator *iter = leafInstanceIterator();
  while (iter->hasNext()) {
    const Instance *inst = iter->next();
    insts.push_back(inst);
  }
  delete iter;
  return insts;
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

typedef std::vector<InstanceChildIterator *> InstanceChildIteratorSeq; 

class LeafInstanceIterator1 : public LeafInstanceIterator
{
public:
  LeafInstanceIterator1(const Instance *inst,
                        const Network *network);
  bool hasNext() override { return next_; }
  Instance *next() override;

private:
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
Network::visitConnectedPins(const Pin *pin,
                            PinVisitor &visitor) const
{
  NetSet visited_nets(network_);
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
  NetSet visited_nets(this);
  visitConnectedPins(net, visitor, visited_nets);
}

void
Network::visitConnectedPins(const Net *net,
                            PinVisitor &visitor,
                            NetSet &visited_nets) const
{
  if (!visited_nets.contains(net)) {
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
      const Pin *pin = pin_iter->next();
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
  ConnectedPinIterator1(PinSet *pins);
  ~ConnectedPinIterator1() override;
  bool hasNext() override;
  const Pin *next() override;

protected:
  PinSet *pins_;
  PinSet::iterator pin_iter_;
};

ConnectedPinIterator1::ConnectedPinIterator1(PinSet *pins) :
  pins_(pins),
  pin_iter_(pins_->begin())
{
}

ConnectedPinIterator1::~ConnectedPinIterator1()
{
  delete pins_;
}

bool
ConnectedPinIterator1::hasNext()
{
  return pin_iter_ != pins_->end();
}

const Pin *
ConnectedPinIterator1::next()
{
  return *pin_iter_++;
}

class FindConnectedPins : public PinVisitor
{
public:
  FindConnectedPins(PinSet *pins);
  void operator()(const Pin *pin) override;

protected:
  PinSet *pins_;
};

FindConnectedPins::FindConnectedPins(PinSet *pins) :
  PinVisitor(),
  pins_(pins)
{
}

void
FindConnectedPins::operator()(const Pin *pin)
{
  pins_->insert(pin);
}

NetConnectedPinIterator *
Network::connectedPinIterator(const Net *net) const
{
  PinSet *pins = new PinSet(this);
  FindConnectedPins visitor(pins);
  visitConnectedPins(net, visitor);
  return new ConnectedPinIterator1(pins);
}

PinConnectedPinIterator *
Network::connectedPinIterator(const Pin *pin) const
{
  PinSet *pins = new PinSet(this);
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
    NetSet nets(this);
    return isConnected(net, pin, nets);
  }
}

bool
Network::isConnected(const Net *net,
                     const Pin *pin,
                     NetSet &nets) const
{
  if (!nets.contains(net)) {
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
      const Pin *pin1 = pin_iter->next();
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
  NetSet nets(this);
  return isConnected(net1, net2, nets);
}

bool
Network::isConnected(const Net *net1,
                     const Net *net2,
                     NetSet &nets) const
{
  if (net1 == net2)
    return true;
  else if (!nets.contains(net1)) {
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
      const Pin *pin1 = pin_iter->next();
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
  FindDrvrPins(PinSet *pins,
               const Network *network);
  void operator()(const Pin *pin) override;

protected:
  PinSet *pins_;
  const Network *network_;
};

FindDrvrPins::FindDrvrPins(PinSet *pins,
                           const Network *network) :
  PinVisitor(),
  pins_(pins),
  network_(network)
{
}

void
FindDrvrPins::operator()(const Pin *pin)
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
  deleteContents(net_drvr_pin_map_);
}

PinSet *
Network::drivers(const Net *net)
{
  PinSet *drvrs = findKey(net_drvr_pin_map_, net);
  if (drvrs == nullptr) {
    drvrs = new PinSet(this);
    FindDrvrPins visitor(drvrs, this);
    visitConnectedPins(net, visitor);
    net_drvr_pin_map_[net] = drvrs;
  }
  return drvrs;
}

////////////////////////////////////////////////////////////////

void
Network::pathNameFirst(std::string_view path_name,
                       std::string &first,
                       std::string &tail) const
{
  first.clear();
  tail.clear();

  char escape = pathEscape();
  char divider = pathDivider();
  size_t i = 0;
  while (i < path_name.size()) {
    size_t d = path_name.find(divider, i);
    while (d != std::string_view::npos && d > 0
           && path_name[d - 1] == escape)
      d = path_name.find(divider, d + 1);
    if (d != std::string_view::npos) {
      first = path_name.substr(0, d);
      tail = path_name.substr(d + 1);
      return;
    }
    break;
  }
}

void
Network::pathNameLast(std::string_view path_name,
                      std::string &head,
                      std::string &last) const
{
  head.clear();
  last.clear();

  char escape = pathEscape();
  char divider = pathDivider();

  size_t div_pos = path_name.rfind(divider);
  size_t path_end = path_name.size();
  while (div_pos > 0) {
    if (div_pos == std::string_view::npos)
      return;
    if (path_name[div_pos - 1] != escape) {
      // Found the last non-escaped divider.
      head = path_name.substr(0, div_pos);
      last = path_name.substr(div_pos + 1);
      return;
    }
    path_end = div_pos - 1;
    div_pos = path_name.rfind(divider, path_end);
  }
}

////////////////////////////////////////////////////////////////

NetworkEdit::NetworkEdit() :
  Network()
{
}

////////////////////////////////////////////////////////////////

NetworkConstantPinIterator::
NetworkConstantPinIterator(const Network *network,
                           NetSet &zero_nets,
                           NetSet &one_nets) :
  ConstantPinIterator(),
  network_(network),
  constant_pins_{PinSet(network), PinSet(network)}
{
  findConstantPins(zero_nets, constant_pins_[0]);
  findConstantPins(one_nets, constant_pins_[1]);
  value_ = LogicValue::zero;
  pin_iter_ = constant_pins_[0].begin();
}

void
NetworkConstantPinIterator::findConstantPins(NetSet &nets,
                                             PinSet &pins)
{
  for (const Net *net : nets) {
    NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      pins.insert(pin);
    }
    delete pin_iter;
  }
}

bool
NetworkConstantPinIterator::hasNext()
{
  if (pin_iter_ != constant_pins_[(int)value_].end())
    return true;
  else if (value_ == LogicValue::zero) {
    value_ = LogicValue::one;
    pin_iter_ = constant_pins_[1].begin();
    return pin_iter_ != constant_pins_[1].end();
  }
  else
    return false;
}

void
NetworkConstantPinIterator::next(const Pin *&pin,
                                 LogicValue &value)
{
  pin = *pin_iter_++;
  value = value_;
}

////////////////////////////////////////////////////////////////

FindNetDrvrLoads::FindNetDrvrLoads(const Pin *drvr_pin,
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
FindNetDrvrLoads::operator()(const Pin *pin)
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
    const Pin *above_pin = pin_iter->next();
    if (above_pin != hpin) {
      if (network->isDriver(above_pin))
        above_drvrs.insert(above_pin);
      if (network->isLoad(above_pin))
        above_loads.insert(above_pin);
      Term *above_term = network->term(above_pin);
      if (above_term) {
        Net *above_net1 = network->net(above_term);
        if (above_net1 && !visited.contains(above_net1))
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
      if (above_net1 && !visited.contains(above_net1))
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
    const Pin *below_pin = pin_iter->next();
    if (below_pin != hpin) {
      NetSet visited_above(network);
      if (network->isDriver(below_pin))
        below_drvrs.insert(below_pin);
      if (network->isLoad(below_pin))
        below_loads.insert(below_pin);
      if (network->isHierarchical(below_pin)) {
        Term *term = network->term(below_pin);
        if (term) {
          Net *below_net1 = network->net(term);
          if (below_net1 && !visited.contains(below_net1))
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
  for (const Pin *drvr : drvrs) {
    for (const Pin *load : loads)
      visitor->visit(drvr, load);
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
        NetSet visited(network);
        PinSet above_drvrs(network);
        PinSet above_loads(network);
        visitPinsAboveNet1(hpin, above_net, visited,
                           above_drvrs, above_loads, network);
        PinSet below_drvrs(network);
        PinSet below_loads(network);
        visitPinsBelowNet1(hpin, below_net, visited,
                           below_drvrs, below_loads, network);
        visitDrvrLoads(above_drvrs, below_loads, visitor);
        visitDrvrLoads(below_drvrs, above_loads, visitor);
      }
    }
  }
}

void
visitDrvrLoadsThruNet(const Net *net,
                      const Network *network,
                      HierPinThruVisitor *visitor)
{
  NetSet visited(network);
  PinSet above_drvrs(network);
  PinSet above_loads(network);
  PinSet below_drvrs(network);
  PinSet below_loads(network);
  PinSet net_drvrs(network);
  PinSet net_loads(network);
  NetPinIterator *pin_iter = network->pinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
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

////////////////////////////////////////////////////////////////

CellIdLess::CellIdLess(const Network *network) :
  network_(network)
{
}

bool
CellIdLess::operator()(const Cell *cell1,
                       const Cell *cell2) const
{
  return network_->id(cell1) < network_->id(cell2);
}

PortIdLess::PortIdLess(const Network *network) :
  network_(network)
{
}

bool
PortIdLess::operator()(const Port *port1,
                      const Port *port2) const
{
  return network_->id(port1) < network_->id(port2);
}

InstanceIdLess::InstanceIdLess(const Network *network) :
  network_(network)
{
}

bool
InstanceIdLess::operator()(const Instance *inst1,
                           const Instance *inst2) const
{
  return network_->id(inst1) < network_->id(inst2);
}

PinIdLess::PinIdLess(const Network *network) :
  network_(network)
{
}

bool
PinIdLess::operator()(const Pin *pin1,
                      const Pin *pin2) const
{
  return network_->id(pin1) < network_->id(pin2);
}

PinIdHash::PinIdHash(const Network *network) :
  network_(network)
{
}

size_t
PinIdHash::operator()(const Pin *pin) const
{
  return network_->id(pin);
}

NetIdLess::NetIdLess(const Network *network) :
  network_(network)
{
}

bool
NetIdLess::operator()(const Net *net1,
                      const Net *net2) const
{
  return network_->id(net1) < network_->id(net2);
}

////////////////////////////////////////////////////////////////

CellSet::CellSet(const Network *network) :
  std::set<const Cell*, CellIdLess>(CellIdLess(network))
{
}

PortSet::PortSet(const Network *network) :
  std::set<const Port*, PortIdLess>(PortIdLess(network))
{
}

InstanceSet::InstanceSet() :
  std::set<const Instance*, InstanceIdLess>(InstanceIdLess(nullptr))
{
}

InstanceSet::InstanceSet(const Network *network) :
  std::set<const Instance*, InstanceIdLess>(InstanceIdLess(network))
{
}

PinSet::PinSet() :
  std::set<const Pin*, PinIdLess>(PinIdLess(nullptr))
{
}

PinSet::PinSet(const Network *network) :
  std::set<const Pin*, PinIdLess>(PinIdLess(network))
{
}

NetSet::NetSet() :
  std::set<const Net*, NetIdLess>(NetIdLess(nullptr))
{
}

NetSet::NetSet(const Network *network) :
  std::set<const Net*, NetIdLess>(NetIdLess(network))
{
}

} // namespace sta
