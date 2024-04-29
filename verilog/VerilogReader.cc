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

#include "VerilogReader.hh"

#include <cstdlib>

#include "Debug.hh"
#include "Report.hh"
#include "Error.hh"
#include "Stats.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "VerilogNamespace.hh"
#include "StringUtil.hh"
#include "verilog/VerilogReaderPvt.hh"

extern int
VerilogParse_parse();

namespace sta {

VerilogReader *verilog_reader;
static const char *unconnected_net_name = reinterpret_cast<const char*>(1);

static string
verilogBusBitName(const char *bus_name,
                  int index);
static int
hierarchyLevel(Net *net,
	       Network *network);
// Return top level instance.
Instance *
linkVerilogNetwork(const char *top_cell_name,
		   bool make_black_boxes,
		   Report *report,
		   NetworkReader *network);

bool
readVerilogFile(const char *filename,
		NetworkReader *network)
{
  if (verilog_reader == nullptr)
    verilog_reader = new VerilogReader(network);
  return verilog_reader->read(filename);
}

void
deleteVerilogReader()
{
  delete verilog_reader;
  verilog_reader = nullptr;
}

////////////////////////////////////////////////////////////////

class VerilogError
{
public:
  VerilogError(int id,
               const char *filename,
	       int line,
	       const char *msg,
	       bool warn);
  ~VerilogError();
  const char *msg() const { return msg_; }
  const char *filename() const { return filename_; }
  int id() const { return id_; }
  int line() const { return line_; }
  bool warn() const { return warn_; }

private:
  int id_;
  const char *filename_;
  int line_;
  const char *msg_;
  bool warn_;

  friend class VerilogErrorCmp;
};

VerilogError::VerilogError(int id,
                           const char *filename,
			   int line,
			   const char *msg,
			   bool warn) :
  id_(id),
  filename_(filename),
  line_(line),
  msg_(msg),
  warn_(warn)
{
}

VerilogError::~VerilogError()
{
  // filename is owned by VerilogReader.
  stringDelete(msg_);
}

class VerilogErrorCmp
{
public:
  bool operator()(const VerilogError *error1,
		  const VerilogError *error2) const
  {
    int file_cmp = strcmp(error1->filename_, error2->filename_);
    if (file_cmp == 0) {
      if (error1->line_ == error2->line_)
	return strcmp(error1->msg_, error2->msg_) < 0;
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
  library_(nullptr),
  black_box_index_(0),
  zero_net_name_("zero_"),
  one_net_name_("one_")
{
  network->setLinkFunc(linkVerilogNetwork);
  VerilogConstant10 constant10_max = 0;
  constant10_max_ = stringPrint("%llu", ~constant10_max);
  constant10_max_length_ = strlen(constant10_max_);
}

VerilogReader::~VerilogReader()
{
  deleteModules();
  stringDelete(constant10_max_);
}

void
VerilogReader::deleteModules()
{
  StringSet filenames;
  for (auto module_iter : module_map_) {
    VerilogModule *module = module_iter.second;
    filenames.insert(module->filename());
    delete module;
  }
  deleteContents(&filenames);
  module_map_.clear();
}

bool
VerilogReader::read(const char *filename)
{
  // Use zlib to uncompress gzip'd files automagically.
  stream_ = gzopen(filename, "rb");
  if (stream_) {
    Stats stats(debug_, report_);
    init(filename);
    bool success = (::VerilogParse_parse() == 0);
    gzclose(stream_);
    reportStmtCounts();
    stats.report("Read verilog");
    return success;
  }
  else
    throw FileNotReadable(filename);
}

void
VerilogReader::init(const char *filename)
{
  // Statements point to verilog_filename, so copy it.
  filename_ = stringCopy(filename);
  line_ = 1;

  library_ = network_->findLibrary("verilog");
  if (library_ == nullptr)
    library_ = network_->makeLibrary("verilog", nullptr);

  report_stmt_stats_ = debug_->check("verilog", 1);
  module_count_ = 0;
  inst_mod_count_ = 0;
  inst_lib_count_ = 0;
  inst_lib_net_arrays_ = 0;
  dcl_count_ = 0;
  dcl_bus_count_ = 0;
  dcl_arg_count_ = 0;
  net_scalar_count_ = 0;
  net_part_select_count_ = 0;
  net_bit_select_count_ = 0;
  net_port_ref_scalar_count_ = 0;
  net_port_ref_scalar_net_count_ = 0;
  net_port_ref_bit_count_ = 0;
  net_port_ref_part_count_ = 0;
  net_constant_count_ = 0;
  assign_count_ = 0;
  concat_count_ = 0;
  inst_names_ = 0;
  port_names_ = 0;
  inst_module_names_ = 0;
  net_scalar_names_ = 0;
  net_bus_names_ = 0;
}

void
VerilogReader::getChars(char *buf,
			size_t &result,
			size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_nullptr
  else
    result = strlen(buf);
}

void
VerilogReader::getChars(char *buf,
			int &result,
			size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_nullptr
  else
    result = strlen(buf);
}

VerilogModule *
VerilogReader::module(Cell *cell)
{
  return module_map_.findKey(cell);
}

void
VerilogReader::makeModule(const char *module_vname,
			  VerilogNetSeq *ports,
			  VerilogStmtSeq *stmts,
                          VerilogAttributeStmtSeq *attribute_stmts,
			  int line)
{
  string module_name = moduleVerilogToSta(module_vname);
  Cell *cell = network_->findCell(library_, module_name.c_str());
  if (cell) {
    VerilogModule *module = module_map_[cell];
    delete module;
    module_map_.erase(cell);
    network_->deleteCell(cell);
  }

  VerilogModule *module = new VerilogModule(module_name.c_str(), ports, stmts,
					    attribute_stmts, filename_, line, this);
  cell = network_->makeCell(library_, module_name.c_str(), false, filename_);

  for (VerilogAttributeStmt *stmt : *attribute_stmts) {
    for (VerilogAttributeEntry *entry : *stmt->attribute_sequence()) {
      network_->setAttribute(cell, entry->key(), entry->value());
    }
  }

  module_map_[cell] = module;
  makeCellPorts(cell, module, ports);
  module_count_++;
  stringDelete(module_vname);
}

void
VerilogReader::makeModule(const char *module_name,
			  VerilogStmtSeq *port_dcls,
			  VerilogStmtSeq *stmts,
                          VerilogAttributeStmtSeq *attribute_stmts,
			  int line)
{
  VerilogNetSeq *ports = new VerilogNetSeq;
  // Pull the port names out of the port declarations.
  for (VerilogStmt *dcl : *port_dcls) {
    if (dcl->isDeclaration()) {
      VerilogDcl *dcl1 = dynamic_cast<VerilogDcl*>(dcl);
      for (VerilogDclArg *arg : *dcl1->args()) {
	VerilogNetNamed *port = new VerilogNetScalar(arg->netName());
	ports->push_back(port);
      }
      // Add the port declarations to the statements.
      stmts->push_back(dcl);
    }
  }
  delete port_dcls;
  makeModule(module_name, ports, stmts, attribute_stmts, line);
}

void
VerilogReader::makeCellPorts(Cell *cell,
			     VerilogModule *module,
			     VerilogNetSeq *ports)
{
  set<string> port_names;
  for (VerilogNet *mod_port : *ports) {
    const string &port_name = mod_port->name();
    if (port_names.find(port_name) == port_names.end()) {
      port_names.insert(port_name);
      if (mod_port->isNamed()) {
	if (mod_port->isNamedPortRef())
	  makeNamedPortRefCellPorts(cell, module, mod_port, port_names);
	else
	  makeCellPort(cell, module, mod_port->name().c_str());
      }
    }
    else
      warn(165, module->filename(), module->line(),
	   "module %s repeated port name %s.",
	   module->name(),
	   port_name.c_str());
  }
  checkModuleDcls(module, port_names);
}

Port *
VerilogReader::makeCellPort(Cell *cell,
			    VerilogModule *module,
			    const char *port_name)
{
  VerilogDcl *dcl = module->declaration(port_name);
  if (dcl) {
    PortDirection *dir = dcl->direction();
    VerilogDclBus *dcl_bus = dynamic_cast<VerilogDclBus*>(dcl);
    Port *port = dcl->isBus()
      ? network_->makeBusPort(cell, port_name, dcl_bus->fromIndex(),
			      dcl_bus->toIndex())
      : network_->makePort(cell, port_name);
    network_->setDirection(port, dir);
    return port;
  }
  else {
    warn(166, module->filename(), module->line(),
	 "module %s missing declaration for port %s.",
	 module->name(),
	 port_name);
    return network_->makePort(cell, port_name);
  }
}

void
VerilogReader::makeNamedPortRefCellPorts(Cell *cell,
					 VerilogModule *module,
					 VerilogNet *mod_port,
					 set<string> &port_names)
{
  PortSeq *member_ports = new PortSeq;
  VerilogNetNameIterator *net_name_iter = mod_port->nameIterator(module,this);
  while (net_name_iter->hasNext()) {
    const char *net_name = net_name_iter->next();
    port_names.insert(net_name);
    Port *port = makeCellPort(cell, module, net_name);
    member_ports->push_back(port);
  }
  delete net_name_iter;
  // Note that the bundle does NOT have a port declaration.
  network_->makeBundlePort(cell, mod_port->name().c_str(), member_ports);
}

// Make sure each declaration appears in the module port list.
void
VerilogReader::checkModuleDcls(VerilogModule *module,
			       set<string> &port_names)
{
  VerilogDclMap::ConstIterator dcl_iter(module->declarationMap());
  while (dcl_iter.hasNext()) {
    VerilogDcl *dcl;
    const char *port_name;
    dcl_iter.next(port_name, dcl);
    PortDirection *dir = dcl->direction();
    if (dir->isInput()
	|| dir->isOutput()
	|| dir->isBidirect()) {
      if (port_names.find(port_name) == port_names.end())
	linkWarn(197, module->filename(), module->line(),
		 "module %s declared signal %s is not in the port list.",
		 module->name(),
		 port_name);
    }
  }
}

VerilogDcl *
VerilogReader::makeDcl(PortDirection *dir,
		       VerilogDclArgSeq *args,
                       VerilogAttributeStmtSeq* attribute_stmts,
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
	dcl_arg_count_--;
      }
    }
    delete args;
    if (assign_args) {
      dcl_count_++;
      return new VerilogDcl(dir, assign_args, attribute_stmts, line);
    }
    else {
      attribute_stmts->deleteContents();
      delete attribute_stmts;
      return nullptr;
    }
  }
  else {
    dcl_count_++;
    return new VerilogDcl(dir, args, attribute_stmts, line);
  }
}

VerilogDcl *
VerilogReader::makeDcl(PortDirection *dir,
		       VerilogDclArg *arg,
                       VerilogAttributeStmtSeq* attribute_stmts,
		       int line)
{
  dcl_count_++;
  return new VerilogDcl(dir, arg, attribute_stmts, line);
}

VerilogDclBus *
VerilogReader::makeDclBus(PortDirection *dir,
			  int from_index,
			  int to_index,
			  VerilogDclArg *arg,
                          VerilogAttributeStmtSeq* attribute_stmts,
			  int line)
{
  dcl_bus_count_++;
  return new VerilogDclBus(dir, from_index, to_index, arg, attribute_stmts,
                           line);
}

VerilogDclBus *
VerilogReader::makeDclBus(PortDirection *dir,
			  int from_index,
			  int to_index,
			  VerilogDclArgSeq *args,
                          VerilogAttributeStmtSeq* attribute_stmts,
			  int line)
{
  dcl_bus_count_++;
  return new VerilogDclBus(dir, from_index, to_index, args, attribute_stmts,
                           line);
}

VerilogDclArg *
VerilogReader::makeDclArg(const char *net_vname)
{
  dcl_arg_count_++;
  string net_name = netVerilogToSta(net_vname);
  VerilogDclArg *dcl =new VerilogDclArg(net_name.c_str());
  stringDelete(net_vname);
  return dcl;
}

VerilogDclArg *
VerilogReader::makeDclArg(VerilogAssign *assign)
{
  dcl_arg_count_++;
  return new VerilogDclArg(assign);
}

VerilogNetPartSelect *
VerilogReader::makeNetPartSelect(const char *net_vname,
				 int from_index,
				 int to_index)
{
  net_part_select_count_++;
  if (report_stmt_stats_)
    net_bus_names_ += strlen(net_vname) + 1;
  string net_name = netVerilogToSta(net_vname);
  VerilogNetPartSelect *select = new VerilogNetPartSelect(net_name.c_str(),
                                                          from_index,
                                                          to_index);
  stringDelete(net_vname);
  return select;
}

VerilogNetConstant *
VerilogReader::makeNetConstant(const char *constant)
{
  net_constant_count_++;
  return new VerilogNetConstant(constant, this);
}

VerilogNetScalar *
VerilogReader::makeNetScalar(const char *net_vname)
{
  net_scalar_count_++;
  if (report_stmt_stats_)
    net_scalar_names_ += strlen(net_vname) + 1;
  string net_name = netVerilogToSta(net_vname);
  VerilogNetScalar *scalar = new VerilogNetScalar(net_name.c_str());
  stringDelete(net_vname);
  return scalar;
}

VerilogNetBitSelect *
VerilogReader::makeNetBitSelect(const char *net_vname,
				int index)
{
  net_bit_select_count_++;
  if (report_stmt_stats_)
    net_bus_names_ += strlen(net_vname) + 1;
  string net_name = netVerilogToSta(net_vname);
  VerilogNetBitSelect *select = new VerilogNetBitSelect(net_name.c_str(), index);
  stringDelete(net_vname);
  return select;
}

VerilogAssign *
VerilogReader::makeAssign(VerilogNet *lhs,
			  VerilogNet *rhs,
			  int line)
{
  assign_count_++;
  return new VerilogAssign(lhs, rhs, line);
}

VerilogInst *
VerilogReader::makeModuleInst(const char *module_vname,
                              const char *inst_vname,
                              VerilogNetSeq *pins,
                              VerilogAttributeStmtSeq* attribute_stmts,
                              const int line)
{
  string module_name = moduleVerilogToSta(module_vname);
  string inst_name = instanceVerilogToSta(inst_vname);
  Cell *cell = network_->findAnyCell(module_name.c_str());
  LibertyCell *liberty_cell = nullptr;
  if (cell)
    liberty_cell = network_->libertyCell(cell);
  // Instances of liberty with scalar ports are special cased
  // to reduce the memory footprint of the verilog parser.
  if (liberty_cell
      && hasScalarNamedPortRefs(liberty_cell, pins)) {
    int port_count = network_->portBitCount(cell);
    const char **net_names = new const char *[port_count];
    for (int i = 0; i < port_count; i++)
      net_names[i] = nullptr;
    for (VerilogNet *vnet : *pins) {
      VerilogNetPortRefScalarNet *vpin =
	dynamic_cast<VerilogNetPortRefScalarNet*>(vnet);
      const char *port_name = vpin->name().c_str();
      const char *net_name = vpin->netName();
      // Steal the net name string.
      vpin->setNetName(nullptr);
      Port *port = network_->findPort(cell, port_name);
      LibertyPort *lport = network_->libertyPort(port);
      if (lport->isBus()) {
	LibertyPortMemberIterator member_iter(lport);
	lport = member_iter.next();
      }
      int pin_index = lport->pinIndex();
      const char *prev_net_name = net_names[pin_index];
      if (prev_net_name
	  && prev_net_name !=unconnected_net_name)
	// Repeated port reference.
	stringDelete(prev_net_name);
      net_names[pin_index]=(net_name == nullptr) ? unconnected_net_name : net_name;
      delete vpin;
      net_port_ref_scalar_net_count_--;
    }
    VerilogInst *inst = new VerilogLibertyInst(liberty_cell, inst_name.c_str(),
					       net_names, attribute_stmts, line);
    delete pins;
    if (report_stmt_stats_) {
      inst_names_ += inst_name.size() + 1;
      inst_lib_count_++;
      inst_lib_net_arrays_ += port_count;
    }
    stringDelete(module_vname);
    stringDelete(inst_vname);
    return inst;
  }
  else {
    VerilogInst *inst = new VerilogModuleInst(module_name.c_str(),
                                              inst_name.c_str(),
                                              pins,
                                              attribute_stmts,
                                              line);
    if (report_stmt_stats_) {
      inst_module_names_ += module_name.size() + 1;
      inst_names_ += inst_name.size() + 1;
      inst_mod_count_++;
    }
    stringDelete(module_vname);
    stringDelete(inst_vname);
    return inst;
  }
}

bool
VerilogReader::hasScalarNamedPortRefs(LibertyCell *liberty_cell,
				      VerilogNetSeq *pins)
{
  if (pins
      && pins->size() > 0
      && (*pins)[0]->isNamedPortRef()) {
    for (VerilogNet *vpin : *pins) {
      const char *port_name = vpin->name().c_str();
      LibertyPort *port = liberty_cell->findLibertyPort(port_name);
      if (port) {
	if (!(port->size() == 1
	      && (vpin->isNamedPortRefScalarNet())))
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
VerilogReader::makeNetNamedPortRefScalarNet(const char *port_vname)
{
  net_port_ref_scalar_net_count_++;
  if (report_stmt_stats_)
    port_names_ += strlen(port_vname) + 1;
  string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefScalarNet(port_name.c_str());
  stringDelete(port_vname);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefScalarNet(const char *port_vname,
					    const char *net_vname)
{
  net_port_ref_scalar_net_count_++;
  if (report_stmt_stats_) {
    if (net_vname)
      net_scalar_names_ += strlen(net_vname) + 1;
    port_names_ += strlen(port_vname) + 1;
  }
  string port_name = portVerilogToSta(port_vname);
  string net_name = netVerilogToSta(net_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefScalarNet(port_name.c_str(),
                                                          net_name.c_str());
  stringDelete(port_vname);
  stringDelete(net_vname);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefBitSelect(const char *port_vname,
					    const char *bus_vname,
					    int index)
{
  net_port_ref_scalar_net_count_++;
  string bus_name = portVerilogToSta(bus_vname);
  string net_name = verilogBusBitName(bus_name.c_str(), index);
  if (report_stmt_stats_) {
    net_scalar_names_ += net_name.length() + 1;
    port_names_ += strlen(port_vname) + 1;
  }
  string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefScalarNet(port_name.c_str(),
                                                          net_name.c_str());
  stringDelete(port_vname);
  stringDelete(bus_vname);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefScalar(const char *port_vname,
					 VerilogNet *net)
{
  net_port_ref_scalar_count_++;
  if (report_stmt_stats_)
    port_names_ += strlen(port_vname) + 1;
  string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefScalar(port_name.c_str(), net);
  stringDelete(port_vname);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefBit(const char *port_vname,
				      int index,
				      VerilogNet *net)
{
  net_port_ref_bit_count_++;
  string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefBit(port_name.c_str(),
                                                    index, net);
  stringDelete(port_vname);
  return ref;
}

VerilogNetPortRef *
VerilogReader::makeNetNamedPortRefPart(const char *port_vname,
				       int from_index,
				       int to_index,
				       VerilogNet *net)
{
  net_port_ref_part_count_++;
  string port_name = portVerilogToSta(port_vname);
  VerilogNetPortRef *ref = new VerilogNetPortRefPart(port_name.c_str(),
                                                     from_index,
                                                     to_index, net);
  stringDelete(port_vname);
  return ref;
}

VerilogNetConcat *
VerilogReader::makeNetConcat(VerilogNetSeq *nets)
{
  concat_count_++;
  return new VerilogNetConcat(nets);
}

void
VerilogReader::incrLine()
{
  line_++;
}

#define printClassMemory(name, class_name, count) \
  report_->reportLine(" %-20s %9d * %3d = %6.1fMb\n",         \
                      name,                                   \
                      count,                                  \
                      static_cast<int>(sizeof(class_name)),   \
                      (count * sizeof(class_name) * 1e-6))

#define printStringMemory(name, count)	\
  report_->reportLine(" %-20s                   %6.1fMb", name, count * 1e-6)

void
VerilogReader::reportStmtCounts()
{
  if (debug_->check("verilog", 1)) {
    report_->reportLine("Verilog stats");
    printClassMemory("modules", VerilogModule, module_count_);
    printClassMemory("module insts", VerilogModuleInst, inst_mod_count_);
    printClassMemory("liberty insts", VerilogLibertyInst, inst_lib_count_);
    printClassMemory("liberty net arrays", char *, inst_lib_net_arrays_);
    printClassMemory("declarations", VerilogDcl, dcl_count_);
    printClassMemory("bus declarations", VerilogDclBus, dcl_bus_count_);
    printClassMemory("declaration args", VerilogDclArg, dcl_arg_count_);
    printClassMemory("port ref scalar", VerilogNetPortRefScalar,
		     net_port_ref_scalar_count_);
    printClassMemory("port ref scalar net", VerilogNetPortRefScalarNet,
		     net_port_ref_scalar_net_count_);
    printClassMemory("port ref bit", VerilogNetPortRefBit,
		     net_port_ref_bit_count_);
    printClassMemory("port ref part", VerilogNetPortRefPart,
		     net_port_ref_part_count_);
    printClassMemory("scalar nets", VerilogNetScalar, net_scalar_count_);
    printClassMemory("bus bit nets",VerilogNetBitSelect,net_bit_select_count_);
    printClassMemory("bus range nets", VerilogNetPartSelect,
		     net_part_select_count_);
    printClassMemory("constant nets", VerilogNetConstant, net_constant_count_);
    printClassMemory("concats", VerilogNetConcat, concat_count_);
    printClassMemory("assigns", VerilogAssign, assign_count_);
    printStringMemory("instance names", inst_names_);
    printStringMemory("instance mod names", inst_module_names_);
    printStringMemory("port names", port_names_);
    printStringMemory("net scalar names", net_scalar_names_);
    printStringMemory("net bus names", net_bus_names_);
  }
}

void
VerilogReader::error(int id,
                     const char *filename,
		     int line,
		     const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report()->vfileError(id, filename, line, fmt, args);
  va_end(args);
}

void
VerilogReader::warn(int id,
                    const char *filename,
		    int line,
		    const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report()->vfileWarn(id, filename, line, fmt, args);
  va_end(args);
}

string
VerilogReader::verilogName(VerilogModuleInst *mod_inst)
{
  return sta::instanceVerilogName(mod_inst->instanceName(),
				  network_->pathEscape());
}

string
VerilogReader::instanceVerilogName(const char *inst_name)
{
  return sta::netVerilogName(inst_name, network_->pathEscape());
}

string
VerilogReader::netVerilogName(const char *net_name)
{
  return sta::netVerilogName(net_name, network_->pathEscape());
}

////////////////////////////////////////////////////////////////

VerilogModule::VerilogModule(const char *name,
                             VerilogNetSeq *ports,
                             VerilogStmtSeq *stmts,
                             VerilogAttributeStmtSeq *attribute_stmts,
                             const char *filename,
                             int line,
                             VerilogReader *reader) :
  VerilogStmt(line),
  name_(stringCopy(name)),
  filename_(filename),
  ports_(ports),
  stmts_(stmts),
  attribute_stmts_(attribute_stmts)
{
  parseStmts(reader);
}

VerilogModule::~VerilogModule()
{
  ports_->deleteContents();
  delete ports_;
  stmts_->deleteContents();
  delete stmts_;
  attribute_stmts_->deleteContents();
  delete attribute_stmts_;
  stringDelete(name_);
}

void
VerilogModule::parseStmts(VerilogReader *reader)
{
  StringSet inst_names;
  for (VerilogStmt *stmt : *stmts_) {
    if (stmt->isDeclaration())
      parseDcl(dynamic_cast<VerilogDcl*>(stmt), reader);
    else if (stmt->isInstance())
      checkInstanceName(dynamic_cast<VerilogInst*>(stmt), inst_names,
			reader);
  }
}

void
VerilogModule::parseDcl(VerilogDcl *dcl,
			VerilogReader *reader)
{
  for (VerilogDclArg *arg : *dcl->args()) {
    const char *net_name = arg->netName();
    if (net_name) {
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
                 && (existing_dir->isOutput()
                     || existing_dir->isInput()
                     || existing_dir->isBidirect()))
          // supply0/supply1 dcl can be used as modifier for
          // input/output/inout dcls.
          dcl_map_[net_name] = dcl;
        else if (!dcl->direction()->isInternal()) {
          string net_vname = reader->netVerilogName(net_name);
          reader->warn(1395, filename_, dcl->line(),
                       "signal %s previously declared on line %d.",
                       net_vname.c_str(),
                       existing_dcl->line());
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
  const char *inst_name = inst->instanceName();
  if (inst_names.findKey(inst_name)) {
    int i = 1;
    const char *replacement_name = nullptr;
    do {
      if (replacement_name)
	stringDelete(replacement_name);
      replacement_name = stringPrint("%s_%d", inst_name, i++);
    } while (inst_names.findKey(replacement_name));
    string inst_vname = reader->instanceVerilogName(inst_name);
    reader->warn(1396, filename_, inst->line(),
		 "instance name %s duplicated - renamed to %s.",
		 inst_vname.c_str(),
		 replacement_name);
    inst_name = replacement_name;
    inst->setInstanceName(inst_name);
  }
  inst_names.insert(inst_name);
}

VerilogDcl *
VerilogModule::declaration(const char *net_name)
{
  return dcl_map_.findKey(net_name);
}

////////////////////////////////////////////////////////////////

VerilogStmt::VerilogStmt(int line) :
  line_(line)
{
}

VerilogInst::VerilogInst(const char *inst_name,
                         VerilogAttributeStmtSeq* attribute_stmts,
                         const int line) :
  VerilogStmt(line),
  inst_name_(stringCopy(inst_name)),
  attribute_stmts_(attribute_stmts)
{
}

VerilogInst::~VerilogInst()
{
  stringDelete(inst_name_);
  attribute_stmts_->deleteContents();
  delete attribute_stmts_;
}

void
VerilogInst::setInstanceName(const char *inst_name)
{
  stringDelete(inst_name_);
  inst_name_ = inst_name;
}

VerilogModuleInst::VerilogModuleInst(const char *module_name,
                                     const char *inst_name,
                                     VerilogNetSeq *pins,
                                     VerilogAttributeStmtSeq* attribute_stmts,
                                     int line) :
  VerilogInst(inst_name, attribute_stmts, line),
  module_name_(stringCopy(module_name)),
  pins_(pins)
{
}

VerilogModuleInst::~VerilogModuleInst()
{
  if (pins_) {
    pins_->deleteContents();
    delete pins_;
  }
  stringDelete(module_name_);
}

bool
VerilogModuleInst::hasPins()
{
  return pins_
    && pins_->size() > 0;

}

bool
VerilogModuleInst::namedPins()
{
  return pins_
    && pins_->size() > 0
    && (*pins_)[0]->isNamedPortRef();
}

VerilogLibertyInst::VerilogLibertyInst(LibertyCell *cell,
                                       const char *inst_name,
                                       const char **net_names,
                                       VerilogAttributeStmtSeq* attribute_stmts,
                                       const int line) :
  VerilogInst(inst_name, attribute_stmts, line),
  cell_(cell),
  net_names_(net_names)
{
}

VerilogLibertyInst::~VerilogLibertyInst()
{
  int port_count = cell_->portBitCount();
  for (int i = 0; i < port_count; i++) {
    const char *net_name = net_names_[i];
    if (net_name
	&& net_name != unconnected_net_name)
      stringDelete(net_name);
  }
  delete [] net_names_;
}

VerilogDcl::VerilogDcl(PortDirection *dir,
                       VerilogDclArgSeq *args,
                       VerilogAttributeStmtSeq* attribute_stmts,
                       int line) :
  VerilogStmt(line),
  dir_(dir),
  args_(args),
  attribute_stmts_(attribute_stmts)
{
}

VerilogDcl::VerilogDcl(PortDirection *dir,
                       VerilogDclArg *arg,
                       VerilogAttributeStmtSeq* attribute_stmts,
                       int line) :
  VerilogStmt(line),
  dir_(dir)
{
  args_ = new VerilogDclArgSeq;
  args_->push_back(arg);
  attribute_stmts_ = attribute_stmts;
}

VerilogDcl::~VerilogDcl()
{
  args_->deleteContents();
  delete args_;
  attribute_stmts_->deleteContents();
  delete attribute_stmts_;
}

void
VerilogDcl::appendArg(VerilogDclArg *arg)
{
  args_->push_back(arg);
}

const char *
VerilogDcl::portName()
{
  return (*args_)[0]->netName();
}

VerilogDclBus::VerilogDclBus(PortDirection *dir,
                             int from_index,
                             int to_index,
                             VerilogDclArgSeq *args,
                             VerilogAttributeStmtSeq* attribute_stmts,
                             int line) :
  VerilogDcl(dir, args, attribute_stmts, line),
  from_index_(from_index),
  to_index_(to_index)
{
}

VerilogDclBus::VerilogDclBus(PortDirection *dir,
                             int from_index,
                             int to_index,
                             VerilogDclArg *arg,
                             VerilogAttributeStmtSeq* attribute_stmts,
                             int line) :
  VerilogDcl(dir, arg, attribute_stmts, line),
  from_index_(from_index),
  to_index_(to_index)
{
}

int
VerilogDclBus::size() const
{
  return abs(to_index_ - from_index_) + 1;
}

VerilogDclArg::VerilogDclArg(const char *net_name) :
  net_name_(stringCopy(net_name)),
  assign_(nullptr)
{
}

VerilogDclArg::VerilogDclArg(VerilogAssign *assign) :
  net_name_(nullptr),
  assign_(assign)
{
}

VerilogDclArg::~VerilogDclArg()
{
  stringDelete(net_name_);
  delete assign_;
}

const char *
VerilogDclArg::netName()
{
  if (net_name_)
    return net_name_;
  else if (assign_)
    return assign_->lhs()->name().c_str();
  else
    return nullptr;
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
  VerilogNullNetNameIterator() {}
  virtual bool hasNext() { return false; }
  virtual const char *next() { return nullptr; }
};

class VerilogOneNetNameIterator : public VerilogNetNameIterator
{
public:
  explicit VerilogOneNetNameIterator(const char *name);
  explicit VerilogOneNetNameIterator(string name);
  virtual bool hasNext();
  virtual const char *next();

protected:
  const char *name_;
};

VerilogOneNetNameIterator::VerilogOneNetNameIterator(const char *name) :
  name_(name)
{
}

bool
VerilogOneNetNameIterator::hasNext()
{
  return name_ != nullptr;
}

const char *
VerilogOneNetNameIterator::next()
{
  const char *name = name_;
  name_ = nullptr;
  return name;
}

class VerilogBusNetNameIterator : public VerilogNetNameIterator
{
public:
  VerilogBusNetNameIterator(const char *bus_name,
			    int from_index,
			    int to_index);
  virtual bool hasNext();
  virtual const char *next();

protected:
  const char *bus_name_;
  int from_index_;
  int to_index_;
  int index_;
  string bit_name_;
};

VerilogBusNetNameIterator::VerilogBusNetNameIterator(const char *bus_name,
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
  return (to_index_ > from_index_
	  && index_ <= to_index_)
    || (to_index_ <= from_index_
	&& index_ >= to_index_);
}

const char *
VerilogBusNetNameIterator::next()
{
  bit_name_ = verilogBusBitName(bus_name_, index_);
  if (to_index_ > from_index_)
    index_++;
  else
    index_--;
  return bit_name_.c_str();
}

static string
verilogBusBitName(const char *bus_name,
                  int index)
{
  string bus_bit_name;
  stringPrint(bus_bit_name, "%s[%d]", bus_name, index);
  return bus_bit_name;
}

class VerilogConstantNetNameIterator : public VerilogNetNameIterator
{
public:
  VerilogConstantNetNameIterator(VerilogConstantValue *value,
				 const char *zero,
                                 const char *one);
  virtual bool hasNext();
  virtual const char *next();

private:
  VerilogConstantValue *value_;
  const char *zero_;
  const char *one_;
  int bit_index_;
};

VerilogConstantNetNameIterator::
VerilogConstantNetNameIterator(VerilogConstantValue *value,
			       const char *zero,
			       const char *one) :
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

const char *
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
  virtual ~VerilogNetConcatNameIterator();
  virtual bool hasNext();
  virtual const char *next();

private:
  VerilogModule *module_;
  VerilogReader *reader_;
  VerilogNetSeq::Iterator net_iter_;
  VerilogNetNameIterator *net_name_iter_;
};

VerilogNetConcatNameIterator::
VerilogNetConcatNameIterator(VerilogNetSeq *nets,
			     VerilogModule *module,
			     VerilogReader *reader) :
  module_(module),
  reader_(reader),
  net_iter_(nets),
  net_name_iter_(nullptr)
{
  if (net_iter_.hasNext())
    net_name_iter_ = net_iter_.next()->nameIterator(module, reader);
}

VerilogNetConcatNameIterator::~VerilogNetConcatNameIterator()
{
  delete net_name_iter_;
}

bool
VerilogNetConcatNameIterator::hasNext()
{
  return (net_name_iter_ && net_name_iter_->hasNext())
    || net_iter_.hasNext();
}

const char *
VerilogNetConcatNameIterator::next()
{
  if (net_name_iter_ && net_name_iter_->hasNext())
    return net_name_iter_->next();
  else {
    if (net_iter_.hasNext()) {
      VerilogNet *net = net_iter_.next();
      delete net_name_iter_;
      net_name_iter_ = net->nameIterator(module_, reader_);
      if (net_name_iter_ && net_name_iter_->hasNext())
	return net_name_iter_->next();
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

const string VerilogNetUnnamed::null_;

VerilogNetNamed::VerilogNetNamed(const char *name) :
  VerilogNet(),
  name_(name)
{
}

VerilogNetNamed::VerilogNetNamed(const string &name) :
  VerilogNet(),
  name_(name)
{
}

VerilogNetNamed::~VerilogNetNamed()
{
}

void
VerilogNetNamed::setName(const char *name)
{
  name_ = name;
}

VerilogNetScalar::VerilogNetScalar(const char *name) :
  VerilogNetNamed(name)
{
}

static int
verilogNetScalarSize(const char *name,
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
  return verilogNetScalarSize(name_.c_str(), module);
}

static VerilogNetNameIterator *
verilogNetScalarNameIterator(const char *name,
			     VerilogModule *module)
{
  if (name) {
    VerilogDcl *dcl = module->declaration(name);
    if (dcl && dcl->isBus()) {
      VerilogDclBus *dcl_bus = dynamic_cast<VerilogDclBus *>(dcl);
      return new VerilogBusNetNameIterator(name, dcl_bus->fromIndex(),
					   dcl_bus->toIndex());
    }
    else
      return new VerilogOneNetNameIterator(name);
  }
  else
    return new VerilogOneNetNameIterator(nullptr);
}

VerilogNetNameIterator *
VerilogNetScalar::nameIterator(VerilogModule *module,
			       VerilogReader *)
{
  return verilogNetScalarNameIterator(name_.c_str(), module);
}

VerilogNetBitSelect::VerilogNetBitSelect(const char *name,
					 int index) :
  VerilogNetNamed(verilogBusBitName(name, index)),
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
  return new VerilogOneNetNameIterator(name_.c_str());
}

VerilogNetPartSelect::VerilogNetPartSelect(const char *name,
					   int from_index,
					   int to_index):
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
  return new VerilogBusNetNameIterator(name_.c_str(), from_index_, to_index_);
}

VerilogNetConstant::VerilogNetConstant(const char *constant,
				       VerilogReader *reader)
{
  parseConstant(constant, reader);
}

void
VerilogNetConstant::parseConstant(const char *constant,
				  VerilogReader *reader)
{
  size_t constant_length = strlen(constant);
  char *tmp = new char[constant_length + 1];
  char *t = tmp;
  const char *c = constant;

  // Copy the constant size to tmp.
  while (*c && *c != '\'')
    *t++ = *c++;
  *t = '\0';

  // Read the constant size.
  size_t size = atol(tmp);
  value_ = new VerilogConstantValue(size);

  // Read the constant base.
  const char *base_ptr = c + 1;
  char base_ch = *base_ptr;
  switch (base_ch) {
  case 'b':
  case 'B':
    parseConstant(constant, constant_length, base_ptr, 2, 1);
    break;
  case 'o':
  case 'O':
    parseConstant(constant, constant_length, base_ptr, 8, 3);
    break;
  case 'h':
  case 'H':
    parseConstant(constant, constant_length, base_ptr, 16, 4);
    break;
  case 'd':
  case 'D':
    parseConstant10(base_ptr + 1, tmp, reader);
    break;
  default:
  case '\0':
    VerilogParse_error("unknown constant base.\n");
    break;
  }

  stringDelete(tmp);
  stringDelete(constant);
}

void
VerilogNetConstant::parseConstant(const char *constant,
				  size_t constant_length,
				  const char *base_ptr,
				  int base,
				  int digit_bit_count)
{
  // Scan the constant from LSD to MSD.
  size_t size = value_->size();
  char value_digit_str[2];
  char *end;
  value_digit_str[1] = '\0';
  const char *c = &constant[constant_length - 1];
  size_t bit = 0;
  while (bit < size) {
    char ch = (c > base_ptr) ? *c-- : '0';
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
VerilogNetConstant::parseConstant10(const char *constant_str,
				    char *tmp,
				    VerilogReader *reader)
{
  // Copy the constant skipping underscores.
  char *t = tmp;
  for (const char *c = constant_str; *c; c++) {
    char ch = *c;
    if (ch != '_')
      *t++ = ch;
  }
  *t = '\0';

  size_t size = value_->size();
  size_t length = strlen(tmp);
  size_t max_length = reader->constant10MaxLength();
  if (length > max_length
      || (length == max_length
	  && strcmp(tmp, reader->constant10Max()) > 0))
    reader->warn(1397, reader->filename(), reader->line(),
		 "base 10 constant greater than %s not supported.",
		 reader->constant10Max());
  else {
    char *end;
    VerilogConstant10 value = strtoull(tmp, &end, 10);
    VerilogConstant10 mask = 1;
    for (size_t bit = 0; bit < size; bit++) {
      (*value_)[bit] = (value & mask) != 0;
      mask = mask << 1;
    }
  }
}

VerilogNetConstant::~VerilogNetConstant()
{
  delete value_;
}

VerilogNetNameIterator *
VerilogNetConstant::nameIterator(VerilogModule *,
				 VerilogReader *reader)
{
  return new VerilogConstantNetNameIterator(value_,
					    reader->zeroNetName(),
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
  nets_->deleteContents();
  delete nets_;
}

int
VerilogNetConcat::size(VerilogModule *module)
{
  VerilogNetSeq::Iterator net_iter(nets_);
  int sz = 0;
  while (net_iter.hasNext()) {
    VerilogNet *net = net_iter.next();
    sz += net->size(module);
  }
  return sz;
}

VerilogNetNameIterator *
VerilogNetConcat::nameIterator(VerilogModule *module,
			       VerilogReader *reader)
{
  return new VerilogNetConcatNameIterator(nets_, module, reader);
}

VerilogNetPortRef::VerilogNetPortRef(const char *name) :
  VerilogNetScalar(name)
{
}

VerilogNetPortRefScalarNet::VerilogNetPortRefScalarNet(const char *name) :
  VerilogNetPortRef(name),
  net_name_(nullptr)
{
}

VerilogNetPortRefScalarNet::VerilogNetPortRefScalarNet(const char *name,
                                                       const char *net_name) :
  VerilogNetPortRef(name),
  net_name_(stringCopy(net_name))
{
}

VerilogNetPortRefScalarNet::~VerilogNetPortRefScalarNet()
{
  stringDelete(net_name_);
}

int
VerilogNetPortRefScalarNet::size(VerilogModule *module)
{
  // VerilogNetScalar::size
  VerilogDcl *dcl = nullptr;
  if (net_name_)
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

VerilogNetPortRefScalar::VerilogNetPortRefScalar(const char *name,
						 VerilogNet *net) :
  VerilogNetPortRef(name),
  net_(net)
{
}

VerilogNetPortRefScalar::~VerilogNetPortRefScalar()
{
  delete net_;
}

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

VerilogNetPortRefBit::VerilogNetPortRefBit(const char *name,
					   int index,
					   VerilogNet *net) :
  VerilogNetPortRefScalar(name, net),
  bit_name_(verilogBusBitName(name, index))
{
}

VerilogNetPortRefPart::VerilogNetPortRefPart(const char *name,
					     int from_index,
					     int to_index,
					     VerilogNet *net) :
  VerilogNetPortRefBit(name, from_index, net),
  to_index_(to_index)
{
}

const string &
VerilogNetPortRefPart::name() const
{
  return name_;
}

VerilogAttributeEntry::VerilogAttributeEntry(std::string key,
                                             std::string value) :
  key_(key),
  value_(value)
{
}


std::string
VerilogAttributeEntry::key()
{
  return key_;
}

std::string
VerilogAttributeEntry::value()
{
  return value_;
}

VerilogAttributeStmt::VerilogAttributeStmt(
    VerilogAttributeEntrySeq* attribute_sequence):
    attribute_sequence_(attribute_sequence)
{
}

VerilogAttributeStmt::~VerilogAttributeStmt()
{
  attribute_sequence_->deleteContents();
  delete attribute_sequence_;
}

VerilogAttributeEntrySeq*
VerilogAttributeStmt::attribute_sequence()
{
  return attribute_sequence_;
}


////////////////////////////////////////////////////////////////
//
// Link verilog network
//
////////////////////////////////////////////////////////////////

Instance *
linkVerilogNetwork(const char *top_cell_name,
		   bool make_black_boxes,
		   Report *report,
		   NetworkReader *)
{
  return verilog_reader->linkNetwork(top_cell_name, make_black_boxes, report);
}

// Verilog net name to network net map.
typedef Map<const char*, Net*, CharPtrLess> BindingMap;

class VerilogBindingTbl
{
public:
  VerilogBindingTbl(const char *zero_net_name_,
		    const char *one_net_name_);
  Net *ensureNetBinding(const char *net_name,
			Instance *inst,
			NetworkReader *network);
  Net *find(const char *name,
	    NetworkReader *network);
  void bind(const char *name,
	    Net *net);

private:
  const char *zero_net_name_;
  const char *one_net_name_;
  BindingMap map_;
};

Instance *
VerilogReader::linkNetwork(const char *top_cell_name,
			   bool make_black_boxes,
			   Report *report)
{
  if (library_) {
    Cell *top_cell = network_->findCell(library_, top_cell_name);
    VerilogModule *module = this->module(top_cell);
    if (module) {
      // Seed the recursion for expansion with the top level instance.
      Instance *top_instance = network_->makeInstance(top_cell, "", nullptr);
      VerilogBindingTbl bindings(zero_net_name_, one_net_name_);
      VerilogNetSeq::Iterator port_iter(module->ports());
      while (port_iter.hasNext()) {
	VerilogNet *mod_port = port_iter.next();
	VerilogNetNameIterator *net_name_iter = mod_port->nameIterator(module,
								       this);
	while (net_name_iter->hasNext()) {
	  const char *net_name = net_name_iter->next();
	  Port *port = network_->findPort(top_cell, net_name);
	  Net *net = bindings.ensureNetBinding(net_name, top_instance, network_);
	  // Guard against repeated port name.
	  if (network_->findPin(top_instance, port) == nullptr) {
	    Pin *pin = network_->makePin(top_instance, port, nullptr);
	    network_->makeTerm(pin, net);
	  }
	}
	delete net_name_iter;
      }
      makeModuleInstBody(module, top_instance, &bindings, make_black_boxes);
      bool errors = reportLinkErrors(report);
      deleteModules();
      if (errors) {
	network_->deleteInstance(top_instance);
	return nullptr;
      }
      else
	return top_instance;
    }
    else {
      report->error(1398, "%s is not a verilog module.", top_cell_name);
      return nullptr;
    }
  }
  else {
    report->error(1399, "%s is not a verilog module.", top_cell_name);
    return nullptr;
  }
}

void
VerilogReader::makeModuleInstBody(VerilogModule *module,
				  Instance *inst,
				  VerilogBindingTbl *bindings,
				  bool make_black_boxes)
{
  VerilogStmtSeq::Iterator stmt_iter(module->stmts());
  while (stmt_iter.hasNext()) {
    VerilogStmt *stmt = stmt_iter.next();
    if (stmt->isModuleInst())
      makeModuleInstNetwork(dynamic_cast<VerilogModuleInst*>(stmt),
			    inst, module, bindings, make_black_boxes);
    else if (stmt->isLibertyInst())
      makeLibertyInst(dynamic_cast<VerilogLibertyInst*>(stmt),
		      inst, module, bindings);
    else if (stmt->isDeclaration()) {
      VerilogDcl *dcl = dynamic_cast<VerilogDcl*>(stmt);
      PortDirection *dir = dcl->direction();
      VerilogDclArgSeq::Iterator arg_iter(dcl->args());
      while (arg_iter.hasNext()) {
	VerilogDclArg *arg = arg_iter.next();
	VerilogAssign *assign = arg->assign();
	if (assign)
	  mergeAssignNet(assign, module, inst, bindings);
	if (dir->isGround()) {
	  Net *net = bindings->ensureNetBinding(arg->netName(),inst,network_);
	  network_->addConstantNet(net, LogicValue::zero);
	}
	if (dir->isPower()) {
	  Net *net = bindings->ensureNetBinding(arg->netName(),inst,network_);
	  network_->addConstantNet(net, LogicValue::one);
	}
      }
    }
    else if (stmt->isAssign())
      mergeAssignNet(dynamic_cast<VerilogAssign*>(stmt), module, inst,
		     bindings);
  }
}

void
VerilogReader::makeModuleInstNetwork(VerilogModuleInst *mod_inst,
				     Instance *parent,
				     VerilogModule *parent_module,
				     VerilogBindingTbl *parent_bindings,
				     bool make_black_boxes)
{
  const char *module_name = mod_inst->moduleName();
  Cell *cell = network_->findAnyCell(module_name);
  if (cell == nullptr) {
    string inst_vname = verilogName(mod_inst);
    if (make_black_boxes) {
      cell = makeBlackBox(mod_inst, parent_module);
      linkWarn(198, parent_module->filename(), mod_inst->line(),
	       "module %s not found. Creating black box for %s.",
	       mod_inst->moduleName(),
	       inst_vname.c_str());
    }
    else
      linkError(199, parent_module->filename(), mod_inst->line(),
		"module %s not found for instance %s.",
		mod_inst->moduleName(),
		inst_vname.c_str());
  }
  if (cell) {
    LibertyCell *lib_cell = network_->libertyCell(cell);
    if (lib_cell)
      cell = network_->cell(lib_cell);
    Instance *inst = network_->makeInstance(cell, mod_inst->instanceName(),
					    parent);
    VerilogAttributeStmtSeq *attribute_stmts = mod_inst->attribute_stmts();
    for (VerilogAttributeStmt *stmt : *attribute_stmts) {
      for (VerilogAttributeEntry *entry : *stmt->attribute_sequence()) {
        network_->setAttribute(inst, entry->key(), entry->value());
      }
    }

    if (lib_cell) {
      // Make all pins so timing arcs are built.
      LibertyCellPortBitIterator port_iter(lib_cell);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	network_->makePin(inst, reinterpret_cast<Port*>(port), nullptr);
      }
    }
    bool is_leaf = network_->isLeaf(cell);
    VerilogBindingTbl bindings(zero_net_name_, one_net_name_);
    if (mod_inst->hasPins()) {
      if (mod_inst->namedPins())
	makeNamedInstPins(cell, inst, mod_inst, &bindings, parent,
			  parent_module, parent_bindings, is_leaf);
      else
	makeOrderedInstPins(cell, inst, mod_inst, &bindings, parent,
			    parent_module, parent_bindings, is_leaf);
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
  string inst_vname = verilogName(mod_inst);
  VerilogNetSeq::Iterator pin_iter(mod_inst->pins());
  while (pin_iter.hasNext()) {
    VerilogNetPortRef *vpin = dynamic_cast<VerilogNetPortRef*>(pin_iter.next());
    const char *port_name = vpin->name().c_str();
    Port *port = network_->findPort(cell, port_name);
    if (port) {
      if (vpin->hasNet()
	  && network_->size(port) != vpin->size(parent_module)) {
	linkWarn(200, parent_module->filename(), mod_inst->line(),
		 "instance %s port %s size %d does not match net size %d.",
		 inst_vname.c_str(),
		 network_->name(port),
		 network_->size(port),
		 vpin->size(parent_module));
      }
      else {
	VerilogNetNameIterator *net_name_iter =
	  vpin->nameIterator(parent_module, this);
	if (network_->hasMembers(port)) {
	  PortMemberIterator *port_iter = network_->memberIterator(port);
	  while (port_iter->hasNext()) {
	    Port *port = port_iter->next();
	    makeInstPin(inst, port, net_name_iter, bindings,
			parent, parent_bindings, is_leaf);
	  }
	  delete port_iter;
	}
	else {
	  makeInstPin(inst, port, net_name_iter, bindings,
		      parent, parent_bindings, is_leaf);
	}
	delete net_name_iter;
      }
    }
    else {
      LibertyPgPort *pg_port = nullptr;
      LibertyCell *lib_cell = network_->libertyCell(cell);
      if (lib_cell)
        pg_port = lib_cell->findPgPort(port_name);
      // Do not warn about connections to pg ports (which are ignored).
      if (pg_port == nullptr) {
        linkWarn(201, parent_module->filename(), mod_inst->line(),
                 "instance %s port %s not found.",
                 inst_vname.c_str(),
                 port_name);
      }
    }
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
  VerilogNetSeq::Iterator pin_iter(mod_inst->pins());
  while (pin_iter.hasNext() && port_iter->hasNext()) {
    VerilogNet *net = pin_iter.next();
    Port *port = port_iter->next();
    if (network_->size(port) != net->size(parent_module)) {
      string inst_vname = verilogName(mod_inst);
      linkWarn(202, parent_module->filename(), mod_inst->line(),
	       "instance %s port %s size %d does not match net size %d.",
	       inst_vname.c_str(),
	       network_->name(port),
	       network_->size(port),
	       net->size(parent_module));
    }
    else {
      VerilogNetNameIterator *net_name_iter=net->nameIterator(parent_module,
							      this);
      if (network_->isBus(port)) {
	PortMemberIterator *member_iter = network_->memberIterator(port);
	while (member_iter->hasNext() && net_name_iter->hasNext()) {
	  Port *port = member_iter->next();
	  makeInstPin(inst, port, net_name_iter, bindings,
		      parent, parent_bindings, is_leaf);
	}
	delete member_iter;
      }
      else
	makeInstPin(inst, port, net_name_iter, bindings,
		    parent, parent_bindings, is_leaf);
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
  const char *net_name = nullptr;
  if (net_name_iter->hasNext())
    net_name = net_name_iter->next();
  makeInstPin(inst, port, net_name, bindings, parent, parent_bindings,
	      is_leaf);
}

void
VerilogReader::makeInstPin(Instance *inst,
			   Port *port,
			   const char *net_name,
			   VerilogBindingTbl *bindings,
			   Instance *parent,
			   VerilogBindingTbl *parent_bindings,
			   bool is_leaf)
{
  Net *net = nullptr;
  if (net_name)
    net = parent_bindings->ensureNetBinding(net_name, parent, network_);
  if (is_leaf) {
    // Connect leaf pin to net.
    if (net)
      network_->connect(inst, port, net);
  }
  else {
    Pin *pin = network_->makePin(inst, port, net);
    if (!is_leaf && net) {
      const char *port_name = network_->name(port);
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
  Cell *cell = reinterpret_cast<Cell*>(lib_cell);
  Instance *inst = network_->makeInstance(cell, lib_inst->instanceName(),
					  parent);
  VerilogAttributeStmtSeq *attribute_stmts = lib_inst->attribute_stmts();
  for (VerilogAttributeStmt *stmt : *attribute_stmts) {
    for (VerilogAttributeEntry *entry : *stmt->attribute_sequence()) {
      network_->setAttribute(inst, entry->key(), entry->value());
    }
  }
  const char **net_names = lib_inst->netNames();
  LibertyCellPortBitIterator port_iter(lib_cell);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    const char *net_name = net_names[port->pinIndex()];
    // net_name may be the name of a single bit bus.
    if (net_name) {
      Net *net = nullptr;
      // If the pin is unconnected (ie, .A()) make the pin but not the net.
      if (net_name != unconnected_net_name) {
	VerilogDcl *dcl = parent_module->declaration(net_name);
	// Check for single bit bus reference .A(BUS) -> .A(BUS[LSB]).
	if (dcl && dcl->isBus()) {
	  VerilogDclBus *dcl_bus = dynamic_cast<VerilogDclBus *>(dcl);
	  // Bus is only 1 bit wide.
	  string bus_name = verilogBusBitName(net_name, dcl_bus->fromIndex());
	  net = parent_bindings->ensureNetBinding(bus_name.c_str(), parent, network_);
	}
	else
	  net = parent_bindings->ensureNetBinding(net_name, parent, network_);
      }
      network_->makePin(inst, reinterpret_cast<Port*>(port), net);
    }
    else
      // Make unconnected pin.
      network_->makePin(inst, reinterpret_cast<Port*>(port), nullptr);
  }
}

////////////////////////////////////////////////////////////////

Cell *
VerilogReader::makeBlackBox(VerilogModuleInst *mod_inst,
			    VerilogModule *parent_module)
{
  const char *module_name = mod_inst->moduleName();
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
  VerilogNetSeq::Iterator pin_iter(mod_inst->pins());
  while (pin_iter.hasNext()) {
    VerilogNetNamed *vpin = dynamic_cast<VerilogNetNamed*>(pin_iter.next());
    const char *port_name = vpin->name().c_str();
    size_t size = vpin->size(parent_module);
    Port *port = (size == 1)
      ? network_->makePort(cell, port_name)
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
  VerilogNetSeq::Iterator pin_iter(mod_inst->pins());
  while (pin_iter.hasNext()) {
    VerilogNet *net = pin_iter.next();
    size_t size = net->size(parent_module);
    char *port_name = stringPrint("p_%d", port_index);
    Port *port = (size == 1)
      ? network_->makePort(cell, port_name)
      : network_->makeBusPort(cell, port_name, size - 1, 0);
    stringDelete(port_name);
    network_->setDirection(port, PortDirection::unknown());
    port_index++;
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
      const char *lhs_name = lhs_iter->next();
      const char *rhs_name = rhs_iter->next();
      Net *lhs_net = bindings->ensureNetBinding(lhs_name, inst, network_);
      Net *rhs_net = bindings->ensureNetBinding(rhs_name, inst, network_);
      // Merge lower level net into higher level net so that deleting
      // instances from the bottom up does not reference deleted nets
      // by referencing the mergedInto field.
      if (hierarchyLevel(lhs_net,network_) >= hierarchyLevel(rhs_net,network_))
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
	     "assign left hand side size %d not equal right hand size %d.",
	     lhs->size(module),
	     rhs->size(module));
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

VerilogBindingTbl::VerilogBindingTbl(const char *zero_net_name,
				     const char *one_net_name) :
  zero_net_name_(zero_net_name),
  one_net_name_(one_net_name)
{
}

// Follow the merged_into pointers rather than update the
// binding tables up the call tree when nodes are merged
// because the name changes up the hierarchy.
Net *
VerilogBindingTbl::find(const char *name, NetworkReader *network)
{
  Net *net = map_.findKey(name);
  while (net && network->mergedInto(net))
    net = network->mergedInto(net);
  return net;
}

void
VerilogBindingTbl::bind(const char *name,
			Net *net)
{
  map_[name] = net;
}

Net *
VerilogBindingTbl::ensureNetBinding(const char *net_name,
				    Instance *inst,
				    NetworkReader *network)
{
  Net *net = find(net_name, network);
  if (net == nullptr) {
    net = network->makeNet(net_name, inst);
    map_[network->name(net)] = net;
    if (stringEq(net_name, zero_net_name_))
      network->addConstantNet(net, LogicValue::zero);
    if (stringEq(net_name, one_net_name_))
      network->addConstantNet(net, LogicValue::one);
  }
  return net;
}

////////////////////////////////////////////////////////////////

void
VerilogReader::linkWarn(int id,
                        const char *filename,
			int line,
			const char *msg, ...)
{
  va_list args;
  va_start(args, msg);
  char *msg_str = stringPrintArgs(msg, args);
  VerilogError *error = new VerilogError(id, filename, line, msg_str, true);
  link_errors_.push_back(error);
  va_end(args);
}

void
VerilogReader::linkError(int id,
                         const char *filename,
			 int line,
			 const char *msg, ...)
{
  va_list args;
  va_start(args, msg);
  char *msg_str = stringPrintArgs(msg, args);
  VerilogError *error = new VerilogError(id, filename, line, msg_str, false);
  link_errors_.push_back(error);
  va_end(args);
}

bool
VerilogReader::reportLinkErrors(Report *report)
{
  // Sort errors so they are in line number order rather than the order
  // they are discovered.
  sort(link_errors_, VerilogErrorCmp());
  bool errors = false;
  VerilogErrorSeq::Iterator error_iter(link_errors_);
  while (error_iter.hasNext()) {
    VerilogError *error = error_iter.next();
    // Report as warnings to avoid throwing.
    report->fileWarn(error->id(), error->filename(), error->line(), "%s", error->msg());
    errors |= !error->warn();
    delete error;
  }
  link_errors_.clear();
  return errors;
}

} // namespace

////////////////////////////////////////////////////////////////
// Global namespace

void verilogFlushBuffer();

int
VerilogParse_error(const char *msg)
{
  verilogFlushBuffer();
  sta::verilog_reader->report()->fileError(164, sta::verilog_reader->filename(),
					   sta::verilog_reader->line(), "%s", msg);
  return 0;
}
