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

#include "VerilogWriter.hh"

#include <cstdlib>
#include <algorithm>

#include "Error.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "VerilogNamespace.hh"
#include "ParseBus.hh"

namespace sta {

using std::min;
using std::max;

class VerilogWriter
{
public:
  VerilogWriter(const char *filename,
		bool sort,
		bool include_pwr_gnd_pins,
		CellSeq *remove_cells,
		FILE *stream,
		Network *network);
  void writeModule(Instance *inst);

protected:
  void writePorts(Cell *cell);
  void writePortDcls(Cell *cell);
  void writeWireDcls(Instance *inst);
  const char *verilogPortDir(PortDirection *dir);
  void writeChildren(Instance *inst);
  void writeChild(Instance *child);
  void writeInstPin(Instance *inst,
		    Port *port,
		    bool &first_port);
  void writeInstBusPin(Instance *inst,
		       Port *port,
		       bool &first_port);
  void writeInstBusPinBit(Instance *inst,
			  Port *port,
			  bool &first_member);
  void writeAssigns(Instance *inst);

  int findUnconnectedNetCount();
  int findNCcount(Instance *inst);
  int findChildNCcount(Instance *child);
  int findPortNCcount(Instance *inst,
                      Port *port);

  const char *filename_;
  bool sort_;
  bool include_pwr_gnd_;
  CellSet remove_cells_;
  FILE *stream_;
  Network *network_;

  CellSet written_cells_;
  Vector<Instance*> pending_children_;
  int unconnected_net_index_;
};

void
writeVerilog(const char *filename,
	     bool sort,
	     bool include_pwr_gnd_pins,
	     CellSeq *remove_cells,
	     Network *network)
{
  if (network->topInstance()) {
    FILE *stream = fopen(filename, "w");
    if (stream) {
      VerilogWriter writer(filename, sort, include_pwr_gnd_pins,
			   remove_cells, stream, network);
      writer.writeModule(network->topInstance());
      fclose(stream);
    }
    else
      throw FileNotWritable(filename);
  }
}

VerilogWriter::VerilogWriter(const char *filename,
			     bool sort,
			     bool include_pwr_gnd_pins,
			     CellSeq *remove_cells,
			     FILE *stream,
			     Network *network) :
  filename_(filename),
  sort_(sort),
  include_pwr_gnd_(include_pwr_gnd_pins),
  remove_cells_(network),
  stream_(stream),
  network_(network),
  written_cells_(network),
  unconnected_net_index_(1)
{
  if (remove_cells) {
    for(Cell *lib_cell : *remove_cells)
      remove_cells_.insert(lib_cell);
  }
}

void
VerilogWriter::writeModule(Instance *inst)
{
  Cell *cell = network_->cell(inst);
  fprintf(stream_, "module %s (",
	  network_->name(cell));
  writePorts(cell);
  writePortDcls(cell);
  fprintf(stream_, "\n");
  writeWireDcls(inst);
  fprintf(stream_, "\n");
  writeChildren(inst);
  writeAssigns(inst);
  fprintf(stream_, "endmodule\n");
  written_cells_.insert(cell);

  if (sort_)
    sort(pending_children_, [this](const Instance *inst1,
                                   const Instance *inst2) {
      return stringLess(network_->cellName(inst1), network_->cellName(inst2));
    });
  for (auto child : pending_children_) {
    Cell *child_cell = network_->cell(child);
    if (!written_cells_.hasKey(child_cell))
      writeModule(child);
  }
}

void
VerilogWriter::writePorts(Cell *cell)
{
  bool first = true;
  CellPortIterator *port_iter = network_->portIterator(cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    if (include_pwr_gnd_
        || !network_->direction(port)->isPowerGround()) {
      if (!first)
        fprintf(stream_, ",\n    ");
      string verillg_name = portVerilogName(network_->name(port),
                                            network_->pathEscape());
      fprintf(stream_, "%s", verillg_name.c_str());
      first = false;
    }
  }
  delete port_iter;
  fprintf(stream_, ");\n");
}

void
VerilogWriter::writePortDcls(Cell *cell)
{
  CellPortIterator *port_iter = network_->portIterator(cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    PortDirection *dir = network_->direction(port);
    if (include_pwr_gnd_
        || !network_->direction(port)->isPowerGround()) {
      string port_vname = portVerilogName(network_->name(port),
                                          network_->pathEscape());
      const char *vtype = verilogPortDir(dir);
      if (vtype) {
        fprintf(stream_, " %s", vtype);
        if (network_->isBus(port))
          fprintf(stream_, " [%d:%d]",
                  network_->fromIndex(port),
                  network_->toIndex(port));
        fprintf(stream_, " %s;\n", port_vname.c_str());
        if (dir->isTristate()) {
          fprintf(stream_, " tri");
          if (network_->isBus(port))
            fprintf(stream_, " [%d:%d]",
                    network_->fromIndex(port),
                    network_->toIndex(port));
          fprintf(stream_, " %s;\n", port_vname.c_str());
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
    return "input";
  else if (dir == PortDirection::ground())
    return "input";
  else if (dir == PortDirection::internal())
    return nullptr;
  else {
    criticalError(268, "unknown port direction");
    return nullptr;
  }
}

typedef std::pair<int, int> BusIndexRange;

void
VerilogWriter::writeWireDcls(Instance *inst)
{
  Cell *cell = network_->cell(inst);
  char escape = network_->pathEscape();
  Map<string, BusIndexRange, std::less<string>> bus_ranges;
  NetIterator *net_iter = network_->netIterator(inst);
  while (net_iter->hasNext()) {
    Net *net = net_iter->next();
    const char *net_name = network_->name(net);
    if (network_->findPort(cell, net_name) == nullptr) {
      if (isBusName(net_name, '[', ']', escape)) {
        bool is_bus;
        string bus_name;
        int index;
        parseBusName(net_name, '[', ']', escape, is_bus, bus_name, index);
        BusIndexRange &range = bus_ranges[bus_name];
        range.first = max(range.first, index);
        range.second = min(range.second, index);
      }
      else {
        string net_vname = netVerilogName(net_name, network_->pathEscape());
        fprintf(stream_, " wire %s;\n", net_vname.c_str());;
      }
    }
  }
  delete net_iter;

  for (auto name_range : bus_ranges) {
    const char *bus_name = name_range.first.c_str();
    const BusIndexRange &range = name_range.second;
    string net_vname = netVerilogName(bus_name, network_->pathEscape());
    fprintf(stream_, " wire [%d:%d] %s;\n",
            range.first,
            range.second,
            net_vname.c_str());;
  }

  // Wire net dcls for writeInstBusPinBit.
  int nc_count = findUnconnectedNetCount();
  for (int i = 1; i < nc_count + 1; i++)
    fprintf(stream_, " wire _NC%d;\n", i);
}

void
VerilogWriter::writeChildren(Instance *inst)			     
{
  Vector<Instance*> children;
  InstanceChildIterator *child_iter = network_->childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    children.push_back(child);
    if (network_->isHierarchical(child)) {
      pending_children_.push_back(child);
    }
  }
  delete child_iter;

  if (sort_)
    sort(children, [this](const Instance *inst1,
                          const Instance *inst2) {
      return stringLess(network_->name(inst1), network_->name(inst2));
    });

  for (auto child : children)
    writeChild(child);
}

void
VerilogWriter::writeChild(Instance *child)
{
  Cell *child_cell = network_->cell(child);
  if (!remove_cells_.hasKey(child_cell)) {
    const char *child_name = network_->name(child);
    string child_vname = instanceVerilogName(child_name, network_->pathEscape());
    fprintf(stream_, " %s %s (",
	    network_->name(child_cell),
	    child_vname.c_str());
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
    fprintf(stream_, ");\n");
  }
}

void
VerilogWriter::writeInstPin(Instance *inst,
			    Port *port,
			    bool &first_port)
{
  Pin *pin = network_->findPin(inst, port);
  if (pin) {
    Net *net = network_->net(pin);
    if (net) {
      const char *net_name = network_->name(net);
      string net_vname = netVerilogName(net_name, network_->pathEscape());
      if (!first_port)
	fprintf(stream_, ",\n    ");
      string port_vname = portVerilogName(network_->name(port),
                                          network_->pathEscape());
      fprintf(stream_, ".%s(%s)",
	      port_vname.c_str(),
	      net_vname.c_str());
      first_port = false;
    }
  }
}

void
VerilogWriter::writeInstBusPin(Instance *inst,
			       Port *port,
			       bool &first_port)
{
  if (!first_port)
    fprintf(stream_, ",\n    ");

  fprintf(stream_, ".%s({", network_->name(port));
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
  fprintf(stream_, "})");
}

void
VerilogWriter::writeInstBusPinBit(Instance *inst,
				  Port *port,
				  bool &first_member)
{
  Pin *pin = network_->findPin(inst, port);
  Net *net = pin ? network_->net(pin) : nullptr;
  string net_name;
  if (net)
    net_name = network_->name(net);
  else
    // There is no verilog syntax to "skip" a bit in the concatentation.
    stringPrint(net_name, "_NC%d", unconnected_net_index_++);
  string net_vname = netVerilogName(net_name.c_str(), network_->pathEscape());
  if (!first_member)
    fprintf(stream_, ",\n    ");
  fprintf(stream_, "%s", net_vname.c_str());
  first_member = false;
}

// Verilog "ports" are not distinct from nets.
// Use an assign statement to alias the net when it is connected to
// multiple output ports.
void
VerilogWriter::writeAssigns(Instance *inst)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    Term *term = network_->term(pin);
    Net *net = network_->net(term);
    Port *port = network_->port(pin);
    if (port
        && network_->direction(port)->isAnyOutput()
        && !stringEqual(network_->name(port), network_->name(net))) {
      // Port name is different from net name.
      string port_vname = netVerilogName(network_->name(port),
                                         network_->pathEscape());
      string net_vname = netVerilogName(network_->name(net),
                                        network_->pathEscape());
      fprintf(stream_, " assign %s = %s;\n",
              port_vname.c_str(),
              net_vname.c_str());
    }
  }
  delete pin_iter;
}

////////////////////////////////////////////////////////////////

// Walk the hierarch counting unconnected nets used to connect to
// bus ports with concatenation.
int
VerilogWriter::findUnconnectedNetCount()
{
  return findNCcount(network_->topInstance());
}

int
VerilogWriter::findNCcount(Instance *inst)
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
VerilogWriter::findChildNCcount(Instance *child)
{
  int nc_count = 0;
  Cell *child_cell = network_->cell(child);
  if (!remove_cells_.hasKey(child_cell)) {
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
VerilogWriter::findPortNCcount(Instance *inst,
                               Port *port)
{
  int nc_count = 0;
  LibertyPort *lib_port = network_->libertyPort(port);
  if (lib_port) {
    Cell *cell = network_->cell(inst);
    LibertyPortMemberIterator member_iter(lib_port);
    while (member_iter.hasNext()) {
      LibertyPort *lib_member = member_iter.next();
      Port *member = network_->findPort(cell, lib_member->name());
      Pin *pin = network_->findPin(inst, member);
      if (pin == nullptr
          || network_->net(pin) == nullptr)
        nc_count++;
    }
  }
  return nc_count;
}

} // namespace
