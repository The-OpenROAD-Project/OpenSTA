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

#include "VerilogReader.hh"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "ContainerHelpers.hh"
#include "Debug.hh"
#include "Error.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Report.hh"
#include "Stats.hh"
#include "StringUtil.hh"
#include "VerilogNamespace.hh"
#include "Zlib.hh"
#include "verilog/VerilogReaderPvt.hh"
#include "verilog/VerilogScanner.hh"

namespace sta {

using VerilogConstant10 = std::uint64_t;

static std::string
verilogBusBitName(std::string_view bus_name,
                  int index);
static int
hierarchyLevel(Net *net,
               Network *network);

VerilogReader *
makeVerilogReader(NetworkReader *network)
{
  return new VerilogReader(network);
}

bool
readVerilogFile(std::string_view filename,
                VerilogReader *verilog_reader)
{
  return verilog_reader->read(filename);
}

void
deleteVerilogReader(VerilogReader *verilog_reader)
{
  delete verilog_reader;
}

////////////////////////////////////////////////////////////////

VerilogError::VerilogError(int id,
                           std::string_view filename,
                           int line,
                           std::string_view msg,
                           bool warn) :
  id_(id),
  filename_(filename),
  line_(line),
  msg_(msg),
  warn_(warn)
{
}

class VerilogErrorCmp
{
public:
  bool operator()(const VerilogError *error1,
                  const VerilogError *error2) const
  {
    int file_cmp = error1->filename_.compare(error2->filename_);
    if (file_cmp == 0) {
      if (error1->line_ == error2->line_)
        return error1->msg_ < error2->msg_;
      else
        return error1->line_ < error2->line_;
    }
    else
      return file_cmp < 0;
  }
};

////////////////////////////////////////////////////////////////

VerilogReader::VerilogReader(NetworkReader *network) :
  report_(network->report()),
  debug_(network->debug()),
  network_(network),
  zero_net_name_("zero_"),
  one_net_name_("one_")
{
  network->setLinkFunc([this](std::string_view top_cell_name,
                              bool make_black_boxes) -> Instance * {
    return linkNetwork(top_cell_name, make_black_boxes, true);
  });
  constant10_max_ = std::to_string(std::numeric_limits<VerilogConstant10>::max());
}

VerilogReader::~VerilogReader()
{
  deleteModules();
}

void
VerilogReader::deleteModules()
{
  deleteContents(module_map_);
}

bool
VerilogReader::read(std::string_view filename)
{
  gzstream::igzstream stream(std::string(filename).c_str());
  if (stream.is_open()) {
    Stats stats(debug_, report_);
    VerilogScanner scanner(&stream, filename, report_);
    VerilogParse parser(&scanner, this);
    init(filename);
    bool success = (parser.parse() == 0);
    stats.report("Read verilog");
    return success;
  }
  else
    throw FileNotReadable(filename);
}

void
VerilogReader::init(std::string_view filename)
{
  filename_ = filename;

  library_ = network_->findLibrary("verilog");
  if (library_ == nullptr)
    library_ = network_->makeLibrary("verilog", "");
}

VerilogModule *
VerilogReader::module(Cell *cell)
{
  return findKey(module_map_, cell);
}

void
VerilogReader::makeModule(std::string_view module_vname,
                          VerilogNetSeq *ports,
                          VerilogStmtSeq *stmts,
                          VerilogAttrStmtSeq *attr_stmts,
                          int line)
{
  const std::string module_name = moduleVerilogToSta(module_vname);
  Cell *cell = network_->findCell(library_, module_name);
  if (cell) {
    VerilogModule *module = module_map_[cell];
    delete module;
    module_map_.erase(cell);
    network_->deleteCell(cell);
  }

  VerilogModule *module = new VerilogModule(module_name, ports, stmts,
                                            attr_stmts, filename_, line, this);
  cell = network_->makeCell(library_, module_name, false, filename_);

  if (attr_stmts) {
    for (VerilogAttrStmt *stmt : *attr_stmts) {
      for (VerilogAttrEntry *entry : *stmt->attrs())
        network_->setAttribute(cell, entry->key(), entry->value());
    }
  }

  module_map_[cell] = module;
  makeCellPorts(cell, module, ports);
}

void
VerilogReader::makeModule(std::string_view module_vname,
                          VerilogStmtSeq *port_dcls,
                          VerilogStmtSeq *stmts,
                          VerilogAttrStmtSeq *attr_stmts,
                          int line)
{
  VerilogNetSeq *ports = new VerilogNetSeq;
  // Pull the port names out of the port declarations.
  for (VerilogStmt *dcl : *port_dcls) {
    if (dcl->isDeclaration()) {
      VerilogDcl *dcl1 = dynamic_cast<VerilogDcl *>(dcl);
      for (VerilogDclArg *arg : *dcl1->args()) {
        VerilogNetNamed *port = new VerilogNetScalar(arg->netName());
        ports->push_back(port);
      }
      // Add the port declarations to the statements.
      stmts->push_back(dcl);
    }
  }
  delete port_dcls;
  makeModule(module_vname, ports, stmts, attr_stmts, line);
}

void
VerilogReader::makeCellPorts(Cell *cell,
                             VerilogModule *module,
                             VerilogNetSeq *ports)
{
  StringSet port_names;
  for (VerilogNet *mod_port : *ports) {
    const std::string &port_name = mod_port->name();
    if (!port_names.contains(port_name)) {
      port_names.insert(port_name);
      if (mod_port->isNamed()) {
        if (mod_port->isNamedPortRef())
          makeNamedPortRefCellPorts(cell, module, mod_port, port_names);
        else
          makeCellPort(cell, module, mod_port->name());
      }
    }
    else
      warn(165, module->filename(), module->line(),
           "module {} repeated port name {}.", module->name(), port_name);
  }
  checkModuleDcls(module, port_names);
}

Port *
VerilogReader::makeCellPort(Cell *cell,
                            VerilogModule *module,
                            const std::string &port_name)
{
  VerilogDcl *dcl = module->declaration(port_name);
  if (dcl) {
    PortDirection *dir = dcl->direction();
    VerilogDclBus *dcl_bus = dynamic_cast<VerilogDclBus *>(dcl);
    Port *port = dcl->isBus()
        ? network_->makeBusPort(cell, port_name, dcl_bus->fromIndex(),
                                dcl_bus->toIndex())
        : network_->makePort(cell, port_name);
    network_->setDirection(port, dir);
    return port;
  }
  else {
    warn(166, module->filename(), module->line(),
         "module {} missing declaration for port {}.",
         module->name(), port_name);
    return network_->makePort(cell, port_name);
  }
}

void
VerilogReader::makeNamedPortRefCellPorts(Cell *cell,
                                         VerilogModule *module,
                                         VerilogNet *mod_port,
                                         StringSet &port_names)
{
  PortSeq *member_ports = new PortSeq;
  VerilogNetNameIterator *net_name_iter = mod_port->nameIterator(module, this);
  while (net_name_iter->hasNext()) {
    const std::string &net_name = net_name_iter->next();
    port_names.insert(net_name);
    Port *port = makeCellPort(cell, module, net_name);
    member_ports->push_back(port);
  }
  delete net_name_iter;
  // Note that the bundle does NOT have a port declaration.
  network_->makeBundlePort(cell, mod_port->name(), member_ports);
}

// Make sure each declaration appears in the module port list.
void
VerilogReader::checkModuleDcls(VerilogModule *module,
                               std::set<std::string> &port_names)
{
  for (auto const &[port_name, dcl] : *module->declarationMap()) {
    PortDirection *dir = dcl->direction();
    if (dir->isInput() || dir->isOutput() || dir->isBidirect()) {
      if (!port_names.contains(port_name))
        linkWarn(197, module->filename(), module->line(),
                 "module {} declared signal {} is not in the port list.",
                 module->name(), port_name);
    }
  }
}

VerilogDcl *
VerilogReader::makeDcl(PortDirection *dir,
                       VerilogDclArgSeq *args,
                       VerilogAttrStmtSeq *attr_stmts,
                       int line)
{
  if (dir->isInternal()) {
    // Prune wire declarations without assigns because they just eat memory.
    VerilogDclArgSeq *assign_args = nullptr;
    for (VerilogDclArg *arg : *args) {
      if (arg->assign()) {
        if (assign_args == nullptr)
          assign_args = new VerilogDclArgSeq;
        assign_args->push_back(arg);
      }
      else {
        delete arg;
      }
    }
    delete args;
    if (assign_args) {
      return new VerilogDcl(dir, assign_args, attr_stmts, line);
    }
    else {
      deleteContents(attr_stmts);
      delete attr_stmts;
      return nullptr;
    }
  }
  else {
    return new VerilogDcl(dir, args, attr_stmts, line);
  }
}

VerilogDcl *
VerilogReader::makeDcl(PortDirection *dir,
                       VerilogDclArg *arg,
                       VerilogAttrStmtSeq *attr_stmts,
                       int line)
{
  return new VerilogDcl(dir, arg, attr_stmts, line);
}

VerilogDclBus *
VerilogReader::makeDclBus(PortDirection *dir,
                          int from_index,
                          int to_index,
                          VerilogDclArg *arg,
                          VerilogAttrStmtSeq *attr_stmts,
                          int line)
{
  return new VerilogDclBus(dir, from_index, to_index, arg, attr_stmts, line);
}

VerilogDclBus *
VerilogReader::makeDclBus(PortDirection *dir,
                          int from_index,
                          int to_index,
                          VerilogDclArgSeq *args,
                          VerilogAttrStmtSeq *attr_stmts,
                          int line)
{
  return new VerilogDclBus(dir, from_index, to_index, args, attr_stmts, line);
}

VerilogDclArg *
VerilogReader::makeDclArg(std::string_view net_vname)
{
  const std::string net_name = netVerilogToSta(net_vname);
  VerilogDclArg *dcl = new VerilogDclArg(net_name);
  return dcl;
}

VerilogDclArg *
VerilogReader::makeDclArg(VerilogAssign *assign)
{
  return new VerilogDclArg(assign);
}

VerilogNetPartSelect *
VerilogReader::makeNetPartSelect(std::string_view net_vname,
                                 int from_index,
                                 int to_index)
{
  const std::string net_name = netVerilogToSta(net_vname);
  VerilogNetPartSelect *select =
      new VerilogNetPartSelect(net_name, from_index, to_index);
  return select;
}

VerilogNetConstant *
VerilogReader::makeNetConstant(std::string_view constant,
                               int line)
{
  return new VerilogNetConstant(constant, this, line);
}

VerilogNetScalar *
VerilogReader::makeNetScalar(std::string_view net_vname)
{
  const std::string net_name = netVerilogToSta(net_vname);
  VerilogNetScalar *scalar = new VerilogNetScalar(net_name);
  return scalar;
}

VerilogNetBitSelect *
VerilogReader::makeNetBitSelect(std::string_view net_vname,
                                int index)
{
  const std::string net_name = netVerilogToSta(net_vname);
  VerilogNetBitSelect *select = new VerilogNetBitSelect(net_name, index);
  return select;
}

VerilogAssign *
VerilogReader::makeAssign(VerilogNet *lhs,
                          VerilogNet *rhs,
                          int line)
{
  return new VerilogAssign(lhs, rhs, line);
}

VerilogInst *
VerilogReader::makeModuleInst(std::string_view module_vname,
                              std::string_view inst_vname,
                              VerilogNetSeq *pins,
                              VerilogAttrStmtSeq *attr_stmts,
                              int line)
{
  const std::string module_name = moduleVerilogToSta(module_vname);
  const std::string inst_name = instanceVerilogToSta(inst_vname);
  Cell *cell = network_->findAnyCell(module_name);
  LibertyCell *liberty_cell = nullptr;
  if (cell)
    liberty_cell = network_->libertyCell(cell);
  // Instances of liberty with scalar ports are special cased
  // to reduce the memory footprint of the verilog parser.
  if (liberty_cell && hasScalarNamedPortRefs(liberty_cell, pins)) {
    int port_count = liberty_cell->portBitCount();
    StringSeq net_names(port_count);
    for (VerilogNet *vnet : *pins) {
      VerilogNetPortRefScalarNet *vpin =
          dynamic_cast<VerilogNetPortRefScalarNet *>(vnet);
      std::string_view port_name = vpin->name();
      std::string_view net_name = vpin->netName();
      Port *port = network_->findPort(cell, port_name);
      LibertyPort *lport = network_->libertyPort(port);
      if (lport->isBus()) {
        LibertyPortMemberIterator member_iter(lport);
        lport = member_iter.next();
      }
      int pin_index = lport->pinIndex();
      net_names[pin_index] = net_name;
      delete vpin;
    }
    VerilogInst *inst =
        new VerilogLibertyInst(liberty_cell, inst_name, net_names, attr_stmts, line);
    delete pins;
    return inst;
  }
  else {
    VerilogInst *inst = new VerilogModuleInst(module_name, inst_name,
                                              pins, attr_stmts, line);
    return inst;
  }
}

bool
VerilogReader::hasScalarNamedPortRefs(LibertyCell *liberty_cell,
                                      VerilogNetSeq *pins)
{
  if (pins && !pins->empty() && (*pins)[0]->isNamedPortRef()) {
    for (VerilogNet *vpin : *pins) {
      std::string_view port_name = vpin->name();
      LibertyPort *port = liberty_cell->findLibertyPort(port_name);
      if (port) {
        if (!(port->size() == 1 && (vpin->isNamedPortRefScalarNet())))
          return false;
      }
      else
        return false;
    }
    return true;
  }
  else
    return false;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefScalarNet(std::string_view port_vname)
{
  const std::string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefScalarNet(port_name);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefScalarNet(std::string_view port_vname,
                                            std::string_view net_vname)
{
  const std::string port_name = portVerilogToSta(port_vname);
  const std::string net_name = netVerilogToSta(net_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefScalarNet(port_name, net_name);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefBitSelect(std::string_view port_vname,
                                            std::string_view bus_vname,
                                            int index)
{
  const std::string bus_name = portVerilogToSta(bus_vname);
  const std::string net_name = verilogBusBitName(bus_name, index);
  const std::string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref =
      new VerilogNetPortRefScalarNet(port_name, net_name);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefScalar(std::string_view port_vname,
                                         VerilogNet *net)
{
  const std::string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefScalar(port_name, net);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefBit(std::string_view port_vname,
                                      int index,
                                      VerilogNet *net)
{
  const std::string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefBit(port_name, index, net);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefPart(std::string_view port_vname,
                                       int from_index,
                                       int to_index,
                                       VerilogNet *net)
{
  const std::string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref =
      new VerilogNetPortRefPart(port_name, from_index, to_index, net);
  return ref;
}

VerilogNetConcat *
VerilogReader::makeNetConcat(VerilogNetSeq *nets)
{
  return new VerilogNetConcat(nets);
}

////////////////////////////////////////////////////////////////

VerilogModule::VerilogModule(std::string_view name,
                             VerilogNetSeq *ports,
                             VerilogStmtSeq *stmts,
                             VerilogAttrStmtSeq *attr_stmts,
                             std::string_view filename,
                             int line,
                             VerilogReader *reader) :
  VerilogStmt(line),
  name_(name),
  filename_(filename),
  ports_(ports),
  stmts_(stmts),
  attr_stmts_(attr_stmts)
{
  parseStmts(reader);
}

VerilogModule::~VerilogModule()
{
  deleteContents(ports_);
  delete ports_;
  deleteContents(stmts_);
  delete stmts_;
  deleteContents(attr_stmts_);
  delete attr_stmts_;
}

void
VerilogModule::parseStmts(VerilogReader *reader)
{
  StringSet inst_names;
  for (VerilogStmt *stmt : *stmts_) {
    if (stmt->isDeclaration())
      parseDcl(dynamic_cast<VerilogDcl *>(stmt), reader);
    else if (stmt->isInstance())
      checkInstanceName(dynamic_cast<VerilogInst *>(stmt), inst_names, reader);
  }
}

void
VerilogModule::parseDcl(VerilogDcl *dcl,
                        VerilogReader *reader)
{
  for (VerilogDclArg *arg : *dcl->args()) {
    if (arg->isNamed()) {
      const std::string &net_name = arg->netName();
      VerilogDcl *existing_dcl = dcl_map_[net_name];
      if (existing_dcl) {
        PortDirection *existing_dir = existing_dcl->direction();
        if (existing_dir->isInternal())
          // wire dcl can be used as modifier for input/inout dcls.
          // Ignore the wire dcl.
          dcl_map_[net_name] = dcl;
        else if (dcl->direction()->isTristate()) {
          if (existing_dir->isOutput())
            // tri dcl can be used as modifier for input/output/inout dcls.
            // Keep the tristate dcl for outputs because it is more specific
            // but ignore it for inputs and bidirs.
            dcl_map_[net_name] = dcl;
        }
        else if (dcl->direction()->isPowerGround()
                 && (existing_dir->isOutput() || existing_dir->isInput()
                     || existing_dir->isBidirect()))
          // supply0/supply1 dcl can be used as modifier for
          // input/output/inout dcls.
          dcl_map_[net_name] = dcl;
        else if (!dcl->direction()->isInternal()) {
          std::string net_vname = netVerilogName(net_name);
          reader->warn(1395, filename_, dcl->line(),
                       "signal {} previously declared on line {}.",
                       net_vname, existing_dcl->line());
        }
      }
      else
        dcl_map_[net_name] = dcl;
    }
  }
}

// Check for duplicate instance names during parse rather than during
// expansion so errors are only reported once.
void
VerilogModule::checkInstanceName(VerilogInst *inst,
                                 StringSet &inst_names,
                                 VerilogReader *reader)
{
  std::string inst_name = inst->instanceName();
  if (inst_names.contains(inst_name)) {
    int i = 1;
    std::string replacement_name;
    do {
      replacement_name = sta::format("{}_{}", inst_name, i++);
    } while (inst_names.contains(replacement_name));
    std::string inst_vname = instanceVerilogName(inst_name);
    reader->warn(1396, filename_, inst->line(),
                 "instance name {} duplicated - renamed to {}.", inst_vname,
                 replacement_name);
    inst_name = replacement_name;
    inst->setInstanceName(inst_name);
  }
  inst_names.insert(inst_name);
}

VerilogDcl *
VerilogModule::declaration(std::string_view net_name)
{
  return findStringKey(dcl_map_, net_name);
}

////////////////////////////////////////////////////////////////

VerilogStmt::VerilogStmt(int line) :
  line_(line)
{
}

VerilogInst::VerilogInst(std::string_view inst_name,
                         VerilogAttrStmtSeq *attr_stmts,
                         int line) :
  VerilogStmt(line),
  inst_name_(inst_name),
  attr_stmts_(attr_stmts)
{
}

VerilogInst::~VerilogInst()
{
  deleteContents(attr_stmts_);
  delete attr_stmts_;
}

void
VerilogInst::setInstanceName(const std::string &inst_name)
{
  inst_name_ = inst_name;
}

VerilogModuleInst::VerilogModuleInst(std::string_view module_name,
                                     std::string_view inst_name,
                                     VerilogNetSeq *pins,
                                     VerilogAttrStmtSeq *attr_stmts,
                                     int line) :
  VerilogInst(inst_name,
              attr_stmts,
              line),
  module_name_(module_name),
  pins_(pins)
{
}

VerilogModuleInst::~VerilogModuleInst()
{
  if (pins_) {
    deleteContents(pins_);
    delete pins_;
  }
}

bool
VerilogModuleInst::hasPins()
{
  return pins_ && !pins_->empty();
}

bool
VerilogModuleInst::namedPins()
{
  return pins_ && !pins_->empty() && (*pins_)[0]->isNamedPortRef();
}

VerilogLibertyInst::VerilogLibertyInst(LibertyCell *cell,
                                       std::string_view inst_name,
                                       const StringSeq &net_names,
                                       VerilogAttrStmtSeq *attr_stmts,
                                       int line) :
  VerilogInst(inst_name, attr_stmts, line),
  cell_(cell),
  net_names_(net_names)
{
}

VerilogDcl::VerilogDcl(PortDirection *dir,
                       VerilogDclArgSeq *args,
                       VerilogAttrStmtSeq *attr_stmts,
                       int line) :
  VerilogStmt(line),
  dir_(dir),
  args_(args),
  attr_stmts_(attr_stmts)
{
}

VerilogDcl::VerilogDcl(PortDirection *dir,
                       VerilogDclArg *arg,
                       VerilogAttrStmtSeq *attr_stmts,
                       int line) :
  VerilogStmt(line),
  dir_(dir)
{
  args_ = new VerilogDclArgSeq;
  args_->push_back(arg);
  attr_stmts_ = attr_stmts;
}

VerilogDcl::~VerilogDcl()
{
  deleteContents(args_);
  delete args_;
  deleteContents(attr_stmts_);
  delete attr_stmts_;
}

void
VerilogDcl::appendArg(VerilogDclArg *arg)
{
  args_->push_back(arg);
}

const std::string &
VerilogDcl::portName()
{
  return (*args_)[0]->netName();
}

VerilogDclBus::VerilogDclBus(PortDirection *dir,
                             int from_index,
                             int to_index,
                             VerilogDclArgSeq *args,
                             VerilogAttrStmtSeq *attr_stmts,
                             int line) :
  VerilogDcl(dir, args, attr_stmts, line),
  from_index_(from_index),
  to_index_(to_index)
{
}

VerilogDclBus::VerilogDclBus(PortDirection *dir,
                             int from_index,
                             int to_index,
                             VerilogDclArg *arg,
                             VerilogAttrStmtSeq *attr_stmts,
                             int line) :
  VerilogDcl(dir, arg, attr_stmts, line),
  from_index_(from_index),
  to_index_(to_index)
{
}

int
VerilogDclBus::size() const
{
  return std::abs(to_index_ - from_index_) + 1;
}

VerilogDclArg::VerilogDclArg(std::string_view net_name) :
  net_name_(net_name),
  assign_(nullptr)
{
}

VerilogDclArg::VerilogDclArg(VerilogAssign *assign) :
  assign_(assign)
{
}

VerilogDclArg::~VerilogDclArg() { delete assign_; }

const std::string &
VerilogDclArg::netName()
{
  if (assign_)
    return assign_->lhs()->name();
  else
    return net_name_;
}

VerilogAssign::VerilogAssign(VerilogNet *lhs,
                             VerilogNet *rhs,
                             int line) :
  VerilogStmt(line),
  lhs_(lhs),
  rhs_(rhs)
{
}

VerilogAssign::~VerilogAssign()
{
  delete lhs_;
  delete rhs_;
}

////////////////////////////////////////////////////////////////

class VerilogNullNetNameIterator : public VerilogNetNameIterator
{
public:
  bool hasNext() override { return false; }
  const std::string &next() override;
};

const std::string &
VerilogNullNetNameIterator::next()
{
  static const std::string null;
  return null;
}

class VerilogOneNetNameIterator : public VerilogNetNameIterator
{
public:
  VerilogOneNetNameIterator(const std::string &name);
  bool hasNext() override;
  const std::string &next() override;

protected:
  std::string name_;
  bool has_next_{true};
};

VerilogOneNetNameIterator::VerilogOneNetNameIterator(const std::string &name) :
  name_(name)
{
}

bool
VerilogOneNetNameIterator::hasNext()
{
  return has_next_;
}

const std::string &
VerilogOneNetNameIterator::next()
{
  has_next_ = false;
  return name_;
}

class VerilogBusNetNameIterator : public VerilogNetNameIterator
{
public:
  VerilogBusNetNameIterator(std::string_view bus_name,
                            int from_index,
                            int to_index);
  bool hasNext() override;
  const std::string &next() override;

protected:
  const std::string bus_name_;
  int from_index_;
  int to_index_;
  int index_;
  std::string bit_name_;
};

VerilogBusNetNameIterator::VerilogBusNetNameIterator(std::string_view bus_name,
                                                     int from_index,
                                                     int to_index) :
  bus_name_(bus_name),
  from_index_(from_index),
  to_index_(to_index),
  index_(from_index)
{
}

bool
VerilogBusNetNameIterator::hasNext()
{
  return (to_index_ > from_index_ && index_ <= to_index_)
      || (to_index_ <= from_index_ && index_ >= to_index_);
}

const std::string &
VerilogBusNetNameIterator::next()
{
  bit_name_ = verilogBusBitName(bus_name_, index_);
  if (to_index_ > from_index_)
    index_++;
  else
    index_--;
  return bit_name_;
}

static std::string
verilogBusBitName(std::string_view bus_name,
                  int index)
{
  return sta::format("{}[{}]", bus_name, index);
}

class VerilogConstantNetNameIterator : public VerilogNetNameIterator
{
public:
  VerilogConstantNetNameIterator(VerilogConstantValue *value,
                                 const std::string &zero,
                                 const std::string &one);
  bool hasNext() override;
  const std::string &next() override;

private:
  VerilogConstantValue *value_;
  const std::string &zero_;
  const std::string &one_;
  int bit_index_;
};

VerilogConstantNetNameIterator::VerilogConstantNetNameIterator(
    VerilogConstantValue *value,
    const std::string &zero,
    const std::string &one) :
  value_(value),
  zero_(zero),
  one_(one),
  bit_index_(value->size() - 1)
{
}

bool
VerilogConstantNetNameIterator::hasNext()
{
  return bit_index_ >= 0;
}

const std::string &
VerilogConstantNetNameIterator::next()
{
  return (*value_)[bit_index_--] ? one_ : zero_;
}

class VerilogNetConcatNameIterator : public VerilogNetNameIterator
{
public:
  VerilogNetConcatNameIterator(VerilogNetSeq *nets,
                               VerilogModule *module,
                               VerilogReader *reader);
  ~VerilogNetConcatNameIterator() override;
  bool hasNext() override;
  const std::string &next() override;

private:
  VerilogModule *module_;
  VerilogReader *reader_;
  VerilogNetSeq *nets_;
  VerilogNetSeq::iterator net_iter_;
  VerilogNetNameIterator *net_name_iter_{nullptr};
};

VerilogNetConcatNameIterator::VerilogNetConcatNameIterator(VerilogNetSeq *nets,
                                                           VerilogModule *module,
                                                           VerilogReader *reader) :
  module_(module),
  reader_(reader),
  nets_(nets),
  net_iter_(nets->begin())
{
  if (net_iter_ != nets_->end()) {
    VerilogNet *net = *net_iter_++;
    net_name_iter_ = net->nameIterator(module, reader);
  }
}

VerilogNetConcatNameIterator::~VerilogNetConcatNameIterator()
{
  delete net_name_iter_;
}

bool
VerilogNetConcatNameIterator::hasNext()
{
  return (net_name_iter_ && net_name_iter_->hasNext()) || net_iter_ != nets_->end();
}

const std::string &
VerilogNetConcatNameIterator::next()
{
  if (net_name_iter_ && net_name_iter_->hasNext())
    return net_name_iter_->next();
  else {
    if (net_iter_ != nets_->end()) {
      VerilogNet *net = *net_iter_++;
      delete net_name_iter_;
      net_name_iter_ = net->nameIterator(module_, reader_);
      if (net_name_iter_ && net_name_iter_->hasNext())
        return net_name_iter_->next();
    }
  }
  static const std::string null;
  return null;
}

////////////////////////////////////////////////////////////////

const std::string VerilogNetUnnamed::null_;

VerilogNetNamed::VerilogNetNamed(std::string_view name) :
  VerilogNet(),
  name_(name)
{
}

VerilogNetScalar::VerilogNetScalar(std::string_view name) :
  VerilogNetNamed(name)
{
}

static int
verilogNetScalarSize(std::string_view name,
                     VerilogModule *module)
{
  VerilogDcl *dcl = module->declaration(name);
  if (dcl)
    return dcl->size();
  else
    // undeclared signals are size 1
    return 1;
}

int
VerilogNetScalar::size(VerilogModule *module)
{
  return verilogNetScalarSize(name_, module);
}

static VerilogNetNameIterator *
verilogNetScalarNameIterator(const std::string &name,
                             VerilogModule *module)
{
  if (!name.empty()) {
    VerilogDcl *dcl = module->declaration(name);
    if (dcl && dcl->isBus()) {
      VerilogDclBus *dcl_bus = dynamic_cast<VerilogDclBus *>(dcl);
      return new VerilogBusNetNameIterator(name, dcl_bus->fromIndex(),
                                           dcl_bus->toIndex());
    }
  }
  return new VerilogOneNetNameIterator(name);
}

VerilogNetNameIterator *
VerilogNetScalar::nameIterator(VerilogModule *module,
                               VerilogReader *)
{
  return verilogNetScalarNameIterator(name_, module);
}

VerilogNetBitSelect::VerilogNetBitSelect(std::string_view name,
                                         int index) :
  VerilogNetNamed(verilogBusBitName(name,
                                    index)),
  index_(index)
{
}

int
VerilogNetBitSelect::size(VerilogModule *)
{
  return 1;
}

VerilogNetNameIterator *
VerilogNetBitSelect::nameIterator(VerilogModule *,
                                  VerilogReader *)
{
  return new VerilogOneNetNameIterator(name_);
}

VerilogNetPartSelect::VerilogNetPartSelect(std::string_view name,
                                           int from_index,
                                           int to_index) :
  VerilogNetNamed(name),
  from_index_(from_index),
  to_index_(to_index)
{
}

int
VerilogNetPartSelect::size(VerilogModule *)
{
  if (to_index_ > from_index_)
    return to_index_ - from_index_ + 1;
  else
    return from_index_ - to_index_ + 1;
}

VerilogNetNameIterator *
VerilogNetPartSelect::nameIterator(VerilogModule *,
                                   VerilogReader *)
{
  return new VerilogBusNetNameIterator(name_, from_index_, to_index_);
}

VerilogNetConstant::VerilogNetConstant(std::string_view constant,
                                       VerilogReader *reader,
                                       int line)
{
  parseConstant(constant, reader, line);
}

void
VerilogNetConstant::parseConstant(std::string_view constant,
                                  VerilogReader *reader,
                                  int line)
{
  // Find constant size.
  size_t csize_end = constant.find('\'');
  std::string csize(constant.substr(0, csize_end));

  // Read the constant size.
  size_t size = std::stol(csize);
  value_ = new VerilogConstantValue(size);

  // Read the constant base.
  size_t base_idx = csize_end + 1;
  char base = constant.at(base_idx);
  switch (base) {
    case 'b':
    case 'B':
      parseConstant(constant, base_idx, 2, 1);
      break;
    case 'o':
    case 'O':
      parseConstant(constant, base_idx, 8, 3);
      break;
    case 'h':
    case 'H':
      parseConstant(constant, base_idx, 16, 4);
      break;
    case 'd':
    case 'D':
      parseConstant10(constant, base_idx, reader, line);
      break;
    default:
    case '\0':
      reader->report()->fileWarn(1861, reader->filename(), line,
                                 "unknown constant base.");
      break;
  }
}

void
VerilogNetConstant::parseConstant(std::string_view constant,
                                  size_t base_idx,
                                  int base,
                                  int digit_bit_count)
{
  // Scan the constant from LSD to MSD.
  size_t size = value_->size();
  char value_digit_str[2];
  char *end;
  value_digit_str[1] = '\0';
  size_t bit = 0;
  size_t idx = constant.size() - 1;
  while (bit < size) {
    char ch = (idx > base_idx) ? constant.at(idx--) : '0';
    // Skip underscores.
    if (ch != '_') {
      value_digit_str[0] = ch;
      unsigned value_digit = strtoul(value_digit_str, &end, base);
      unsigned mask = 1;
      for (int b = 0; b < digit_bit_count && bit < size; b++) {
        bool value_bit = (value_digit & mask) != 0;
        (*value_)[bit++] = value_bit;
        mask = mask << 1;
      }
    }
  }
}

void
VerilogNetConstant::parseConstant10(std::string_view constant,
                                      size_t base_idx,
                                      VerilogReader *reader,
                                      int line)
{
  // Copy the constant skipping underscores.
  std::string constant1;
  for (size_t i = base_idx + 1; i < constant.size(); i++) {
    char ch = constant.at(i);
    if (ch != '_')
      constant1 += ch;
  }

  size_t size = value_->size();
  size_t length = constant1.size();
  const std::string &constant10_max = reader->constant10Max();
  size_t max_length = constant10_max.size();
  if (length > max_length
      || (length == max_length && constant1 > constant10_max))
    reader->warn(1397, reader->filename(), line,
                 "base 10 constant greater than {} not supported.",
                 constant10_max);
  else {
    size_t *end = nullptr;
    VerilogConstant10 value = std::stoull(constant1, end, 10);
    VerilogConstant10 mask = 1;
    for (size_t bit = 0; bit < size; bit++) {
      (*value_)[bit] = (value & mask) != 0;
      mask = mask << 1;
    }
  }
}

VerilogNetConstant::~VerilogNetConstant() { delete value_; }

VerilogNetNameIterator *
VerilogNetConstant::nameIterator(VerilogModule *,
                                 VerilogReader *reader)
{
  return new VerilogConstantNetNameIterator(value_, reader->zeroNetName(),
                                            reader->oneNetName());
}

int
VerilogNetConstant::size(VerilogModule *)
{
  return value_->size();
}

VerilogNetConcat::VerilogNetConcat(VerilogNetSeq *nets) :
  nets_(nets)
{
}

VerilogNetConcat::~VerilogNetConcat()
{
  deleteContents(nets_);
  delete nets_;
}

int
VerilogNetConcat::size(VerilogModule *module)
{
  int sz = 0;
  for (VerilogNet *net : *nets_)
    sz += net->size(module);
  return sz;
}

VerilogNetNameIterator *
VerilogNetConcat::nameIterator(VerilogModule *module,
                               VerilogReader *reader)
{
  return new VerilogNetConcatNameIterator(nets_, module, reader);
}

VerilogNetPortRef::VerilogNetPortRef(std::string_view name) :
  VerilogNetScalar(name)
{
}

VerilogNetPortRefScalarNet::VerilogNetPortRefScalarNet(std::string_view name) :
  VerilogNetPortRef(name)
{
}

VerilogNetPortRefScalarNet::VerilogNetPortRefScalarNet(std::string_view name,
                                                       std::string_view net_name) :
  VerilogNetPortRef(name),
  net_name_(net_name)
{
}

int
VerilogNetPortRefScalarNet::size(VerilogModule *module)
{
  // VerilogNetScalar::size
  VerilogDcl *dcl = nullptr;
  if (!net_name_.empty())
    dcl = module->declaration(net_name_);
  if (dcl)
    return dcl->size();
  else
    // undeclared signals are size 1
    return 1;
}

VerilogNetNameIterator *
VerilogNetPortRefScalarNet::nameIterator(VerilogModule *module,
                                         VerilogReader *)
{
  return verilogNetScalarNameIterator(net_name_, module);
}

VerilogNetPortRefScalar::VerilogNetPortRefScalar(std::string_view name,
                                                 VerilogNet *net) :
  VerilogNetPortRef(name),
  net_(net)
{
}

VerilogNetPortRefScalar::~VerilogNetPortRefScalar() { delete net_; }

int
VerilogNetPortRefScalar::size(VerilogModule *module)
{
  if (net_)
    return net_->size(module);
  else
    return 0;
}

VerilogNetNameIterator *
VerilogNetPortRefScalar::nameIterator(VerilogModule *module,
                                      VerilogReader *reader)
{
  if (net_)
    return net_->nameIterator(module, reader);
  else
    return new VerilogNullNetNameIterator();
}

VerilogNetPortRefBit::VerilogNetPortRefBit(std::string_view name,
                                           int index,
                                           VerilogNet *net) :
  VerilogNetPortRefScalar(name,
                          net),
  bit_name_(verilogBusBitName(name,
                              index))
{
}

VerilogNetPortRefPart::VerilogNetPortRefPart(std::string_view name,
                                             int from_index,
                                             int to_index,
                                             VerilogNet *net) :
  VerilogNetPortRefBit(name, from_index, net),
  to_index_(to_index)
{
}

const std::string &
VerilogNetPortRefPart::name() const
{
  return name_;
}

VerilogAttrEntry::VerilogAttrEntry(std::string_view key,
                                   std::string_view value) :
  key_(key),
  value_(value)
{
}

VerilogAttrStmt::VerilogAttrStmt(VerilogAttrEntrySeq *attrs) :
  attrs_(attrs)
{
}

VerilogAttrStmt::~VerilogAttrStmt()
{
  deleteContents(attrs_);
  delete attrs_;
}

VerilogAttrEntrySeq *
VerilogAttrStmt::attrs()
{
  return attrs_;
}

////////////////////////////////////////////////////////////////
//
// Link verilog network
//
////////////////////////////////////////////////////////////////

// Verilog net name to network net map.
using BindingMap = std::map<std::string, Net *, std::less<>>;

class VerilogBindingTbl
{
public:
  VerilogBindingTbl(const std::string &zero_net_name_,
                    const std::string &one_net_name_);
  Net *ensureNetBinding(std::string_view net_name,
                        Instance *inst,
                        NetworkReader *network);
  Net *find(std::string_view name,
            NetworkReader *network);
  void bind(std::string_view name,
            Net *net);

private:
  const std::string &zero_net_name_;
  const std::string &one_net_name_;
  BindingMap net_map_;
};

Instance *
VerilogReader::linkNetwork(std::string_view top_cell_name,
                           bool make_black_boxes,
                           bool delete_modules)
{
  if (library_) {
    const std::string top_cell_str(top_cell_name);
    Cell *top_cell = network_->findCell(library_, top_cell_str);
    VerilogModule *module = this->module(top_cell);
    if (module) {
      // Seed the recursion for expansion with the top level instance.
      Instance *top_instance =
          network_->makeInstance(top_cell, top_cell_str, nullptr);
      VerilogBindingTbl bindings(zero_net_name_, one_net_name_);
      for (VerilogNet *mod_port : *module->ports()) {
        VerilogNetNameIterator *net_name_iter = mod_port->nameIterator(module, this);
        while (net_name_iter->hasNext()) {
          const std::string &net_name = net_name_iter->next();
          Port *port = network_->findPort(top_cell, net_name);
          Net *net =
              bindings.ensureNetBinding(net_name, top_instance, network_);
          // Guard against repeated port name.
          if (network_->findPin(top_instance, port) == nullptr) {
            Pin *pin = network_->makePin(top_instance, port, nullptr);
            network_->makeTerm(pin, net);
          }
        }
        delete net_name_iter;
      }
      makeModuleInstBody(module, top_instance, &bindings, make_black_boxes);
      bool errors = reportLinkErrors();
      if (delete_modules)
        deleteModules();
      if (errors) {
        network_->deleteInstance(top_instance);
        return nullptr;
      }
      else
        return top_instance;
    }
    else {
      report_->error(1398, "{} is not a verilog module.", top_cell_name);
      return nullptr;
    }
  }
  else {
    report_->error(1399, "{} is not a verilog module.", top_cell_name);
    return nullptr;
  }
}

void
VerilogReader::makeModuleInstBody(VerilogModule *module,
                                  Instance *inst,
                                  VerilogBindingTbl *bindings,
                                  bool make_black_boxes)
{
  for (VerilogStmt *stmt : *module->stmts()) {
    if (stmt->isModuleInst())
      makeModuleInstNetwork(dynamic_cast<VerilogModuleInst *>(stmt), inst, module,
                            bindings, make_black_boxes);
    else if (stmt->isLibertyInst())
      makeLibertyInst(dynamic_cast<VerilogLibertyInst *>(stmt), inst, module,
                      bindings);
    else if (stmt->isDeclaration()) {
      VerilogDcl *dcl = dynamic_cast<VerilogDcl *>(stmt);
      PortDirection *dir = dcl->direction();
      for (VerilogDclArg *arg : *dcl->args()) {
        VerilogAssign *assign = arg->assign();
        if (assign)
          mergeAssignNet(assign, module, inst, bindings);
        if (dir->isGround()) {
          Net *net =
              bindings->ensureNetBinding(arg->netName(), inst, network_);
          network_->addConstantNet(net, LogicValue::zero);
        }
        if (dir->isPower()) {
          Net *net =
              bindings->ensureNetBinding(arg->netName(), inst, network_);
          network_->addConstantNet(net, LogicValue::one);
        }
      }
    }
    else if (stmt->isAssign())
      mergeAssignNet(dynamic_cast<VerilogAssign *>(stmt), module, inst, bindings);
  }
}

void
VerilogReader::makeModuleInstNetwork(VerilogModuleInst *mod_inst,
                                     Instance *parent,
                                     VerilogModule *parent_module,
                                     VerilogBindingTbl *parent_bindings,
                                     bool make_black_boxes)
{
  const std::string &module_name = mod_inst->moduleName();
  Cell *cell = network_->findAnyCell(module_name);
  if (cell == nullptr) {
    std::string inst_vname = instanceVerilogName(mod_inst->instanceName());
    if (make_black_boxes) {
      cell = makeBlackBox(mod_inst, parent_module);
      linkWarn(198, parent_module->filename(), mod_inst->line(),
               "module {} not found. Creating black box for {}.",
               mod_inst->moduleName(), inst_vname);
    }
    else
      linkError(199, parent_module->filename(), mod_inst->line(),
                "module {} not found for instance {}.",
                mod_inst->moduleName(), inst_vname);
  }
  if (cell) {
    LibertyCell *lib_cell = network_->libertyCell(cell);
    if (lib_cell)
      cell = network_->cell(lib_cell);
    Instance *inst =
        network_->makeInstance(cell, mod_inst->instanceName(), parent);
    VerilogAttrStmtSeq *attr_stmts = mod_inst->attrStmts();
    for (VerilogAttrStmt *stmt : *attr_stmts) {
      for (VerilogAttrEntry *entry : *stmt->attrs()) {
        network_->setAttribute(inst, entry->key(), entry->value());
      }
    }

    // Make all pins so timing arcs are built and get_pins finds them.
    CellPortBitIterator *port_iter = network_->portBitIterator(cell);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      network_->makePin(inst, port, nullptr);
    }
    delete port_iter;
    bool is_leaf = network_->isLeaf(cell);
    VerilogBindingTbl bindings(zero_net_name_, one_net_name_);
    if (mod_inst->hasPins()) {
      if (mod_inst->namedPins())
        makeNamedInstPins(cell, inst, mod_inst, &bindings, parent, parent_module,
                          parent_bindings, is_leaf);
      else
        makeOrderedInstPins(cell, inst, mod_inst, &bindings, parent, parent_module,
                            parent_bindings, is_leaf);
    }
    if (!is_leaf) {
      VerilogModule *module = this->module(cell);
      if (module)
        makeModuleInstBody(module, inst, &bindings, make_black_boxes);
    }
  }
}

void
VerilogReader::makeNamedInstPins(Cell *cell,
                                 Instance *inst,
                                 VerilogModuleInst *mod_inst,
                                 VerilogBindingTbl *bindings,
                                 Instance *parent,
                                 VerilogModule *parent_module,
                                 VerilogBindingTbl *parent_bindings,
                                 bool is_leaf)
{
  std::string inst_vname = instanceVerilogName(mod_inst->instanceName());
  for (auto mpin : *mod_inst->pins()) {
    VerilogNetPortRef *vpin = dynamic_cast<VerilogNetPortRef *>(mpin);
    const std::string &port_name = vpin->name();
    Port *port = network_->findPort(cell, port_name);
    if (port) {
      if (vpin->hasNet() && network_->size(port) != vpin->size(parent_module)) {
        linkWarn(200, parent_module->filename(), mod_inst->line(),
                 "instance {} port {} size {} does not match net size {}.",
                 inst_vname, network_->name(port), network_->size(port),
                 vpin->size(parent_module));
      }
      else {
        VerilogNetNameIterator *net_name_iter =
            vpin->nameIterator(parent_module, this);
        if (network_->hasMembers(port)) {
          PortMemberIterator *port_iter = network_->memberIterator(port);
          while (port_iter->hasNext()) {
            Port *port = port_iter->next();
            makeInstPin(inst, port, net_name_iter, bindings, parent, parent_bindings,
                        is_leaf);
          }
          delete port_iter;
        }
        else {
          makeInstPin(inst, port, net_name_iter, bindings, parent, parent_bindings,
                      is_leaf);
        }
        delete net_name_iter;
      }
    }
    else
      linkWarn(201, parent_module->filename(), mod_inst->line(),
               "instance {} port {} not found.", inst_vname, port_name);
  }
}

void
VerilogReader::makeOrderedInstPins(Cell *cell,
                                   Instance *inst,
                                   VerilogModuleInst *mod_inst,
                                   VerilogBindingTbl *bindings,
                                   Instance *parent,
                                   VerilogModule *parent_module,
                                   VerilogBindingTbl *parent_bindings,
                                   bool is_leaf)
{
  CellPortIterator *port_iter = network_->portIterator(cell);
  VerilogNetSeq *mod_pins = mod_inst->pins();
  VerilogNetSeq::iterator pin_iter = mod_pins->begin();
  while (pin_iter != mod_pins->end() && port_iter->hasNext()) {
    VerilogNet *net = *pin_iter++;
    Port *port = port_iter->next();
    if (network_->size(port) != net->size(parent_module)) {
      std::string inst_vname = instanceVerilogName(mod_inst->instanceName());
      linkWarn(202, parent_module->filename(), mod_inst->line(),
               "instance {} port {} size {} does not match net size {}.",
               inst_vname, network_->name(port), network_->size(port),
               net->size(parent_module));
    }
    else {
      VerilogNetNameIterator *net_name_iter = net->nameIterator(parent_module, this);
      if (network_->isBus(port)) {
        PortMemberIterator *member_iter = network_->memberIterator(port);
        while (member_iter->hasNext() && net_name_iter->hasNext()) {
          Port *port = member_iter->next();
          makeInstPin(inst, port, net_name_iter, bindings, parent, parent_bindings,
                      is_leaf);
        }
        delete member_iter;
      }
      else
        makeInstPin(inst, port, net_name_iter, bindings, parent, parent_bindings,
                    is_leaf);
      delete net_name_iter;
    }
  }
  delete port_iter;
}

void
VerilogReader::makeInstPin(Instance *inst,
                           Port *port,
                           VerilogNetNameIterator *net_name_iter,
                           VerilogBindingTbl *bindings,
                           Instance *parent,
                           VerilogBindingTbl *parent_bindings,
                           bool is_leaf)
{
  std::string net_name;
  if (net_name_iter->hasNext())
    net_name = net_name_iter->next();
  makeInstPin(inst, port, net_name, bindings, parent, parent_bindings, is_leaf);
}

void
VerilogReader::makeInstPin(Instance *inst,
                           Port *port,
                           const std::string &net_name,
                           VerilogBindingTbl *bindings,
                           Instance *parent,
                           VerilogBindingTbl *parent_bindings,
                           bool is_leaf)
{
  Net *net = nullptr;
  if (!net_name.empty())
    net = parent_bindings->ensureNetBinding(net_name, parent, network_);
  if (is_leaf) {
    // Connect leaf pin to net.
    if (net)
      network_->connect(inst, port, net);
  }
  else {
    // Pin should already exist by prior makePin, then connect to parent
    // net if present and create a term for the child-side net.
    Pin *pin = network_->findPin(inst, port);
    if (net) {
      network_->connect(inst, port, net);
      std::string port_name = network_->name(port);
      Net *child_net = bindings->ensureNetBinding(port_name, inst, network_);
      network_->makeTerm(pin, child_net);
    }
  }
}

void
VerilogReader::makeLibertyInst(VerilogLibertyInst *lib_inst,
                               Instance *parent,
                               VerilogModule *parent_module,
                               VerilogBindingTbl *parent_bindings)
{
  LibertyCell *lib_cell = lib_inst->cell();
  Cell *cell = reinterpret_cast<Cell *>(lib_cell);
  Instance *inst =
      network_->makeInstance(cell, lib_inst->instanceName(), parent);
  VerilogAttrStmtSeq *attr_stmts = lib_inst->attrStmts();
  for (VerilogAttrStmt *stmt : *attr_stmts) {
    for (VerilogAttrEntry *entry : *stmt->attrs()) {
      network_->setAttribute(inst, entry->key(), entry->value());
    }
  }
  const StringSeq &net_names = lib_inst->netNames();
  LibertyCellPortBitIterator port_iter(lib_cell);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    const std::string &net_name = net_names[port->pinIndex()];
    // net_name may be the name of a single bit bus.
    if (!net_name.empty()) {
      Net *net = nullptr;
      // If the pin is unconnected (ie, .A()) make the pin but not the net.
      VerilogDcl *dcl = parent_module->declaration(net_name);
      // Check for single bit bus reference .A(BUS) -> .A(BUS[LSB]).
      if (dcl && dcl->isBus()) {
        VerilogDclBus *dcl_bus = dynamic_cast<VerilogDclBus *>(dcl);
        // Bus is only 1 bit wide.
        std::string bus_name = verilogBusBitName(net_name, dcl_bus->fromIndex());
        net = parent_bindings->ensureNetBinding(bus_name, parent, network_);
      }
      else
        net = parent_bindings->ensureNetBinding(net_name, parent, network_);
      network_->makePin(inst, reinterpret_cast<Port *>(port), net);
    }
    else
      // Make unconnected pin.
      network_->makePin(inst, reinterpret_cast<Port *>(port), nullptr);
  }
}

////////////////////////////////////////////////////////////////

Cell *
VerilogReader::makeBlackBox(VerilogModuleInst *mod_inst,
                            VerilogModule *parent_module)
{
  const std::string &module_name = mod_inst->moduleName();
  Cell *cell = network_->makeCell(library_, module_name, true,
                                  parent_module->filename());
  if (mod_inst->namedPins())
    makeBlackBoxNamedPorts(cell, mod_inst, parent_module);
  else
    makeBlackBoxOrderedPorts(cell, mod_inst, parent_module);
  return cell;
}

void
VerilogReader::makeBlackBoxNamedPorts(Cell *cell,
                                      VerilogModuleInst *mod_inst,
                                      VerilogModule *parent_module)
{
  for (VerilogNet *mpin : *mod_inst->pins()) {
    VerilogNetNamed *vpin = dynamic_cast<VerilogNetNamed *>(mpin);
    const std::string &port_name = vpin->name();
    size_t size = vpin->size(parent_module);
    Port *port = (size == 1) ? network_->makePort(cell, port_name)
                             : network_->makeBusPort(cell, port_name, 0, size - 1);
    network_->setDirection(port, PortDirection::unknown());
  }
}

void
VerilogReader::makeBlackBoxOrderedPorts(Cell *cell,
                                        VerilogModuleInst *mod_inst,
                                        VerilogModule *parent_module)
{
  int port_index = 0;
  VerilogNetSeq *nets = mod_inst->pins();
  if (nets) {
    for (VerilogNet *net : *nets) {
      size_t size = net->size(parent_module);
      std::string port_name = format("p_{}", port_index);
      Port *port = (size == 1)
        ? network_->makePort(cell, port_name)
        : network_->makeBusPort(cell, port_name, size - 1, 0);
      network_->setDirection(port, PortDirection::unknown());
      port_index++;
    }
  }
}

bool
VerilogReader::isBlackBox(Cell *cell)
{
  return network_->library(cell) == library_;
}

////////////////////////////////////////////////////////////////

void
VerilogReader::mergeAssignNet(VerilogAssign *assign,
                              VerilogModule *module,
                              Instance *inst,
                              VerilogBindingTbl *bindings)
{
  VerilogNet *lhs = assign->lhs();
  VerilogNet *rhs = assign->rhs();
  if (lhs->size(module) == rhs->size(module)) {
    VerilogNetNameIterator *lhs_iter = lhs->nameIterator(module, this);
    VerilogNetNameIterator *rhs_iter = rhs->nameIterator(module, this);
    while (lhs_iter->hasNext() && rhs_iter->hasNext()) {
      const std::string &lhs_name = lhs_iter->next();
      const std::string &rhs_name = rhs_iter->next();
      Net *lhs_net = bindings->ensureNetBinding(lhs_name, inst, network_);
      Net *rhs_net = bindings->ensureNetBinding(rhs_name, inst, network_);
      // Merge lower level net into higher level net so that deleting
      // instances from the bottom up does not reference deleted nets
      // by referencing the mergedInto field.
      if (hierarchyLevel(lhs_net, network_) >= hierarchyLevel(rhs_net, network_))
        network_->mergeInto(lhs_net, rhs_net);
      else
        network_->mergeInto(rhs_net, lhs_net);
      // No need to update binding tables because the VerilogBindingTbl::find
      // finds the net that survives the merge.
    }
    delete lhs_iter;
    delete rhs_iter;
  }
  else
    linkWarn(203, module->filename(), assign->line(),
             "assign left hand side size {} not equal right hand size {}.",
             lhs->size(module), rhs->size(module));
}

static int
hierarchyLevel(Net *net,
               Network *network)
{
  Instance *parent = network->instance(net);
  int level = 0;
  while (parent) {
    parent = network->parent(parent);
    level++;
  }
  return level;
}

////////////////////////////////////////////////////////////////

VerilogBindingTbl::VerilogBindingTbl(const std::string &zero_net_name,
                                     const std::string &one_net_name) :
  zero_net_name_(zero_net_name),
  one_net_name_(one_net_name)
{
}

// Follow the merged_into pointers rather than update the
// binding tables up the call tree when nodes are merged
// because the name changes up the hierarchy.
Net *
VerilogBindingTbl::find(std::string_view name,
                        NetworkReader *network)
{
  Net *net = findStringKey(net_map_, name);
  while (net && network->mergedInto(net))
    net = network->mergedInto(net);
  return net;
}

void
VerilogBindingTbl::bind(std::string_view name,
                        Net *net)
{
  net_map_[std::string(name)] = net;
}

Net *
VerilogBindingTbl::ensureNetBinding(std::string_view net_name,
                                    Instance *inst,
                                    NetworkReader *network)
{
  Net *net = find(net_name, network);
  if (net == nullptr) {
    const std::string net_str(net_name);
    net = network->makeNet(net_str, inst);
    net_map_[std::string(network->name(net))] = net;
    if (net_str == zero_net_name_)
      network->addConstantNet(net, LogicValue::zero);
    if (net_str == one_net_name_)
      network->addConstantNet(net, LogicValue::one);
  }
  return net;
}

////////////////////////////////////////////////////////////////

bool
VerilogReader::reportLinkErrors()
{
  // Sort errors so they are in line number order rather than the order
  // they are discovered.
  sort(link_errors_, VerilogErrorCmp());
  bool errors = false;
  for (VerilogError *error : link_errors_) {
    // Report as warnings to avoid throwing.
    report_->fileWarn(error->id(), error->filename(), error->line(), "{}",
                      error->msg());
    errors |= !error->warn();
    delete error;
  }
  link_errors_.clear();
  return errors;
}

////////////////////////////////////////////////////////////////

VerilogScanner::VerilogScanner(std::istream *stream,
                               std::string_view filename,
                               Report *report) :
  yyFlexLexer(stream),
  filename_(filename),
  report_(report)
{
}

void
VerilogScanner::error(std::string_view msg)
{
  report_->fileError(1870, filename_, lineno(), "{}", msg);
}

}  // namespace sta
