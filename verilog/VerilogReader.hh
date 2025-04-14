// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#pragma once

#include <string>

#include "StringSet.hh"
#include "Vector.hh"
#include "Map.hh"
#include "NetworkClass.hh"

namespace sta {

class VerilogScanner;
class VerilogParse;
class Debug;
class Report;
class VerilogAttrEntry;
class VerilogAttrStmt;
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

typedef Map<Cell*, VerilogModule*> VerilogModuleMap;
typedef Vector<VerilogStmt*> VerilogStmtSeq;
typedef Vector<VerilogNet*> VerilogNetSeq;
typedef Vector<VerilogDclArg*> VerilogDclArgSeq;
typedef Vector<VerilogAttrStmt*> VerilogAttrStmtSeq;
typedef Vector<VerilogAttrEntry*> VerilogAttrEntrySeq;
typedef Vector<VerilogError*> VerilogErrorSeq;

class VerilogReader
{
public:
  VerilogReader(NetworkReader *network);
  ~VerilogReader();
  bool read(const char *filename);

  void makeModule(const std::string *module_name,
                  VerilogNetSeq *ports,
                  VerilogStmtSeq *stmts,
                  VerilogAttrStmtSeq *attr_stmts,
                  int line);
  void makeModule(const std::string *module_name,
                  VerilogStmtSeq *port_dcls,
                  VerilogStmtSeq *stmts,
                  VerilogAttrStmtSeq *attr_stmts,
                  int line);
  VerilogDcl *makeDcl(PortDirection *dir,
                      VerilogDclArgSeq *args,
                      VerilogAttrStmtSeq *attr_stmts,
                      int line);
  VerilogDcl *makeDcl(PortDirection *dir,
                      VerilogDclArg *arg,
                      VerilogAttrStmtSeq *attr_stmts,
                      int line);
  VerilogDclArg *makeDclArg(const std::string *net_name);
  VerilogDclArg*makeDclArg(VerilogAssign *assign);
  VerilogDclBus *makeDclBus(PortDirection *dir,
                            int from_index,
                            int to_index,
                            VerilogDclArg *arg,
                            VerilogAttrStmtSeq *attr_stmts,
                            int line);
  VerilogDclBus *makeDclBus(PortDirection *dir,
                            int from_index,
                            int to_index,
                            VerilogDclArgSeq *args,
                            VerilogAttrStmtSeq *attr_stmts,
                            int line);
  VerilogInst *makeModuleInst(const std::string *module_name,
                              const std::string *inst_name,
                              VerilogNetSeq *pins,
                              VerilogAttrStmtSeq *attr_stmts,
                              const int line);
  VerilogAssign *makeAssign(VerilogNet *lhs,
			    VerilogNet *rhs,
			    int line);
  VerilogNetScalar *makeNetScalar(const std::string *name);
  VerilogNetPortRef *makeNetNamedPortRefScalarNet(const std::string *port_vname);
  VerilogNetPortRef *makeNetNamedPortRefScalarNet(const std::string *port_name,
						  const std::string *net_name);
  VerilogNetPortRef *makeNetNamedPortRefBitSelect(const std::string *port_name,
						  const std::string *bus_name,
						  int index);
  VerilogNetPortRef *makeNetNamedPortRefScalar(const std::string *port_name,
					       VerilogNet *net);
  VerilogNetPortRef *makeNetNamedPortRefBit(const std::string *port_name,
					    int index,
					    VerilogNet *net);
  VerilogNetPortRef *makeNetNamedPortRefPart(const std::string *port_name,
					     int from_index,
					     int to_index,
					     VerilogNet *net);
  VerilogNetConcat *makeNetConcat(VerilogNetSeq *nets);
  VerilogNetConstant *makeNetConstant(const std::string *constant,
                                  int line);
  VerilogNetBitSelect *makeNetBitSelect(const std::string *name,
					int index);
  VerilogNetPartSelect *makeNetPartSelect(const std::string *name,
					  int from_index,
					  int to_index);
  VerilogModule *module(Cell *cell);
  Instance *linkNetwork(const char *top_cell_name,
			bool make_black_boxes,
                        bool delete_modules);
  const char *filename() const { return filename_.c_str(); }
  void incrLine();
  Report *report() const { return report_; }
  void error(int id,
             const char *filename,
	     int line,
	     const char *fmt, ...);
  void warn(int id,
            const char *filename,
	    int line,
	    const char *fmt, ...);
  const std::string &zeroNetName() const { return zero_net_name_; }
  const std::string &oneNetName() const { return one_net_name_; }
  void deleteModules();
  void reportStmtCounts();
  const std::string &constant10Max() const { return constant10_max_; }

protected:
  void init(const char *filename);
  void makeCellPorts(Cell *cell,
		     VerilogModule *module,
		     VerilogNetSeq *ports);
  Port *makeCellPort(Cell *cell,
		     VerilogModule *module,
		     const std::string &port_name);
  void makeNamedPortRefCellPorts(Cell *cell,
				 VerilogModule *module,
				 VerilogNet *mod_port,
				 StdStringSet &port_names);
  void checkModuleDcls(VerilogModule *module,
		       std::set<std::string> &port_names);
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
		   const std::string &net_name,
		   VerilogBindingTbl *bindings,
		   Instance *parent,
		   VerilogBindingTbl *parent_bindings,
		   bool is_leaf);
  void linkWarn(int id,
                const char *filename,
		int line,
		const char *msg, ...)
    __attribute__((format (printf, 5, 6)));
  void linkError(int id,
                 const char *filename,
		 int line,
		 const char *msg, ...)
    __attribute__((format (printf, 5, 6)));
  bool reportLinkErrors();
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

  std::string filename_;
  Report *report_;
  Debug *debug_;
  NetworkReader *network_;

  Library *library_;
  int black_box_index_;
  VerilogModuleMap module_map_;
  VerilogErrorSeq link_errors_;
  const std::string zero_net_name_;
  const std::string one_net_name_;
  std::string constant10_max_;
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

} // namespace sta
