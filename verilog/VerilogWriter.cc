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

#include <stdlib.h>
#include "Machine.hh"
#include "Error.hh"
#include "PortDirection.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "VerilogNamespace.hh"
#include "VerilogWriter.hh"

namespace sta {

class VerilogWriter
{
public:
  VerilogWriter(const char *filename,
		bool sort,
		FILE *stream,
		Network *network);
  void writeModule(Instance *inst);

protected:
  void writePorts(Cell *cell);
  void writePortDcls(Cell *cell);
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

  const char *filename_;
  bool sort_;
  FILE *stream_;
  Network *network_;

  Set<Cell*> written_cells_;
  Set<Instance*> pending_children_;
  int unconnected_net_index_;
};

void
writeVerilog(const char *filename,
	     bool sort,
	     Network *network)
{
  if (network->topInstance()) {
    FILE *stream = fopen(filename, "w");
    if (stream) {
      VerilogWriter writer(filename, sort, stream, network);
      writer.writeModule(network->topInstance());
      fclose(stream);
    }
    else
      throw FileNotWritable(filename);
  }
}

VerilogWriter::VerilogWriter(const char *filename,
			     bool sort,
			     FILE *stream,
			     Network *network) :
  filename_(filename),
  sort_(sort),
  stream_(stream),
  network_(network),
  unconnected_net_index_(1)
{
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
  writeChildren(inst);
  fprintf(stream_, "endmodule\n");
  written_cells_.insert(cell);

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
    if (!first)
      fprintf(stream_, ",\n    ");
    fprintf(stream_, "%s", portVerilogName(network_->name(port),
					   network_->pathEscape()));
    first = false;
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
    const char *port_name = portVerilogName(network_->name(port),
					    network_->pathEscape());
    const char *vtype = verilogPortDir(dir);
    if (vtype) {
      fprintf(stream_, " %s", vtype);
      if (network_->isBus(port))
	fprintf(stream_, " [%d:%d]",
		network_->fromIndex(port),
		network_->toIndex(port));
      fprintf(stream_, " %s;\n", port_name);
      if (dir->isTristate()) {
	fprintf(stream_, " tri");
	if (network_->isBus(port))
	  fprintf(stream_, " [%d:%d]",
		  network_->fromIndex(port),
		  network_->toIndex(port));
	fprintf(stream_, " %s;\n", port_name);
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
  else if (dir == PortDirection::bidirect())
    return "inout";
  else if (dir == PortDirection::tristate())
    return "output";
  else
    return nullptr;
}

void
VerilogWriter::writeChildren(Instance *inst)			     
{
  Vector<Instance*> children;
  InstanceChildIterator *child_iter = network_->childIterator(inst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    children.push_back(child);
    if (network_->isHierarchical(child))
      pending_children_.insert(child);
  }
  delete child_iter;

  if (sort_)
    sort(children, InstancePathNameLess(network_));

  for (auto child : children)
    writeChild(child);
}

void
VerilogWriter::writeChild(Instance *child)
{
  Cell *child_cell = network_->cell(child);
  const char *child_name = network_->name(child);
  const char *child_vname = instanceVerilogName(child_name,
						network_->pathEscape());
  fprintf(stream_, " %s %s (",
	  network_->name(child_cell),
	  child_vname);
  bool first_port = true;
  CellPortIterator *port_iter = network_->portIterator(child_cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    if (network_->hasMembers(port)) 
      writeInstBusPin(child, port, first_port);
    else 
      writeInstPin(child, port, first_port);
  }
  delete port_iter;
  fprintf(stream_, ");\n");
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
      const char *net_vname = netVerilogName(net_name, network_->pathEscape());
      if (!first_port)
	fprintf(stream_, ",\n    ");
      const char *port_name = portVerilogName(network_->name(port),
					      network_->pathEscape());
      fprintf(stream_, ".%s(%s)",
	      port_name,
	      net_vname);
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
  const char *net_name = nullptr;
  if (pin) {
    Net *net = network_->net(pin);
    if (net)
      net_name = network_->name(net);
  }
  if (net_name == nullptr)
    // There is no verilog syntax to "skip" a bit in the concatentation.
    net_name = stringPrintTmp("_NC%d", unconnected_net_index_++);
  const char *net_vname = netVerilogName(net_name, network_->pathEscape());
  if (!first_member)
    fprintf(stream_, ",\n    ");
  fprintf(stream_, "%s", net_vname);
  first_member = false;
}

} // namespace
