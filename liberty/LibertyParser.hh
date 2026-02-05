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

#include <vector>
#include <map>

#include "Zlib.hh"
#include "StringUtil.hh"

namespace sta {

class Report;
class LibertyGroupVisitor;
class LibertyStmt;
class LibertyGroup;
class LibertyAttr;
class LibertyDefine;
class LibertyAttrValue;
class LibertyVariable;
class LibertyScanner;

using LibertyStmtSeq = std::vector<LibertyStmt*>;
using LibertyGroupSeq = std::vector<LibertyGroup*>;
using LibertyAttrSeq = std::vector<LibertyAttr*>;
using LibertyAttrMap = std::map<std::string, LibertyAttr*>;
using LibertyDefineMap = std::map<std::string, LibertyDefine*>;
using LibertyAttrValueSeq = std::vector<LibertyAttrValue*>;
using LibertyVariableMap = std::map<std::string, float>;
using LibertyGroupVisitorMap = std::map<std::string, LibertyGroupVisitor*>;

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
  void groupBegin(std::string type,
                  LibertyAttrValueSeq *params,
                  int line);
  LibertyGroup *groupEnd();
  LibertyGroup *group();
  void deleteGroups();
  LibertyStmt *makeSimpleAttr(std::string name,
                              LibertyAttrValue *value,
                              int line);
  LibertyStmt *makeComplexAttr(std::string name,
                               LibertyAttrValueSeq *values,
                               int line);
  LibertyAttrValue *makeStringAttrValue(std::string value);
  LibertyAttrValue *makeFloatAttrValue(float value);
  LibertyStmt *makeVariable(std::string var,
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
  virtual bool isSimpleAttr() const { return false; }
  virtual bool isComplexAttr() const { return false; }
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
  LibertyGroup(std::string type,
               LibertyAttrValueSeq *params,
               int line);
  virtual ~LibertyGroup();
  virtual bool isGroup() const { return true; }
  const std::string &type() const { return type_; }
  LibertyAttrValueSeq *params() const { return params_; }
  // First param as a string.
  const char *firstName();
  // Second param as a string.
  const char *secondName();
  void addStmt(LibertyStmt *stmt);
  LibertyStmtSeq *stmts() const { return stmts_; }

protected:
  void parseNames(LibertyAttrValueSeq *values);

  std::string type_;
  LibertyAttrValueSeq *params_;
  LibertyStmtSeq *stmts_;
};

// Abstract base class for attributes.
class LibertyAttr : public LibertyStmt
{
public:
  LibertyAttr(std::string name,
              int line);
  const std::string &name() const { return name_; }
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
  LibertySimpleAttr(std::string name,
                    LibertyAttrValue *value,
                    int line);
  virtual ~LibertySimpleAttr();
  bool isSimpleAttr() const override { return true; };
  LibertyAttrValue *firstValue() override { return value_; };
  LibertyAttrValueSeq *values() const override;

private:
  LibertyAttrValue *value_;
};

// Complex attributes have multiple values.
//  name(attr_value1[, attr_value2]...);
class LibertyComplexAttr : public LibertyAttr
{
public:
  LibertyComplexAttr(std::string name,
                     LibertyAttrValueSeq *values,
                     int line);
  virtual ~LibertyComplexAttr();
  bool isComplexAttr() const override { return true; };
  LibertyAttrValue *firstValue() override ;
  LibertyAttrValueSeq *values() const override { return values_; }

private:
  LibertyAttrValueSeq *values_;
};

// Attribute values are a string or float.
class LibertyAttrValue
{
public:
  LibertyAttrValue() {}
  virtual ~LibertyAttrValue() {}
  virtual bool isString() const = 0;
  virtual bool isFloat() const  = 0;
  virtual float floatValue() const = 0;
  virtual const std::string &stringValue() const = 0;
};

class LibertyStringAttrValue : public LibertyAttrValue
{
public:
  LibertyStringAttrValue(std::string value);
  virtual ~LibertyStringAttrValue() {}
  bool isFloat() const override { return false; }
  bool isString() const override { return true; }
  float floatValue() const override ;
  const std::string &stringValue() const  override { return value_; }

private:
  std::string value_;
};

class LibertyFloatAttrValue : public LibertyAttrValue
{
public:
  LibertyFloatAttrValue(float value);
  virtual ~LibertyFloatAttrValue() {}
  bool isString() const override { return false; }
  bool isFloat() const override { return true; }
  float floatValue() const override { return value_; }
  const std::string &stringValue() const override;

private:
  float value_;
};

// Define statements define new simple attributes.
//  define(attribute_name, group_name, attribute_type);
//  attribute_type is string|integer|float.
class LibertyDefine : public LibertyStmt
{
public:
  LibertyDefine(std::string name,
                LibertyGroupType group_type,
                LibertyAttrType value_type,
                int line);
  virtual bool isDefine() const { return true; }
  const std::string &name() const { return name_; }
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
  LibertyVariable(std::string var,
                  float value,
                  int line);
  bool isVariable() const override { return true; }
  const std::string &variable() const { return var_; }
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
