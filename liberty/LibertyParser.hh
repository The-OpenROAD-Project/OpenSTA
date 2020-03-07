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

typedef Vector<LibertyStmt*> LibertyStmtSeq;
typedef Vector<LibertyGroup*> LibertyGroupSeq;
typedef Vector<LibertyAttr*> LibertyAttrSeq;
typedef Map<const char *, LibertyAttr*, CharPtrLess> LibertyAttrMap;
typedef Map<const char *, LibertyDefine*, CharPtrLess> LibertyDefineMap;
typedef Vector<LibertyAttrValue*> LibertyAttrValueSeq;
typedef Map<const char *, float, CharPtrLess> LibertyVariableMap;
typedef Map<const char*,LibertyGroupVisitor*,CharPtrLess>LibertyGroupVisitorMap;
typedef LibertyAttrValueSeq::Iterator LibertyAttrValueIterator;

enum class LibertyAttrType { attr_string, attr_int, attr_double,
			     attr_boolean, attr_unknown };

enum class LibertyGroupType { library, cell, pin, timing, unknown };

// Abstract base class for liberty statements.
class LibertyStmt
{
public:
  explicit LibertyStmt(int line);
  virtual ~LibertyStmt() {}
  int line() const { return line_; }
  virtual bool isGroup() const { return false; }
  virtual bool isAttribute() const { return false; }
  virtual bool isDefine() const { return false; }
  virtual bool isVariable() const { return false; }

protected:
  int line_;

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyStmt);
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
  const char *type() const { return type_; }
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

  const char *type_;
  LibertyAttrValueSeq *params_;
  LibertyAttrSeq *attrs_;
  LibertyAttrMap *attr_map_;
  LibertyGroupSeq *subgroups_;
  LibertyDefineMap *define_map_;

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyGroup);
};

class LibertySubgroupIterator : public LibertyGroupSeq::Iterator
{
public:
  explicit LibertySubgroupIterator(LibertyGroup *group);

private:
  DISALLOW_COPY_AND_ASSIGN(LibertySubgroupIterator);
};

class LibertyAttrIterator : public LibertyAttrSeq::Iterator
{
public:
  explicit LibertyAttrIterator(LibertyGroup *group);

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyAttrIterator);
};

// Abstract base class for attributes.
class LibertyAttr : public LibertyStmt
{
public:
  LibertyAttr(const char *name,
	      int line);
  virtual ~LibertyAttr();
  const char *name() const { return name_; }
  virtual bool isAttribute() const { return true; }
  virtual bool isSimple() const = 0;
  virtual bool isComplex() const = 0;
  virtual LibertyAttrValueSeq *values() const = 0;
  virtual LibertyAttrValue *firstValue() = 0;

protected:
  const char *name_;

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyAttr);
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
  DISALLOW_COPY_AND_ASSIGN(LibertySimpleAttr);

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
  DISALLOW_COPY_AND_ASSIGN(LibertyComplexAttr);

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

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyAttrValue);
};

class LibertyStringAttrValue : public LibertyAttrValue
{
public:
  explicit LibertyStringAttrValue(const char *value);
  virtual ~LibertyStringAttrValue();
  virtual bool isFloat() { return false; }
  virtual bool isString() { return true; }
  virtual float floatValue();
  virtual const char *stringValue();

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyStringAttrValue);

  const char *value_;
};

class LibertyFloatAttrValue : public LibertyAttrValue
{
public:
  explicit LibertyFloatAttrValue(float value);
  virtual bool isString() { return false; }
  virtual bool isFloat() { return true; }
  virtual float floatValue();
  virtual const char *stringValue();

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyFloatAttrValue);

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
  virtual ~LibertyDefine();
  virtual bool isDefine() const { return true; }
  const char *name() const { return name_; }
  LibertyGroupType groupType() const { return group_type_; }
  LibertyAttrType valueType() const { return value_type_; }

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyDefine);

  const char *name_;
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
  // var_ is NOT deleted by ~LibertyVariable because the group
  // variable map ref's it.
  virtual ~LibertyVariable();
  virtual bool isVariable() const { return true; }
  const char *variable() const { return var_; }
  float value() const { return value_; }

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyVariable);

  const char *var_;
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

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyGroupVisitor);
};

FILE *
libertyIncludeBegin(const char *filename);
void
libertyIncludeEnd();
bool
libertyInInclude();
void
libertyIncrLine();
void
libertyParseError(const char *fmt,
		  ...);
int
libertyLine();

void
parseLibertyFile(const char *filename,
		 LibertyGroupVisitor *library_visitor,
		 Report *report);
void
libertyGroupBegin(const char *type,
		  LibertyAttrValueSeq *params,
		  int line);
LibertyGroup *
libertyGroupEnd();
LibertyGroup *
libertyGroup();
LibertyStmt *
makeLibertyComplexAttr(const char *name,
		       LibertyAttrValueSeq *values,
		       int line);
LibertyStmt *
makeLibertySimpleAttr(const char *name,
		      LibertyAttrValue *value,
		      int line);
LibertyAttrValue *
makeLibertyFloatAttrValue(float value);
LibertyAttrValue *
makeLibertyStringAttrValue(char *value);
LibertyStmt *
makeLibertyVariable(char *var,
		    float value,
		    int line);

} // namespace

// Global namespace.
int
LibertyParse_error(const char *msg);

