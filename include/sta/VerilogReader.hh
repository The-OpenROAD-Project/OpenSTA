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

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <map>

#include "Format.hh"
#include "Report.hh"
#include "StringUtil.hh"
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
class LibertyCell;
class VerilogErrorCmp;

class VerilogError
{
public:
  VerilogError(int id,
               std::string_view filename,
               int line,
               std::string_view msg,
               bool warn);
  const char *msg() const { return msg_.c_str(); }
  std::string_view filename() const { return filename_; }
  int id() const { return id_; }
  int line() const { return line_; }
  bool warn() const { return warn_; }

private:
  int id_;
  std::string filename_;
  int line_;
  std::string msg_;
  bool warn_;

  friend class VerilogErrorCmp;
};

using VerilogModuleMap = std::map<Cell*, VerilogModule*>;
using VerilogStmtSeq = std::vector<VerilogStmt*>;
using VerilogNetSeq = std::vector<VerilogNet*>;
using VerilogDclArgSeq = std::vector<VerilogDclArg*>;
using VerilogAttrStmtSeq = std::vector<VerilogAttrStmt*>;
using VerilogAttrEntrySeq = std::vector<VerilogAttrEntry*>;
using VerilogErrorSeq = std::vector<VerilogError*>;

class VerilogReader
{
public:
  VerilogReader(NetworkReader *network);
  ~VerilogReader();
  bool read(std::string_view filename);

  void makeModule(std::string &&module_name,
                  VerilogNetSeq *ports,
                  VerilogStmtSeq *stmts,
                  VerilogAttrStmtSeq *attr_stmts,
                  int line);
  void makeModule(std::string &&module_name,
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
  VerilogDclArg *makeDclArg(std::string &&net_name);
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
  VerilogInst *makeModuleInst(std::string &&module_name,
                              std::string &&inst_name,
                              VerilogNetSeq *pins,
                              VerilogAttrStmtSeq *attr_stmts,
                              const int line);
  VerilogAssign *makeAssign(VerilogNet *lhs,
                            VerilogNet *rhs,
                            int line);
  VerilogNetScalar *makeNetScalar(std::string &&name);
  VerilogNetPortRef *makeNetNamedPortRefScalarNet(std::string &&port_vname);
  VerilogNetPortRef *makeNetNamedPortRefScalarNet(std::string &&port_name,
                                                  std::string &&net_name);
  VerilogNetPortRef *makeNetNamedPortRefBitSelect(std::string &&port_name,
                                                  std::string &&bus_name,
                                                  int index);
  VerilogNetPortRef *makeNetNamedPortRefScalar(std::string &&port_name,
                                               VerilogNet *net);
  VerilogNetPortRef *makeNetNamedPortRefBit(std::string &&port_name,
                                            int index,
                                            VerilogNet *net);
  VerilogNetPortRef *makeNetNamedPortRefPart(std::string &&port_name,
                                             int from_index,
                                             int to_index,
                                             VerilogNet *net);
  VerilogNetConcat *makeNetConcat(VerilogNetSeq *nets);
  VerilogNetConstant *makeNetConstant(std::string &&constant,
                                      int line);
  VerilogNetBitSelect *makeNetBitSelect(std::string &&name,
                                        int index);
  VerilogNetPartSelect *makeNetPartSelect(std::string &&name,
                                          int from_index,
                                          int to_index);
  VerilogModule *module(Cell *cell);
  Instance *linkNetwork(std::string_view top_cell_name,
                        bool make_black_boxes,
                        bool delete_modules);
  std::string_view filename() const { return filename_; }
  void incrLine();
  Report *report() const { return report_; }
  template <typename... Args>
  void error(int id,
             std::string_view filename,
             int line,
             std::string_view fmt,
             Args &&...args)
  {
    report_->fileError(id, filename, line, fmt, std::forward<Args>(args)...);
  }
  template <typename... Args>
  void warn(int id,
            std::string_view filename,
            int line,
            std::string_view fmt,
            Args &&...args)
  {
    report_->fileWarn(id, filename, line, fmt, std::forward<Args>(args)...);
  }
  const std::string &zeroNetName() const { return zero_net_name_; }
  const std::string &oneNetName() const { return one_net_name_; }
  void deleteModules();
  const std::string &constant10Max() const { return constant10_max_; }

protected:
  void init(std::string_view filename);
  void makeCellPorts(Cell *cell,
                     VerilogModule *module,
                     VerilogNetSeq *ports);
  Port *makeCellPort(Cell *cell,
                     VerilogModule *module,
                     const std::string &port_name);
  void makeNamedPortRefCellPorts(Cell *cell,
                                 VerilogModule *module,
                                 VerilogNet *mod_port,
                                 StringSet &port_names);
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
  template <typename... Args>
  void linkWarn(int id,
                std::string_view filename,
                int line,
                std::string_view msg,
                Args &&...args)
  {
    std::string msg_str = sta::formatRuntime(msg, std::forward<Args>(args)...);
    link_errors_.push_back(new VerilogError(id, filename, line, msg_str, true));
  }
  template <typename... Args>
  void linkError(int id,
                 std::string_view filename,
                 int line,
                 std::string_view msg,
                 Args &&...args)
  {
    std::string msg_str = sta::formatRuntime(msg, std::forward<Args>(args)...);
    link_errors_.push_back(new VerilogError(id, filename, line, msg_str, false));
  }
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
};

} // namespace sta
