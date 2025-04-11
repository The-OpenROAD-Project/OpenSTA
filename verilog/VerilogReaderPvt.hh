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

#include "Map.hh"
#include "Vector.hh"
#include "StringSeq.hh"

namespace sta {

typedef Map<std::string, VerilogDcl*> VerilogDclMap;
typedef Vector<bool> VerilogConstantValue;
typedef std::vector<std::string> StdStringSeq;

class VerilogStmt
{
public:
  VerilogStmt(int line);
  virtual ~VerilogStmt() {}
  virtual bool isInstance() const { return false; }
  virtual bool isModuleInst() const { return false; }
  virtual bool isLibertyInst() const { return false; }
  virtual bool isAssign() const { return false; }
  virtual bool isDeclaration() const { return false; }
  int line() const { return line_; }

private:
  int line_;
};

class VerilogModule : public VerilogStmt
{
public:
  VerilogModule(const std::string &name,
                VerilogNetSeq *ports,
                VerilogStmtSeq *stmts,
                VerilogAttrStmtSeq *attr_stmts,
                const std::string &filename,
                int line,
                VerilogReader *reader);
  virtual ~VerilogModule();
  const std::string &name() { return name_; }
  const char *filename() { return filename_.c_str(); }
  VerilogAttrStmtSeq *attrStmts() { return attr_stmts_; }
  VerilogNetSeq *ports() { return ports_; }
  VerilogDcl *declaration(const std::string &net_name);
  VerilogStmtSeq *stmts() { return stmts_; }
  VerilogDclMap *declarationMap() { return &dcl_map_; }
  void parseDcl(VerilogDcl *dcl,
		VerilogReader *reader);

private:
  void parseStmts(VerilogReader *reader);
  void checkInstanceName(VerilogInst *inst,
			 StdStringSet &inst_names,
			 VerilogReader *reader);

  std::string name_;
  std::string filename_;
  VerilogNetSeq *ports_;
  VerilogStmtSeq *stmts_;
  VerilogDclMap dcl_map_;
  VerilogAttrStmtSeq *attr_stmts_;
};

class VerilogDcl : public VerilogStmt
{
public:
  VerilogDcl(PortDirection *dir,
             VerilogDclArgSeq *args,
             VerilogAttrStmtSeq *attr_stmts,
             int line);
  VerilogDcl(PortDirection *dir,
             VerilogDclArg *arg,
             VerilogAttrStmtSeq *attr_stmts,
             int line);
  virtual ~VerilogDcl();
  const std::string &portName();
  virtual bool isBus() const { return false; }
  virtual bool isDeclaration() const { return true; }
  VerilogDclArgSeq *args() const { return args_; }
  void appendArg(VerilogDclArg *arg);
  PortDirection *direction() const { return dir_; }
  virtual int size() const { return 1; }
  void check();

private:
  PortDirection *dir_;
  VerilogDclArgSeq *args_;
  VerilogAttrStmtSeq *attr_stmts_;
};

class VerilogDclBus : public VerilogDcl
{
public:
  VerilogDclBus(PortDirection *dir,
                int from_index,
                int to_index,
                VerilogDclArgSeq *args,
                VerilogAttrStmtSeq *attr_stmts,
                int line);
  VerilogDclBus(PortDirection *dir,
                int from_index,
                int to_index,
                VerilogDclArg *arg,
                VerilogAttrStmtSeq *attr_stmts,
                int line);
  virtual bool isBus() const { return true; }
  int fromIndex() const { return from_index_; }
  int toIndex() const { return to_index_; }
  virtual int size() const;

private:
  int from_index_;
  int to_index_;
};

// Declaratation arguments can be a net name or an assignment.
class VerilogDclArg
{
public:
  VerilogDclArg(const std::string &net_name);
  VerilogDclArg(VerilogAssign *assign);
  ~VerilogDclArg();
  const std::string &netName();
  bool isNamed() const { return assign_ == nullptr; }
  VerilogAssign *assign() { return assign_; }

private:
  std::string net_name_;
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
  VerilogNet *lhs_;
  VerilogNet *rhs_;
};

class VerilogInst : public VerilogStmt
{
public:
  VerilogInst(const std::string &inst_name,
              VerilogAttrStmtSeq *attr_stmts,
              const int line);
  virtual ~VerilogInst();
  virtual bool isInstance() const { return true; }
  const std::string &instanceName() const { return inst_name_; }
  VerilogAttrStmtSeq *attrStmts() const { return attr_stmts_; }
  void setInstanceName(const std::string &inst_name);

private:
  std::string inst_name_;
  VerilogAttrStmtSeq *attr_stmts_;
};

class VerilogModuleInst : public VerilogInst
{
public:
  VerilogModuleInst(const std::string &module_name,
                    const std::string &inst_name,
                    VerilogNetSeq *pins,
                    VerilogAttrStmtSeq *attr_stmts,
                    const int line);
  virtual ~VerilogModuleInst();
  virtual bool isModuleInst() const { return true; }
  const std::string &moduleName() const { return module_name_; }
  VerilogNetSeq *pins() const { return pins_; }
  bool namedPins();
  bool hasPins();

private:
  std::string module_name_;
  VerilogNetSeq *pins_;
};

// Instance of liberty cell when all connections are single bit.
// Connections are an array of net name strings indexed by port pin
// index.
class VerilogLibertyInst : public VerilogInst
{
public:
  VerilogLibertyInst(LibertyCell *cell,
                     const std::string &inst_name,
                     const StdStringSeq &net_names,
                     VerilogAttrStmtSeq *attr_stmts,
                     const int line);
  virtual bool isLibertyInst() const { return true; }
  LibertyCell *cell() const { return cell_; }
  const StdStringSeq &netNames() const { return net_names_; }

private:
  LibertyCell *cell_;
  StdStringSeq net_names_;
};

// Abstract base class for nets.
class VerilogNet
{
public:
  VerilogNet() {}
  virtual ~VerilogNet() {}
  virtual bool isNamed() const = 0;
  virtual const std::string &name() const = 0;
  virtual bool isNamedPortRef() { return false; }
  virtual bool isNamedPortRefScalarNet() const { return false; }
  virtual int size(VerilogModule *module) = 0;
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader) = 0;
};

class VerilogNetUnnamed : public VerilogNet
{
public:
  VerilogNetUnnamed() {}
  bool isNamed() const override { return false; }
  const std::string &name() const override { return null_; }

private:
  static const std::string null_;
};

class VerilogNetNamed : public VerilogNet
{
public:
  VerilogNetNamed(const std::string &name);
  virtual ~VerilogNetNamed();
  bool isNamed() const override { return true; }
  virtual bool isScalar() const = 0;
  const std::string &name() const override { return name_; }

protected:
  std::string name_;
};

// Named net reference, which could be the name of a scalar or bus signal.
class VerilogNetScalar : public VerilogNetNamed
{
public:
  VerilogNetScalar(const std::string &name);
  virtual bool isScalar() const { return true; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);
};

class VerilogNetBitSelect : public VerilogNetNamed
{
public:
  VerilogNetBitSelect(const std::string &name,
		      int index);
  int index() { return index_; }
  virtual bool isScalar() const { return false; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);
private:
  int index_;
};

class VerilogNetPartSelect : public VerilogNetNamed
{
public:
  VerilogNetPartSelect(const std::string &name,
		       int from_index,
		       int to_index);
  virtual bool isScalar() const { return false; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);
  int fromIndex() const { return from_index_; }
  int toIndex() const { return to_index_; }

private:
  int from_index_;
  int to_index_;
};

class VerilogNetConstant : public VerilogNetUnnamed
{
public:
  VerilogNetConstant(const std::string *constant,
		     VerilogReader *reader,
                     int line);
  virtual ~VerilogNetConstant();
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);

private:
  void parseConstant(const std::string *constant,
		     VerilogReader *reader,
                     int line);
  void parseConstant(const std::string *constant,
		     size_t base_idx,
		     int base,
		     int digit_bit_count);
  void parseConstant10(const std::string *constant,
                       size_t base_idx,
		       VerilogReader *reader,
                       int line);

  VerilogConstantValue *value_;
};

class VerilogNetConcat : public VerilogNetUnnamed
{
public:
  VerilogNetConcat(VerilogNetSeq *nets);
  virtual ~VerilogNetConcat();
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);

private:
  VerilogNetSeq *nets_;
};

// Module instance named port reference base class.
class VerilogNetPortRef : public VerilogNetScalar
{
public:
  VerilogNetPortRef(const std::string &name);
  virtual bool isNamedPortRef() { return true; }
  virtual bool hasNet() = 0;
};

// Named scalar port reference to scalar net .port(net).
// This is special cased because it is so common.  The overhead
// of pointers to VerilogNet objects for the port name and net name
// is quite high.
class VerilogNetPortRefScalarNet : public VerilogNetPortRef
{
public:
  VerilogNetPortRefScalarNet(const std::string &name);
  VerilogNetPortRefScalarNet(const std::string &name,
			     const std::string &net_name);
  virtual bool isScalar() const { return true; }
  virtual bool isNamedPortRefScalarNet() const { return true; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);
  virtual bool hasNet() { return !net_name_.empty(); }
  const std::string &netName() const { return net_name_; }
  void setNetName(const std::string &net_name) { net_name_ = net_name; }

private:
  std::string net_name_;
};

class VerilogNetPortRefScalar : public VerilogNetPortRef
{
public:
  VerilogNetPortRefScalar(const std::string &name,
			  VerilogNet *net);
  virtual ~VerilogNetPortRefScalar();
  virtual bool isScalar() const { return true; }
  virtual int size(VerilogModule *module);
  virtual VerilogNetNameIterator *nameIterator(VerilogModule *module,
					       VerilogReader *reader);
  virtual bool hasNet() { return net_ != nullptr; }

private:
  VerilogNet *net_;
};

class VerilogNetPortRefBit : public VerilogNetPortRefScalar
{
public:
  VerilogNetPortRefBit(const std::string &name,
		       int index,
		       VerilogNet *net);
  const std::string &name() const override { return bit_name_; }

private:
  std::string bit_name_;
};

class VerilogNetPortRefPart : public VerilogNetPortRefBit
{
public:
  VerilogNetPortRefPart(const std::string &name,
			int from_index,
			int to_index,
			VerilogNet *net);
  const std::string &name() const override;
  int toIndex() const { return to_index_; }

private:
  int to_index_;
};

// Abstract class for iterating over the component nets of a net.
class VerilogNetNameIterator : public Iterator<const std::string&>
{
};

class VerilogAttrStmt
{
public:
  VerilogAttrStmt(VerilogAttrEntrySeq *attrs);
  VerilogAttrEntrySeq *attrs();
  virtual ~VerilogAttrStmt();

private:
  VerilogAttrEntrySeq *attrs_;
};

class VerilogAttrEntry
{
public:
  VerilogAttrEntry(const std::string &key,
                   const std::string &value);
  virtual std::string key();
  virtual std::string value();
  virtual ~VerilogAttrEntry() = default;

private:
  std::string key_;
  std::string value_;
};

} // namespace
