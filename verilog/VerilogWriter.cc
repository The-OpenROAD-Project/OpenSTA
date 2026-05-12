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

#include "VerilogWriter.hh"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>

#include "Error.hh"
#include "Format.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "ParseBus.hh"
#include "PortDirection.hh"
#include "VerilogNamespace.hh"

namespace sta {

class VerilogWriter
{
public:
  VerilogWriter(const char *filename,
                bool include_pwr_gnd,
                CellSeq *remove_cells,
                FILE *stream,
                Network *network);
  void writeModules();

protected:
  void writeModule(const Instance *inst);
  InstanceSeq findHierChildren();
  void findHierChildren(const Instance *inst,
                        InstanceSeq &children,
                        CellSet &cells);
  void writePorts(const Cell *cell);
  void writePortDcls(const Cell *cell);
  void writeWireDcls(const Instance *inst);
  const char *verilogPortDir(PortDirection *dir);
  void writeChildren(const Instance *inst);
  void writeChild(const Instance *child);
  void writeInstPin(const Instance *inst,
                    const Port *port,
                    bool &first_port);
  void writeInstBusPin(const Instance *inst,
                       const Port *port,
                       bool &first_port);
  void writeInstBusPinBit(const Instance *inst,
                          const Port *port,
                          bool &first_member);
  void writeAssigns(const Instance *inst);

  int findUnconnectedNetCount(const Instance *inst);
  int findChildNCcount(const Instance *child);
  int findPortNCcount(const Instance *inst,
                      const Port *port);

  const char *filename_;
  bool include_pwr_gnd_;
  CellSet remove_cells_;
  FILE *stream_;
  Network *network_;
  int unconnected_net_index_{1};
};

void
writeVerilog(const char *filename,
             bool include_pwr_gnd,
             CellSeq *remove_cells,
             Network *network)
{
  if (network->topInstance()) {
    FILE *stream = fopen(filename, "w");
    if (stream) {
      VerilogWriter writer(filename, include_pwr_gnd,
                           remove_cells, stream, network);
      writer.writeModules();
      fclose(stream);
    }
    else
      throw FileNotWritable(filename);
  }
}

VerilogWriter::VerilogWriter(const char *filename,
                             bool include_pwr_gnd,
                             CellSeq *remove_cells,
                             FILE *stream,
                             Network *network) :
  filename_(filename),
  include_pwr_gnd_(include_pwr_gnd),
  remove_cells_(network),
  stream_(stream),
  network_(network)
{
  if (remove_cells) {
    for(Cell *lib_cell : *remove_cells)
      remove_cells_.insert(lib_cell);
  }
}

void
VerilogWriter::writeModules()
{
  // Write the top level modeule first.
  writeModule(network_->topInstance());
  InstanceSeq hier_childrenn = findHierChildren();
  for (const Instance *child : hier_childrenn)
    writeModule(child);
}

InstanceSeq
VerilogWriter::findHierChildren()
{
  InstanceSeq children;
  CellSet cells(network_);
  findHierChildren(network_->topInstance(), children, cells);

  sort(children, [this](const Instance *inst1,
                        const Instance *inst2) {
    std::string cell_name1 = network_->cellName(inst1);
    std::string cell_name2 = network_->cellName(inst2);
    return cell_name1 < cell_name2;
  });

  return children;
}

void
VerilogWriter::findHierChildren(const Instance *inst,
                                InstanceSeq &children,
                                CellSet &cells)
{
  InstanceChildIterator *child_iter = network_->childIterator(inst);
  while (child_iter->hasNext()) {
    const Instance *child = child_iter->next();
    const Cell *cell = network_->cell(child);
    if (network_->isHierarchical(child)
        && !cells.contains(cell)) {
      children.push_back(child);
      cells.insert(cell);
      findHierChildren(child, children, cells);
    }
  }
  delete child_iter;
}

void
VerilogWriter::writeModule(const Instance *inst)
{
  Cell *cell = network_->cell(inst);
  std::string cell_vname = cellVerilogName(std::string(network_->name(cell)));
  sta::print(stream_, "module {} (", cell_vname);
  writePorts(cell);
  writePortDcls(cell);
  sta::print(stream_, "\n");
  writeWireDcls(inst);
  sta::print(stream_, "\n");
  writeChildren(inst);
  writeAssigns(inst);
  sta::print(stream_, "endmodule\n");
}

void
VerilogWriter::writePorts(const Cell *cell)
{
  bool first = true;
  CellPortIterator *port_iter = network_->portIterator(cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    if (include_pwr_gnd_
        || !network_->direction(port)->isPowerGround()) {
      if (!first)
        sta::print(stream_, ",\n    ");
      std::string verilog_name = portVerilogName(std::string(network_->name(port)));
      sta::print(stream_, "{}", verilog_name);
      first = false;
    }
  }
  delete port_iter;
  sta::print(stream_, ");\n");
}

void
VerilogWriter::writePortDcls(const Cell *cell)
{
  CellPortIterator *port_iter = network_->portIterator(cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    PortDirection *dir = network_->direction(port);
    if (include_pwr_gnd_
        || !network_->direction(port)->isPowerGround()) {
      std::string port_vname = portVerilogName(std::string(network_->name(port)));
      const char *vtype = verilogPortDir(dir);
      if (vtype) {
        sta::print(stream_, " {}", vtype);
        if (network_->isBus(port))
          sta::print(stream_, " [{}:{}]",
                    network_->fromIndex(port),
                    network_->toIndex(port));
        sta::print(stream_, " {};\n", port_vname);
        if (dir->isTristate()) {
          sta::print(stream_, " tri");
          if (network_->isBus(port))
            sta::print(stream_, " [{}:{}]",
                      network_->fromIndex(port),
                      network_->toIndex(port));
          sta::print(stream_, " {};\n", port_vname);
        }
      }
    }
  }
  delete port_iter;
}

const char *
VerilogWriter::verilogPortDir(PortDirection *dir)
{
  if (dir == PortDirection::input())
    return "input";
  else if (dir == PortDirection::output())
    return "output";
  else if (dir == PortDirection::tristate())
    return "output";
  else if (dir == PortDirection::bidirect())
    return "inout";
  else if (dir == PortDirection::power())
    return "inout";
  else if (dir == PortDirection::ground())
    return "inout";
  else if (dir == PortDirection::well())
    return "inout";
  else if (dir == PortDirection::internal()
           || dir == PortDirection::unknown())
    return "inout";
  else {
    criticalError(268, "unknown port direction");
    return nullptr;
  }
}

using BusIndexRange = std::pair<int, int>;

void
VerilogWriter::writeWireDcls(const Instance *inst)
{
  Cell *cell = network_->cell(inst);
  char escape = network_->pathEscape();
  std::map<std::string, BusIndexRange, std::less<std::string>> bus_ranges;
  NetIterator *net_iter = network_->netIterator(inst);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    if (include_pwr_gnd_
        || !(network_->isPower(net) || network_->isGround(net))) {
      std::string net_name = network_->name(net);
      if (network_->findPort(cell, net_name) == nullptr) {
        if (isBusName(net_name, '[', ']', escape)) {
          bool is_bus;
          std::string bus_name;
          int index;
          parseBusName(net_name, '[', ']', escape, is_bus, bus_name, index);
          BusIndexRange &range = bus_ranges[bus_name];
          range.first = std::max(range.first, index);
          range.second = std::min(range.second, index);
        }
        else {
          std::string net_vname = netVerilogName(std::string(net_name));
          sta::print(stream_, " wire {};\n", net_vname);
        }
      }
    }
  }
  delete net_iter;

  for (const auto& [bus_name, range] : bus_ranges) {
    std::string net_vname = netVerilogName(bus_name);
    sta::print(stream_, " wire [{}:{}] {};\n",
               range.first,
               range.second,
               net_vname);
  }

  // Wire net dcls for writeInstBusPinBit.
  int nc_count = findUnconnectedNetCount(inst);
  for (int i = 1; i < nc_count + 1; i++)
    sta::print(stream_, " wire _NC{};\n", i);
}

void
VerilogWriter::writeChildren(const Instance *inst)
{
  std::vector<Instance*> children;
  InstanceChildIterator *child_iter = network_->childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    children.push_back(child);
  }
  delete child_iter;

  sort(children, [this](const Instance *inst1,
                        const Instance *inst2) {
    return network_->name(inst1) < network_->name(inst2);
  });

  for (auto child : children)
    writeChild(child);
}

void
VerilogWriter::writeChild(const Instance *child)
{
  Cell *child_cell = network_->cell(child);
  if (!remove_cells_.contains(child_cell)) {
    std::string child_name = network_->name(child);
    std::string child_vname = instanceVerilogName(std::string(child_name));
    std::string child_cell_vname = cellVerilogName(std::string(network_->name(child_cell)));
    sta::print(stream_, " {} {} (",
               child_cell_vname,
               child_vname);
    bool first_port = true;
    CellPortIterator *port_iter = network_->portIterator(child_cell);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      if (include_pwr_gnd_
          || !network_->direction(port)->isPowerGround()) {
        if (network_->hasMembers(port))
          writeInstBusPin(child, port, first_port);
        else
          writeInstPin(child, port, first_port);
      }
    }
    delete port_iter;
    sta::print(stream_, ");\n");
  }
}

void
VerilogWriter::writeInstPin(const Instance *inst,
                            const Port *port,
                            bool &first_port)
{
  Pin *pin = network_->findPin(inst, port);
  if (pin) {
    Net *net = network_->net(pin);
    if (net) {
      std::string net_name = network_->name(net);
      std::string net_vname = netVerilogName(std::string(net_name));
      if (!first_port)
        sta::print(stream_, ",\n    ");
      std::string port_vname = portVerilogName(std::string(network_->name(port)));
      sta::print(stream_, ".{}({})",
                 port_vname,
                 net_vname);
      first_port = false;
    }
  }
}

void
VerilogWriter::writeInstBusPin(const Instance *inst,
                               const Port *port,
                               bool &first_port)
{
  if (!first_port)
    sta::print(stream_, ",\n    ");

  std::string port_vname = portVerilogName(std::string(network_->name(port)));
  sta::print(stream_, ".{}({{", port_vname);
  first_port = false;
  bool first_member = true;

  // Match the member order of the liberty cell if it exists.
  LibertyPort *lib_port = network_->libertyPort(port);
  if (lib_port) {
    Cell *cell = network_->cell(inst);
    LibertyPortMemberIterator member_iter(lib_port);
    while (member_iter.hasNext()) {
      LibertyPort *lib_member = member_iter.next();
      Port *member = network_->findPort(cell, lib_member->name());
      writeInstBusPinBit(inst, member, first_member);
    }
  }
  else {
    PortMemberIterator *member_iter = network_->memberIterator(port);
    while (member_iter->hasNext()) {
      Port *member = member_iter->next();
      writeInstBusPinBit(inst, member, first_member);
    }
    delete member_iter;
  }
  sta::print(stream_, "}})");
}

void
VerilogWriter::writeInstBusPinBit(const Instance *inst,
                                  const Port *port,
                                  bool &first_member)
{
  Pin *pin = network_->findPin(inst, port);
  Net *net = pin ? network_->net(pin) : nullptr;
  std::string net_name = net
    ? std::string(network_->name(net))
    // There is no verilog syntax to "skip" a bit in the concatentation.
    : sta::format("_NC{}", unconnected_net_index_++);

  std::string net_vname = netVerilogName(net_name);
  if (!first_member)
    sta::print(stream_, ",\n    ");
  sta::print(stream_, "{}", net_vname);
  first_member = false;
}

// Verilog "ports" are not distinct from nets.
// Use an assign statement to alias the net when it is connected to
// multiple output ports.
void
VerilogWriter::writeAssigns(const Instance *inst)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    Term *term = network_->term(pin);
    if (term) {
      Net *net = network_->net(term);
      Port *port = network_->port(pin);
      if (net
          && port
          && (include_pwr_gnd_
              || !(network_->isPower(net) || network_->isGround(net)))
          && (network_->direction(port)->isAnyOutput()
              || (include_pwr_gnd_ && network_->direction(port)->isPowerGround()))
          && network_->name(port) != network_->name(net)) {
        // Port name is different from net name.
        std::string port_vname = netVerilogName(std::string(network_->name(port)));
        std::string net_vname = netVerilogName(std::string(network_->name(net)));
        sta::print(stream_, " assign {} = {};\n",
                   port_vname,
                   net_vname);
      }
    }
  }
  delete pin_iter;
}

////////////////////////////////////////////////////////////////

int
VerilogWriter::findUnconnectedNetCount(const Instance *inst)
{
  int nc_count = 0;
  InstanceChildIterator *child_iter = network_->childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    nc_count += findChildNCcount(child);
  }
  delete child_iter;
  return nc_count;
}

int
VerilogWriter::findChildNCcount(const Instance *child)
{
  int nc_count = 0;
  Cell *child_cell = network_->cell(child);
  if (!remove_cells_.contains(child_cell)) {
    CellPortIterator *port_iter = network_->portIterator(child_cell);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      if (network_->hasMembers(port))
        nc_count += findPortNCcount(child, port);
    }
    delete port_iter;
  }
  return nc_count;
}

int
VerilogWriter::findPortNCcount(const Instance *inst,
                               const Port *port)
{
  int nc_count = 0;
  PortMemberIterator *member_iter = network_->memberIterator(port);
  while (member_iter->hasNext()) {
    Port *member = member_iter->next();
    Pin *pin = network_->findPin(inst, member);
    if (pin == nullptr
        || network_->net(pin) == nullptr)
      nc_count++;
  }
  delete member_iter;
  return nc_count;
}

} // namespace sta
