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

#include <functional>
#include <string_view>
#include <vector>
#include <map>
#include <utility>

#include "Zlib.hh"
#include "StringUtil.hh"

namespace sta {

class Report;
class LibertyGroupVisitor;
class LibertyGroup;
class LibertyDefine;
class LibertySimpleAttr;
class LibertyComplexAttr;
class LibertyAttrValue;
class LibertyVariable;
class LibertyScanner;

using LibertyGroupSeq = std::vector<LibertyGroup*>;
using LibertySubGroupMap = std::map<std::string, LibertyGroupSeq, std::less<>>;
using LibertySimpleAttrMap = std::map<std::string, LibertySimpleAttr*, std::less<>>;
using LibertyComplexAttrSeq = std::vector<LibertyComplexAttr*>;
using LibertyComplexAttrMap = std::map<std::string, LibertyComplexAttrSeq, std::less<>>;
using LibertyDefineMap = std::map<std::string, LibertyDefine*, std::less<>>;
using LibertyAttrValueSeq = std::vector<LibertyAttrValue*>;
using LibertyVariableSeq = std::vector<LibertyVariable*>;
using LibertyVariableMap = std::map<std::string, float, std::less<>>;
using LibertyGroupVisitorMap = std::map<std::string, LibertyGroupVisitor*, std::less<>>;

enum class LibertyAttrType { attr_string, attr_int, attr_double,
                             attr_boolean, attr_unknown };

enum class LibertyGroupType { library, cell, pin, timing, unknown };

class LibertyParser
{
public:
  LibertyParser(std::string_view filename,
                LibertyGroupVisitor *library_visitor,
                Report *report);
  const std::string &filename() const { return filename_; }
  void setFilename(std::string_view filename);
  Report *report() const { return report_; }
  LibertyDefine *makeDefine(const LibertyAttrValueSeq *values,
                           int line);
  LibertyAttrType attrValueType(const std::string &value_type_name);
  LibertyGroupType groupType(const std::string &group_type_name);
  void groupBegin(std::string &&type,
                  LibertyAttrValueSeq *params,
                  int line);
  LibertyGroup *groupEnd();
  LibertyGroup *group();
  void deleteGroups();
  LibertySimpleAttr *makeSimpleAttr(std::string &&name,
                                    const LibertyAttrValue *value,
                                    int line);
  LibertyComplexAttr *makeComplexAttr(std::string &&name,
                                     const LibertyAttrValueSeq *values,
                                     int line);
  LibertyAttrValue *makeAttrValueString(std::string &&value);
  LibertyAttrValue *makeAttrValueFloat(float value);
  LibertyVariable *makeVariable(std::string &&var,
                                float value,
                                int line);

private:
  std::string filename_;
  LibertyGroupVisitor *group_visitor_;
  Report *report_;
  LibertyGroupSeq group_stack_;
};

// Attribute values are a string or float.
class LibertyAttrValue
{
public:
  LibertyAttrValue() {}
  LibertyAttrValue(float value);
  LibertyAttrValue(std::string &&value);
  bool isString() const;
  bool isFloat() const;
  std::pair<float, bool> floatValue() const;
  const std::string &stringValue() const { return string_value_; }
  std::string &stringValue() { return string_value_; }

private:
  float float_value_;
  std::string string_value_;
};

// Groups are a type keyword with a set of parameters and statements
// enclosed in brackets.
//  type([param1][, param2]...) { stmts.. }
class LibertyGroup
{
public:
  LibertyGroup(const std::string type,
               const LibertyAttrValueSeq params,
               int line);
  ~LibertyGroup();
  void clear();
  bool empty() const;
  bool oneGroupOnly() const;
  const std::string &type() const { return type_; }
  const LibertyAttrValueSeq &params() const { return params_; }
  bool hasFirstParam() const;
  const std::string &firstParam() const;
  bool hasSecondParam() const;
  const std::string &secondParam() const;
  int line() const { return line_; }

  const LibertyGroupSeq &findSubgroups(std::string_view type) const;
  const LibertyGroup *findSubgroup(std::string_view type) const;
  const LibertySimpleAttr *findSimpleAttr(std::string_view attr_name) const;
  const LibertyComplexAttrSeq &findComplexAttrs(std::string_view attr_name) const;
  const LibertyComplexAttr *findComplexAttr(std::string_view attr_name) const;
  const std::string &findAttrString(std::string_view attr_name) const;
  void findAttrFloat(std::string_view attr_name,
                     // Return values.
                     float &value,
                     bool &exists) const;
  void findAttrInt(std::string_view attr_name,
                   // Return values.
                   int &value,
                   bool &exists) const;

  const LibertyGroupSeq &subgroups() const { return subgroups_; }
  const LibertyDefineMap &defineMap() const { return define_map_; }

  void addSubgroup(LibertyGroup *subgroup);
  void deleteSubgroup(const LibertyGroup *subgroup);
  void addAttr(LibertySimpleAttr *attr);
  void addAttr(LibertyComplexAttr *attr);
  void addDefine(LibertyDefine *define);
  void addVariable(LibertyVariable *var);

protected:
  std::string type_;
  LibertyAttrValueSeq params_;
  int line_;

  LibertySimpleAttrMap simple_attr_map_;
  LibertyComplexAttrMap complex_attr_map_;
  LibertyGroupSeq subgroups_;
  LibertySubGroupMap subgroup_map_;
  LibertyDefineMap define_map_;
  LibertyVariableSeq variables_;
};

class LibertyGroupLineLess
{
public:
  bool
  operator()(const LibertyGroup *group1,
             const LibertyGroup *group2) const {
    return group1->line() < group2->line();
  }
};

// Simple attributes: name : value;
class LibertySimpleAttr
{
public:
  LibertySimpleAttr(std::string &&name,
                    const LibertyAttrValue value,
                    int line);
  const std::string &name() const { return name_; }
  const LibertyAttrValue &value() const { return value_; };
  const std::string &stringValue() const { return value_.stringValue(); }
  int line() const { return line_; }

private:
  std::string name_;
  int line_;
  LibertyAttrValue value_;
};

// Complex attributes have multiple values.
//  name(attr_value1[, attr_value2]...);
class LibertyComplexAttr
{
public:
  LibertyComplexAttr(std::string &&name,
                     const LibertyAttrValueSeq values,
                     int line);
  ~LibertyComplexAttr();
  const std::string &name() const { return name_; }
  const LibertyAttrValue *firstValue() const;
  const LibertyAttrValueSeq &values() const { return values_; }
  int line() const { return line_; }

private:
  std::string name_;
  LibertyAttrValueSeq values_;
  int line_;
};

// Define statements define new simple attributes.
//  define(attribute_name, group_name, attribute_type);
//  attribute_type is string|integer|float.
class LibertyDefine
{
public:
  LibertyDefine(std::string &&name,
                LibertyGroupType group_type,
                LibertyAttrType value_type,
                int line);
  const std::string &name() const { return name_; }
  LibertyGroupType groupType() const { return group_type_; }
  LibertyAttrType valueType() const { return value_type_; }
  int line() const { return line_; }

private:
  std::string name_;
  LibertyGroupType group_type_;
  LibertyAttrType value_type_;
  int line_;
};

// The Liberty User Guide Version 2003.12 fails to document variables.
// var = value;
// The only example I have only uses float values, so I am assuming
// that is all that is supported (which is probably wrong).
class LibertyVariable
{
public:
  LibertyVariable(std::string var,
                  float value,
                  int line);
  int line() const { return line_; }
  const std::string &variable() const { return var_; }
  float value() const { return value_; }

private:
  std::string var_;
  float value_;
  int line_;
};

class LibertyGroupVisitor
{
public:
  LibertyGroupVisitor() {}
  virtual ~LibertyGroupVisitor() {}
  virtual void begin(const LibertyGroup *group,
                     LibertyGroup *parent_group) = 0;
  virtual void end(const LibertyGroup *group,
                   LibertyGroup *parent_group) = 0;
  virtual void visitAttr(const LibertySimpleAttr *attr) = 0;
  virtual void visitAttr(const LibertyComplexAttr *attr) = 0;
  virtual void visitVariable(LibertyVariable *variable) = 0;
};

void
parseLibertyFile(std::string_view filename,
                 LibertyGroupVisitor *library_visitor,
                 Report *report);
} // namespace
