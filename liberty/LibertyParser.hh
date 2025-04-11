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

#include "Zlib.hh"
#include "Vector.hh"
#include "Map.hh"
#include "Set.hh"
#include "StringUtil.hh"

namespace sta {

class Report;
class LibertyGroupVisitor;
class LibertyAttrVisitor;
class LibertyStmt;
class LibertyGroup;
class LibertyAttr;
class LibertyDefine;
class LibertyAttrValue;
class LibertyVariable;
class LibertySubgroupIterator;
class LibertyAttrIterator;
class LibertyScanner;

typedef Vector<LibertyStmt*> LibertyStmtSeq;
typedef Vector<LibertyGroup*> LibertyGroupSeq;
typedef Vector<LibertyAttr*> LibertyAttrSeq;
typedef Map<std::string, LibertyAttr*> LibertyAttrMap;
typedef Map<std::string, LibertyDefine*> LibertyDefineMap;
typedef Vector<LibertyAttrValue*> LibertyAttrValueSeq;
typedef Map<std::string, float> LibertyVariableMap;
typedef Map<std::string, LibertyGroupVisitor*>LibertyGroupVisitorMap;
typedef LibertyAttrValueSeq::Iterator LibertyAttrValueIterator;
typedef Vector<LibertyGroup*> LibertyGroupSeq;

enum class LibertyAttrType { attr_string, attr_int, attr_double,
			     attr_boolean, attr_unknown };

enum class LibertyGroupType { library, cell, pin, timing, unknown };

class LibertyParser
{
public:
  LibertyParser(const char *filename,
                LibertyGroupVisitor *library_visitor,
                Report *report);
  const std::string &filename() const { return filename_; }
  void setFilename(const std::string &filename);
  Report *report() const { return report_; }
  LibertyStmt *makeDefine(LibertyAttrValueSeq *values,
                          int line);
  LibertyAttrType attrValueType(const char *value_type_name);
  LibertyGroupType groupType(const char *group_type_name);
  void groupBegin(const char *type,
                  LibertyAttrValueSeq *params,
                  int line);
  LibertyGroup *groupEnd();
  LibertyGroup *group();
  void deleteGroups();
  LibertyStmt *makeSimpleAttr(const char *name,
                              LibertyAttrValue *value,
                              int line);
  LibertyStmt *makeComplexAttr(const char *name,
                               LibertyAttrValueSeq *values,
                               int line);
  LibertyAttrValue *makeStringAttrValue(char *value);
  LibertyAttrValue *makeFloatAttrValue(float value);
  LibertyStmt *makeVariable(const char *var,
                            float value,
                            int line);

private:
  std::string filename_;
  LibertyGroupVisitor *group_visitor_;
  Report *report_;
  LibertyGroupSeq group_stack_;
};

// Abstract base class for liberty statements.
class LibertyStmt
{
public:
  LibertyStmt(int line);
  virtual ~LibertyStmt() {}
  int line() const { return line_; }
  virtual bool isGroup() const { return false; }
  virtual bool isAttribute() const { return false; }
  virtual bool isDefine() const { return false; }
  virtual bool isVariable() const { return false; }

protected:
  int line_;
};

// Groups are a type keyword with a set of parameters and statements
// enclosed in brackets.
//  type([param1][, param2]...) { stmts.. }
class LibertyGroup : public LibertyStmt
{
public:
  LibertyGroup(const char *type,
	       LibertyAttrValueSeq *params,
	       int line);
  virtual ~LibertyGroup();
  virtual bool isGroup() const { return true; }
  const char *type() const { return type_.c_str(); }
  // First param as a string.
  const char *firstName();
  // Second param as a string.
  const char *secondName();
  LibertyAttr *findAttr(const char *name);
  void addSubgroup(LibertyGroup *subgroup);
  void addDefine(LibertyDefine *define);
  void addAttribute(LibertyAttr *attr);
  void addVariable(LibertyVariable *var);
  LibertyGroupSeq *subgroups() const { return subgroups_; }
  LibertyAttrSeq *attrs() const { return attrs_; }
  LibertyAttrValueSeq *params() const { return params_; }

protected:
  void parseNames(LibertyAttrValueSeq *values);

  std::string type_;
  LibertyAttrValueSeq *params_;
  LibertyAttrSeq *attrs_;
  LibertyAttrMap *attr_map_;
  LibertyGroupSeq *subgroups_;
  LibertyDefineMap *define_map_;
};

class LibertySubgroupIterator : public LibertyGroupSeq::Iterator
{
public:
  LibertySubgroupIterator(LibertyGroup *group);
};

class LibertyAttrIterator : public LibertyAttrSeq::Iterator
{
public:
  LibertyAttrIterator(LibertyGroup *group);
};

// Abstract base class for attributes.
class LibertyAttr : public LibertyStmt
{
public:
  LibertyAttr(const char *name,
	      int line);
  const char *name() const { return name_.c_str(); }
  virtual bool isAttribute() const { return true; }
  virtual bool isSimple() const = 0;
  virtual bool isComplex() const = 0;
  virtual LibertyAttrValueSeq *values() const = 0;
  virtual LibertyAttrValue *firstValue() = 0;

protected:
  std::string name_;
};

// Abstract base class for simple attributes.
//  name : value;
class LibertySimpleAttr : public LibertyAttr
{
public:
  LibertySimpleAttr(const char *name,
		    LibertyAttrValue *value,
		    int line);
  virtual ~LibertySimpleAttr();
  virtual bool isSimple() const { return true; }
  virtual bool isComplex() const { return false; }
  virtual LibertyAttrValue *firstValue() { return value_; }
  virtual LibertyAttrValueSeq *values() const;

private:
  LibertyAttrValue *value_;
};

// Complex attributes have multiple values.
//  name(attr_value1[, attr_value2]...);
class LibertyComplexAttr : public LibertyAttr
{
public:
  LibertyComplexAttr(const char *name,
		     LibertyAttrValueSeq *values,
		     int line);
  virtual ~LibertyComplexAttr();
  virtual bool isSimple() const { return false; }
  virtual bool isComplex() const { return true; }
  virtual LibertyAttrValue *firstValue();
  virtual LibertyAttrValueSeq *values() const { return values_; }

private:
  LibertyAttrValueSeq *values_;
};

// Attribute values are a string or float.
class LibertyAttrValue
{
public:
  LibertyAttrValue() {}
  virtual ~LibertyAttrValue() {}
  virtual bool isString() = 0;
  virtual bool isFloat() = 0;
  virtual float floatValue() = 0;
  virtual const char *stringValue() = 0;
};

class LibertyStringAttrValue : public LibertyAttrValue
{
public:
  LibertyStringAttrValue(const char *value);
  virtual ~LibertyStringAttrValue() {}
  virtual bool isFloat() { return false; }
  virtual bool isString() { return true; }
  virtual float floatValue();
  virtual const char *stringValue();

private:
  std::string value_;
};

class LibertyFloatAttrValue : public LibertyAttrValue
{
public:
  LibertyFloatAttrValue(float value);
  virtual ~LibertyFloatAttrValue() {}
  virtual bool isString() { return false; }
  virtual bool isFloat() { return true; }
  virtual float floatValue();
  virtual const char *stringValue();

private:
  float value_;
};

// Define statements define new simple attributes.
//  define(attribute_name, group_name, attribute_type);
//  attribute_type is string|integer|float.
class LibertyDefine : public LibertyStmt
{
public:
  LibertyDefine(const char *name,
		LibertyGroupType group_type,
		LibertyAttrType value_type,
		int line);
  virtual bool isDefine() const { return true; }
  const char *name() const { return name_.c_str(); }
  LibertyGroupType groupType() const { return group_type_; }
  LibertyAttrType valueType() const { return value_type_; }

private:
  std::string name_;
  LibertyGroupType group_type_;
  LibertyAttrType value_type_;
};

// The Liberty User Guide Version 2003.12 fails to document variables.
// var = value;
// The only example I have only uses float values, so I am assuming
// that is all that is supported (which is probably wrong).
class LibertyVariable : public LibertyStmt
{
public:
  LibertyVariable(const char *var,
		  float value,
		  int line);
  virtual bool isVariable() const { return true; }
  const char *variable() const { return var_.c_str(); }
  float value() const { return value_; }

private:
  std::string var_;
  float value_;
};

class LibertyGroupVisitor
{
public:
  LibertyGroupVisitor() {}
  virtual ~LibertyGroupVisitor() {}
  virtual void begin(LibertyGroup *group) = 0;
  virtual void end(LibertyGroup *group) = 0;
  virtual void visitAttr(LibertyAttr *attr) = 0;
  virtual void visitVariable(LibertyVariable *variable) = 0;
  // Predicates to save parse structure after visits.
  virtual bool save(LibertyGroup *group) = 0;
  virtual bool save(LibertyAttr *attr) = 0;
  virtual bool save(LibertyVariable *variable) = 0;
};

void
parseLibertyFile(const char *filename,
		 LibertyGroupVisitor *library_visitor,
		 Report *report);
} // namespace
