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

#pragma once

#include "DisallowCopyAssign.hh"
#include "Zlib.hh"
#include "Vector.hh"
#include "Map.hh"
#include "StringSeq.hh"
#include "StringSet.hh"
#include "NetworkClass.hh"

// Global namespace.
#define YY_INPUT(buf,result,max_size) \
  sta::verilog_reader->getChars(buf, result, max_size)

int
VerilogParse_error(const char *msg);

namespace sta {

class Debug;
class Report;
class VerilogReader;
class VerilogStmt;
class VerilogNet;
class VerilogNetScalar;
class VerilogModule;
class VerilogInst;
class VerilogModuleInst;
class VerilogLibertyInst;
class VerilogDcl;
class VerilogDclBus;
class VerilogDclArg;
class VerilogAssign;
class VerilogNetConcat;
class VerilogNetConstant;
class VerilogNetBitSelect;
class VerilogNetPartSelect;
class StringRegistry;
class VerilogBindingTbl;
class VerilogNetNameIterator;
class VerilogNetPortRef;
class VerilogError;
class LibertyCell;

typedef Vector<VerilogNet*> VerilogNetSeq;
typedef Vector<VerilogStmt*> VerilogStmtSeq;
typedef Map<const char*, VerilogDcl*, CharPtrLess> VerilogDclMap;
typedef Vector<VerilogDclArg*> VerilogDclArgSeq;
typedef Map<Cell*, VerilogModule*> VerilogModuleMap;
typedef Vector<VerilogError*> VerilogErrorSeq;
typedef Vector<bool> VerilogConstantValue;
// Max base 10 constant net value (for strtoll).
typedef unsigned long long VerilogConstant10;

extern VerilogReader *verilog_reader;

class VerilogReader
{
public:
  explicit VerilogReader(NetworkReader *network);
  ~VerilogReader();
  bool read(const char *filename);
  // flex YY_INPUT yy_n_chars arg changed definition from int to size_t,
  // so provide both forms.
  void getChars(char *buf,
		size_t &result,
		size_t max_size);
  void getChars(char *buf,
		int &result,
		size_t max_size);
  void makeModule(const char *module_name,
		  VerilogNetSeq *ports,
		  VerilogStmtSeq *stmts,
		  int line);
  void makeModule(const char *module_name,
		  VerilogStmtSeq *port_dcls,
		  VerilogStmtSeq *stmts,
		  int line);
  VerilogDcl *makeDcl(PortDirection *dir,
		      VerilogDclArgSeq *args,
		      int line);
  VerilogDcl *makeDcl(PortDirection *dir,
		      VerilogDclArg *arg,
		      int line);
  VerilogDclArg *makeDclArg(const char *net_name);
  VerilogDclArg*makeDclArg(VerilogAssign *assign);
  VerilogDclBus *makeDclBus(PortDirection *dir,
			    int from_index,
			    int to_index,
			    VerilogDclArg *arg,
			    int line);
  VerilogDclBus *makeDclBus(PortDirection *dir,
			    int from_index,
			    int to_index,
			    VerilogDclArgSeq *args,
			    int line);
  VerilogInst *makeModuleInst(const char *module_name,
			      const char *inst_name,
			      VerilogNetSeq *pins,
			      const int line);
  VerilogAssign *makeAssign(VerilogNet *lhs,
			    VerilogNet *rhs,
			    int line);
  VerilogNetScalar *makeNetScalar(const char *name);
  VerilogNetPortRef *makeNetNamedPortRefScalarNet(const char *port_name,
						  const char *net_name);
  VerilogNetPortRef *makeNetNamedPortRefBitSelect(const char *port_name,
						  const char *bus_name,
						  int index);
  VerilogNetPortRef *makeNetNamedPortRefScalar(const char *port_name,
					       VerilogNet *net);
  VerilogNetPortRef *makeNetNamedPortRefBit(const char *port_name,
					    int index,
					    VerilogNet *net);
  VerilogNetPortRef *makeNetNamedPortRefPart(const char *port_name,
					     int from_index,
					     int to_index,
					     VerilogNet *net);
  VerilogNetConcat *makeNetConcat(VerilogNetSeq *nets);
  VerilogNetConstant *makeNetConstant(const char *constant);
  VerilogNetBitSelect *makeNetBitSelect(const char *name,
					int index);
  VerilogNetPartSelect *makeNetPartSelect(const char *name,
					  int from_index,
					  int to_index);
  VerilogModule *module(Cell *cell);
  Instance *linkNetwork(const char *top_cell_name,
			bool make_black_boxes,
			Report *report);
  int line() const { return line_; }
  const char *filename() const { return filename_; }
  void incrLine();
  Report *report() const { return report_; }
  void error(const char *filename,
	     int line,
	     const char *fmt, ...);
  void warn(const char *filename,
	    int line,
	    const char *fmt, ...);
  const char *zeroNetName() const { return zero_net_name_; }
  const char *oneNetName() const { return one_net_name_; }
  void deleteModules();
  void reportStmtCounts();
  const char *constant10Max() const { return constant10_max_; }
  size_t constant10MaxLength() const { return constant10_max_length_; }
  const char *
  verilogName(VerilogModuleInst *inst);
  const char *
  instanceVerilogName(const char *inst_name);
  const char *
  netVerilogName(const char *net_name);

protected:
  DISALLOW_COPY_AND_ASSIGN(VerilogReader);
  void init(const char *filename);
  void makeCellPorts(Cell *cell,
		     VerilogModule *module,
		     VerilogNetSeq *ports);
  Port *makeCellPort(Cell *cell,
		     VerilogModule *module,
		     const char *port_name);
  void makeNamedPortRefCellPorts(Cell *cell,
				 VerilogModule *module,
				 VerilogNet *mod_port,
				 StringSet &port_names);
  void checkModuleDcls(VerilogModule *module,
		       StringSet &port_names);
  void makeModuleInstBody(VerilogModule *module,
			  Instance *inst,
			  VerilogBindingTbl *bindings,
			  bool make_black_boxes);
  void makeModuleInstNetwork(VerilogModuleInst *mod_inst,
			     Instance *parent,
			     VerilogModule *parent_module,
			     VerilogBindingTbl *parent_bindings,
			     bool make_black_boxes);
  void makeLibertyInst(VerilogLibertyInst *lib_inst,
		       Instance *parent,
		       VerilogModule *parent_module,
		       VerilogBindingTbl *parent_bindings);
  void bindGlobalNets(VerilogBindingTbl *bindings);
  void makeNamedInstPins1(Cell *cell,
			  Instance *inst,
			  VerilogModuleInst *mod_inst,
			  VerilogBindingTbl *bindings,
			  Instance *parent,
			  VerilogBindingTbl *parent_bindings,
			  bool is_leaf);
  void makeNamedInstPins(Cell *cell,
			 Instance *inst,
			 VerilogModuleInst *mod_inst,
			 VerilogBindingTbl *bindings,
			 Instance *parent,
			 VerilogModule *parent_module,
			 VerilogBindingTbl *parent_bindings,
			 bool is_leaf);
  void makeOrderedInstPins(Cell *cell,
			   Instance *inst,
			   VerilogModuleInst *mod_inst,
			   VerilogBindingTbl *bindings,
			   Instance *parent,
			   VerilogModule *parent_module,
			   VerilogBindingTbl *parent_bindings,
			   bool is_leaf);
  void mergeAssignNet(VerilogAssign *assign,
		      VerilogModule *module,
		      Instance *inst,
		      VerilogBindingTbl *bindings);
  void makeInstPin(Instance *inst,
		   Port *port,
		   VerilogNetNameIterator *net_name_iter,
		   VerilogBindingTbl *bindings,
		   Instance *parent,
		   VerilogBindingTbl *parent_bindings,
		   bool is_leaf);
  void makeInstPin(Instance *inst,
		   Port *port,
		   const char *net_name,
		   VerilogBindingTbl *bindings,
		   Instance *parent,
		   VerilogBindingTbl *parent_bindings,
		   bool is_leaf);
  void linkWarn(const char *filename,
		int line,
		const char *msg, ...);
  void linkError(const char *filename,
		 int line,
		 const char *msg, ...);
  bool reportLinkErrors(Report *report);
  bool haveLinkErrors();
  Cell *makeBlackBox(VerilogModuleInst *mod_inst,
		     VerilogModule *parent_module);
  void makeBlackBoxNamedPorts(Cell *cell,
			      VerilogModuleInst *mod_inst,
			      VerilogModule *parent_module);
  void makeBlackBoxOrderedPorts(Cell *cell,
				VerilogModuleInst *mod_inst,
				VerilogModule *parent_module);
  bool isBlackBox(Cell *cell);
  bool hasScalarNamedPortRefs(LibertyCell *liberty_cell,
			      VerilogNetSeq *pins);

  Report *report_;
  Debug *debug_;
  NetworkReader *network_;

  const char *filename_;
  int line_;
  gzFile stream_;

  Library *library_;
  int black_box_index_;
  VerilogModuleMap module_map_;
  VerilogErrorSeq link_errors_;
  const char *zero_net_name_;
  const char *one_net_name_;
  const char *constant10_max_;
  size_t constant10_max_length_;
  ViewType *view_type_;
  bool report_stmt_stats_;
  int module_count_;
  int inst_mod_count_;
  int inst_lib_count_;
  int inst_lib_net_arrays_;
  int port_names_;
  int inst_module_names_;
  int inst_names_;
  int dcl_count_;
  int dcl_bus_count_;
  int dcl_arg_count_;
  int net_scalar_count_;
  int net_scalar_names_;
  int net_bus_names_;
  int net_part_select_count_;
  int net_bit_select_count_;
  int net_port_ref_scalar_count_;
  int net_port_ref_scalar_net_count_;
  int net_port_ref_bit_count_;
  int net_port_ref_part_count_;
  int net_constant_count_;
  int assign_count_;
  int concat_count_;
};

class VerilogStmt
{
public:
  explicit VerilogStmt(int line);
  virtual ~VerilogStmt() {}
  virtual bool isInstance() const { return false; }
  virtual bool isModuleInst() const { return false; }
  virtual bool isLibertyInst() const { return false; }
  virtual bool isAssign() const { return false; }
  virtual bool isDeclaration() const { return false; }
  int line() const { return line_; }

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogStmt);

  int line_;
};

class VerilogModule : public VerilogStmt
{
public:
  VerilogModule(const char *name,
		VerilogNetSeq *ports,
		VerilogStmtSeq *stmts,
		const char *filename,
		int line,
		VerilogReader *reader);
  virtual ~VerilogModule();
  const char *name() { return name_; }
  const char *filename() { return filename_; }
  VerilogNetSeq *ports() { return ports_; }
  VerilogDcl *declaration(const char *net_name);
  VerilogStmtSeq *stmts() { return stmts_; }
  VerilogDclMap *declarationMap() { return &dcl_map_; }
  void parseDcl(VerilogDcl *dcl,
		VerilogReader *reader);

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogModule);
  void parseStmts(VerilogReader *reader);
  void checkInstanceName(VerilogInst *inst,
			 StringSet &inst_names,
			 VerilogReader *reader);

  const char *name_;
  const char *filename_;
  VerilogNetSeq *ports_;
  VerilogStmtSeq *stmts_;
  VerilogDclMap dcl_map_;
};

class VerilogDcl : public VerilogStmt
{
public:
  VerilogDcl(PortDirection *dir,
	     VerilogDclArgSeq *args,
	     int line);
  VerilogDcl(PortDirection *dir,
	     VerilogDclArg *arg,
	     int line);
  virtual ~VerilogDcl();
  const char *portName();
  virtual bool isBus() const { return false; }
  virtual bool isDeclaration() const { return true; }
  VerilogDclArgSeq *args() const { return args_; }
  void appendArg(VerilogDclArg *arg);
  PortDirection *direction() const { return dir_; }
  virtual int size() const { return 1; }
  void check();

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogDcl);

  PortDirection *dir_;
  VerilogDclArgSeq *args_;
};

class VerilogDclBus : public VerilogDcl
{
public:
  VerilogDclBus(PortDirection *dir,
		int from_index,
		int to_index,
		VerilogDclArgSeq *args,
		int line);
  VerilogDclBus(PortDirection *dir,
		int from_index,
		int to_index,
		VerilogDclArg *arg,
		int line);
  virtual bool isBus() const { return true; }
  int fromIndex() const { return from_index_; }
  int toIndex() const { return to_index_; }
  virtual int size() const;

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogDclBus);

  int from_index_;
  int to_index_;
};

// Declaratation arguments can be a net name or an assignment.
class VerilogDclArg
{
public:
  explicit VerilogDclArg(const char *net_name);
  explicit VerilogDclArg(VerilogAssign *assign);
  ~VerilogDclArg();
  const char *netName();
  VerilogAssign *assign() { return assign_; }

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogDclArg);

  const char *net_name_;
  VerilogAssign *assign_;
};

// Continuous assignment.
class VerilogAssign : public VerilogStmt
{
public:
  VerilogAssign(VerilogNet *lhs,
		VerilogNet *rhs,
		int line);
  virtual ~VerilogAssign();
  virtual bool isAssign() const { return true; }
  VerilogNet *lhs() const { return lhs_; }
  VerilogNet *rhs() const { return rhs_; }

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogAssign);

  VerilogNet *lhs_;
  VerilogNet *rhs_;
};

class VerilogInst : public VerilogStmt
{
public:
  VerilogInst(const char *inst_name,
		  const int line);
  virtual ~VerilogInst();
  virtual bool isInstance() const { return true; }
  const char *instanceName() const { return inst_name_; }
  void setInstanceName(const char *inst_name);

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogInst);

  const char *inst_name_;
};

class VerilogModuleInst : public VerilogInst
{
public:
  VerilogModuleInst(const char *module_name,
		    const char *inst_name,
		    VerilogNetSeq *pins,
		    const int line);
  virtual ~VerilogModuleInst();
  virtual bool isModuleInst() const { return true; }
  const char *moduleName() const { return module_name_; }
  VerilogNetSeq *pins() const { return pins_; }
  bool namedPins();
  bool hasPins();

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogModuleInst);

  const char *module_name_;
  VerilogNetSeq *pins_;
};

// Instance of liberty cell when all connections are single bit.
// Connections are an array of net name strings indexed by port pin
// index.
class VerilogLibertyInst : public VerilogInst
{
public:
  VerilogLibertyInst(LibertyCell *cell,
		     const char *inst_name,
		     const char **net_names,
		     const int line);
  virtual ~VerilogLibertyInst();
  virtual bool isLibertyInst() const { return true; }
  LibertyCell *cell() const { return cell_; }
  const char **netNames() const { return net_names_; }

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogLibertyInst);

  LibertyCell *cell_;
  const char **net_names_;
};

// Abstract base class for nets.
class VerilogNet
{
public:
  VerilogNet() {}
  virtual ~VerilogNet() {}
  virtual bool isNamed() = 0;
  virtual const char *name() = 0;
  virtual bool isNamedPortRef() { return false; }
  virtual bool isNamedPortRefScalarNet() const { return false; }
  virtual int size(VerilogModule *module) = 0;
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader) = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNet);
};

class VerilogNetUnnamed : public VerilogNet
{
public:
  VerilogNetUnnamed() {}
  virtual bool isNamed() { return false; }
  virtual const char *name() { return nullptr; }

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetUnnamed);
};

class VerilogNetNamed : public VerilogNet
{
public:
  explicit VerilogNetNamed(const char *name);
  virtual ~VerilogNetNamed();
  virtual bool isNamed() { return true; }
  virtual bool isScalar() const = 0;
  virtual const char *name() { return name_; }
  const char *baseName() { return name_; }
  void setName(const char *name);

protected:
  const char *name_;

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetNamed);
};

// Named net reference, which could be the name of a scalar or bus signal.
class VerilogNetScalar : public VerilogNetNamed
{
public:
  explicit VerilogNetScalar(const char *name);
  virtual bool isScalar() const { return true; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetScalar);
};

class VerilogNetBitSelect : public VerilogNetNamed
{
public:
  VerilogNetBitSelect(const char *name,
		      int index);
  virtual const char *name();
  int index() { return index_; }
  virtual bool isScalar() const { return false; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetBitSelect);

  int index_;
};

class VerilogNetPartSelect : public VerilogNetNamed
{
public:
  VerilogNetPartSelect(const char *name,
		       int from_index,
		       int to_index);
  virtual bool isScalar() const { return false; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);
  int fromIndex() const { return from_index_; }
  int toIndex() const { return to_index_; }

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetPartSelect);

  int from_index_;
  int to_index_;
};

class VerilogNetConstant : public VerilogNetUnnamed
{
public:
  VerilogNetConstant(const char *constant,
		     VerilogReader *reader);
  virtual ~VerilogNetConstant();
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetConstant);
  void parseConstant(const char *constant,
		     VerilogReader *reader);
  void parseConstant(const char *constant,
		     size_t constant_length,
		     const char *base_ptr,
		     int base,
		     int digit_bit_count);
  void parseConstant10(const char *constant_str,
		       char *tmp,
		       VerilogReader *reader);

  VerilogConstantValue *value_;
};

class VerilogNetConcat : public VerilogNetUnnamed
{
public:
  explicit VerilogNetConcat(VerilogNetSeq *nets);
  virtual ~VerilogNetConcat();
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetConcat);

  VerilogNetSeq *nets_;
};

// Module instance named port reference base class.
class VerilogNetPortRef : public VerilogNetScalar
{
public:
  explicit VerilogNetPortRef(const char *name);
  virtual bool isNamedPortRef() { return true; }
  virtual bool hasNet() = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetPortRef);
};

// Named scalar port reference to scalar net .port(net).
// This is special cased because it is so common.  The overhead
// of pointers to VerilogNet objects for the port name and net name
// is quite high.
class VerilogNetPortRefScalarNet : public VerilogNetPortRef
{
public:
  VerilogNetPortRefScalarNet(const char *name,
			     const char *net_name);
  virtual ~VerilogNetPortRefScalarNet();
  virtual bool isScalar() const { return true; }
  virtual bool isNamedPortRefScalarNet() const { return true; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);
  virtual bool hasNet() { return net_name_ != nullptr; }
  const char *netName() const { return net_name_; }
  void setNetName(const char *net_name) { net_name_ = net_name; }

private:
  const char *net_name_;
};

class VerilogNetPortRefScalar : public VerilogNetPortRef
{
public:
  VerilogNetPortRefScalar(const char *name,
			  VerilogNet *net);
  virtual ~VerilogNetPortRefScalar();
  virtual bool isScalar() const { return true; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);
  virtual bool hasNet() { return net_ != nullptr; }

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetPortRefScalar);

  VerilogNet *net_;
};

class VerilogNetPortRefBit : public VerilogNetPortRefScalar
{
public:
  VerilogNetPortRefBit(const char *name,
		       int index,
		       VerilogNet *net);
  virtual const char *name();

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetPortRefBit);

  int index_;
};

class VerilogNetPortRefPart : public VerilogNetPortRefBit
{
public:
  VerilogNetPortRefPart(const char *name,
			int from_index,
			int to_index,
			VerilogNet *net);
  virtual const char *name();
  int toIndex() const { return to_index_; }

private:
  DISALLOW_COPY_AND_ASSIGN(VerilogNetPortRefPart);

  int to_index_;
};

// Abstract class for iterating over the component nets of a net.
class VerilogNetNameIterator : public Iterator<const char*>
{
};

} // namespace
