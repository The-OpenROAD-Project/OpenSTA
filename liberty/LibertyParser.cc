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

#include "LibertyParser.hh"

#include <cstring>
#include <istream>
#include <regex>
#include <string>
#include <string_view>
#include <utility>

#include "ContainerHelpers.hh"
#include "Error.hh"
#include "LibertyParse.hh"
#include "LibertyScanner.hh"
#include "Report.hh"
#include "StringUtil.hh"
#include "util/gzstream.hh"

namespace sta {

void
parseLibertyFile(std::string_view filename,
                 LibertyGroupVisitor *library_visitor,
                 Report *report)
{
  std::string fn(filename);
  gzstream::igzstream stream(fn.c_str());
  if (stream.is_open()) {
    LibertyParser reader(filename, library_visitor, report);
    LibertyScanner scanner(&stream, filename, &reader, report);
    LibertyParse parser(&scanner, &reader);
    parser.parse();
  }
  else
    throw FileNotReadable(filename);
}

LibertyParser::LibertyParser(std::string_view filename,
                             LibertyGroupVisitor *library_visitor,
                             Report *report) :
  filename_(filename),
  group_visitor_(library_visitor),
  report_(report)
{
}

void
LibertyParser::setFilename(std::string_view filename)
{
  filename_ = filename;
}

LibertyDefine *
LibertyParser::makeDefine(const LibertyAttrValueSeq *values,
                          int line)
{
  LibertyDefine *define = nullptr;
  if (values->size() == 3) {
    std::string &define_name = (*values)[0]->stringValue();
    const std::string &group_type_name = (*values)[1]->stringValue();
    const std::string &value_type_name = (*values)[2]->stringValue();
    LibertyAttrType value_type = attrValueType(value_type_name);
    LibertyGroupType group_type = groupType(group_type_name);
    define = new LibertyDefine(std::move(define_name), group_type, value_type, line);
    LibertyGroup *group = this->group();
    group->addDefine(define);
    for (auto value : *values)
      delete value;
    delete values;
  }
  else
    report_->fileWarn(24, filename_, line,
                      "define does not have three arguments.");
  return define;
}

// The Liberty User Guide Version 2001.08 fails to define the strings
// used to define valid attribute types.  Beyond "string" these are
// guesses.
LibertyAttrType
LibertyParser::attrValueType(const std::string &value_type_name)
{
  if (value_type_name == "string")
    return LibertyAttrType::attr_string;
  else if (value_type_name == "integer")
    return LibertyAttrType::attr_int;
  else if (value_type_name == "float")
    return LibertyAttrType::attr_double;
  else if (value_type_name == "boolean")
    return LibertyAttrType::attr_boolean;
  else
    return LibertyAttrType::attr_unknown;
}

LibertyGroupType
LibertyParser::groupType(const std::string &group_type_name)
{
  if (group_type_name == "library")
    return LibertyGroupType::library;
  else if (group_type_name == "cell")
    return LibertyGroupType::cell;
  else if (group_type_name == "pin")
    return LibertyGroupType::pin;
  else if (group_type_name == "timing")
    return LibertyGroupType::timing;
  else
    return LibertyGroupType::unknown;
}

void
LibertyParser::groupBegin(std::string &&type,
                          LibertyAttrValueSeq *params,
                          int line)
{
  LibertyGroup *group = new LibertyGroup(std::move(type),
                                         params
                                         ? std::move(*params)
                                         : LibertyAttrValueSeq(),
                                         line);
  delete params;
  LibertyGroup *parent_group = group_stack_.empty() ? nullptr : group_stack_.back();
  group_visitor_->begin(group, parent_group);
  group_stack_.push_back(group);
}

LibertyGroup *
LibertyParser::groupEnd()
{
  LibertyGroup *group = this->group();
  group_stack_.pop_back();
  LibertyGroup *parent = group_stack_.empty() ? nullptr : group_stack_.back();
  if (parent)
    parent->addSubgroup(group);
  group_visitor_->end(group, parent);
  return group;
}

LibertyGroup *
LibertyParser::group()
{
  return group_stack_.back();
}

void
LibertyParser::deleteGroups()
{
  deleteContents(group_stack_);
}

LibertySimpleAttr *
LibertyParser::makeSimpleAttr(std::string &&name,
                              const LibertyAttrValue *value,
                              int line)
{
  LibertySimpleAttr *attr = new LibertySimpleAttr(std::move(name), *value, line);
  delete value;
  LibertyGroup *group = this->group();
  group->addAttr(attr);
  group_visitor_->visitAttr(attr);
  return attr;
}

LibertyComplexAttr *
LibertyParser::makeComplexAttr(std::string &&name,
                               const LibertyAttrValueSeq *values,
                               int line)
{
  // Defines have the same syntax as complex attributes.
  // Detect and convert them.
  if (name == "define") {
    makeDefine(values, line);
    return nullptr;  // Define is not a complex attr; already added to group
  }
  else {
    LibertyComplexAttr *attr = new LibertyComplexAttr(std::move(name), *values, line);
    delete values;
    LibertyGroup *group = this->group();
    group->addAttr(attr);
    group_visitor_->visitAttr(attr);
    return attr;
  }
}

LibertyVariable *
LibertyParser::makeVariable(std::string &&var,
                            float value,
                            int line)
{
  LibertyVariable *variable = new LibertyVariable(std::move(var), value, line);
  LibertyGroup *group = this->group();
  group->addVariable(variable);
  group_visitor_->visitVariable(variable);
  return variable;
}

LibertyAttrValue *
LibertyParser::makeAttrValueString(std::string &&value)
{
  return new LibertyAttrValue(std::move(value));
}

LibertyAttrValue *
LibertyParser::makeAttrValueFloat(float value)
{
  return new LibertyAttrValue(value);
}

////////////////////////////////////////////////////////////////

LibertyScanner::LibertyScanner(std::istream *stream,
                               std::string_view filename,
                               LibertyParser *reader,
                               Report *report) :
  yyFlexLexer(stream),
  stream_(stream),
  filename_(filename),
  reader_(reader),
  report_(report)
{
}

bool
LibertyScanner::includeBegin()
{
  if (stream_prev_ != nullptr)
    error("nested include_file's are not supported");
  else {
    // include_file(filename);
    static const std::regex include_regexp("include_file *\\( *([^)]+) *\\) *;?");
    std::cmatch matches;
    if (std::regex_match(yytext, matches, include_regexp)) {
      std::string filename = matches[1].str();
      gzstream::igzstream *stream = new gzstream::igzstream(filename.c_str());
      if (stream->is_open()) {
        yypush_buffer_state(yy_create_buffer(stream, 16384));

        filename_prev_ = filename_;
        stream_prev_ = stream_;

        filename_ = filename;
        reader_->setFilename(filename);
        stream_ = stream;
        return true;
      }
      else {
        report_->fileWarn(25, filename_, yylineno,
                          "cannot open include file {}.", filename);
        delete stream;
      }
    }
    else
      error("include_file syntax error.");
  }
  return false;
}

void
LibertyScanner::fileEnd()
{
  if (stream_prev_)
    delete stream_;
  stream_ = stream_prev_;
  filename_ = filename_prev_;
  stream_prev_ = nullptr;

  yypop_buffer_state();
}

void
LibertyScanner::error(const char *msg)
{
  report_->fileError(1866, filename_, lineno(), "{}", msg);
}

////////////////////////////////////////////////////////////////

LibertyGroup::LibertyGroup(std::string type,
                           LibertyAttrValueSeq params,
                           int line) :
  type_(std::move(type)),
  params_(std::move(params)),
  line_(line)
{
}

LibertyGroup::~LibertyGroup() { clear(); }

void
LibertyGroup::clear()
{
  deleteContents(params_);
  deleteContents(simple_attr_map_);
  for (auto &attr : complex_attr_map_)
    deleteContents(attr.second);
  complex_attr_map_.clear();
  deleteContents(subgroups_);
  subgroup_map_.clear();
  deleteContents(define_map_);
  deleteContents(variables_);
}

bool
LibertyGroup::empty() const
{
  return subgroups_.empty() && simple_attr_map_.empty() && complex_attr_map_.empty()
      && define_map_.empty();
}

bool
LibertyGroup::oneGroupOnly() const
{
  return subgroups_.size() == 1 && simple_attr_map_.empty()
      && complex_attr_map_.empty() && define_map_.empty();
}

void
LibertyGroup::addSubgroup(LibertyGroup *subgroup)
{
  subgroups_.push_back(subgroup);
  subgroup_map_[subgroup->type()].push_back(subgroup);
}

void
LibertyGroup::deleteSubgroup(const LibertyGroup *subgroup)
{
  if (subgroup == subgroups_.back()) {
    subgroups_.pop_back();
    subgroup_map_[subgroup->type()].pop_back();
    delete subgroup;
  }
  else
    criticalError(1128, "LibertyAttrValue::floatValue() called on string");
}

void
LibertyGroup::addDefine(LibertyDefine *define)
{
  const std::string &define_name = define->name();
  LibertyDefine *prev_define = findKey(define_map_, define_name);
  if (prev_define) {
    define_map_.erase(define_name);
    delete prev_define;
  }
  define_map_[define_name] = define;
}

void
LibertyGroup::addAttr(LibertySimpleAttr *attr)
{
  // Only keep the most recent simple attribute value.
  const auto &it = simple_attr_map_.find(attr->name());
  if (it != simple_attr_map_.end())
    delete it->second;
  simple_attr_map_[attr->name()] = attr;
}

void
LibertyGroup::addAttr(LibertyComplexAttr *attr)
{
  complex_attr_map_[attr->name()].push_back(attr);
}

void
LibertyGroup::addVariable(LibertyVariable *var)
{
  variables_.push_back(var);
}

bool
LibertyGroup::hasFirstParam() const
{
  return !params_.empty();
}

const std::string &
LibertyGroup::firstParam() const
{
  LibertyAttrValue *value = params_[0];
  return value->stringValue();
}

bool
LibertyGroup::hasSecondParam() const
{
  return params_.size() >= 2;
}

const std::string &
LibertyGroup::secondParam() const
{
  LibertyAttrValue *value = params_[1];
  return value->stringValue();
}

const LibertyGroupSeq &
LibertyGroup::findSubgroups(std::string_view type) const
{
  auto it = subgroup_map_.find(type);
  if (it != subgroup_map_.end())
    return it->second;
  static const LibertyGroupSeq empty;
  return empty;
}

const LibertyGroup *
LibertyGroup::findSubgroup(std::string_view type) const
{
  const LibertyGroupSeq &groups = findSubgroups(type);
  if (!groups.empty())
    return groups[0];
  else
    return nullptr;
}

const LibertySimpleAttr *
LibertyGroup::findSimpleAttr(std::string_view attr_name) const
{
  return findStringKey(simple_attr_map_, attr_name);
}

const LibertyComplexAttrSeq &
LibertyGroup::findComplexAttrs(std::string_view attr_name) const
{
  auto it = complex_attr_map_.find(attr_name);
  if (it != complex_attr_map_.end())
    return it->second;
  static const LibertyComplexAttrSeq empty;
  return empty;
}

const LibertyComplexAttr *
LibertyGroup::findComplexAttr(std::string_view attr_name) const
{
  const LibertyComplexAttrSeq &attrs = findComplexAttrs(attr_name);
  if (!attrs.empty())
    return attrs[0];
  else
    return nullptr;
}

const std::string &
LibertyGroup::findAttrString(std::string_view attr_name) const
{
  const LibertySimpleAttr *attr = findSimpleAttr(attr_name);
  if (attr)
    return attr->value().stringValue();
  static const std::string null_string;
  return null_string;
}

void
LibertyGroup::findAttrFloat(std::string_view attr_name,
                            // Return values.
                            float &value,
                            bool &exists) const
{
  const LibertySimpleAttr *attr = findSimpleAttr(attr_name);
  if (attr) {
    const LibertyAttrValue &attr_value = attr->value();
    if (attr_value.isFloat()) {
      auto [value1, exists1] = attr_value.floatValue();
      value = value1;
      exists = exists1;
      return;
    }
    else {
      // Possibly quoted string float.
      const std::string &float_str = attr_value.stringValue();
      auto [value1, valid1] = stringFloat(float_str);
      value = value1;
      exists = valid1;
      return;
    }
  }
  exists = false;
}

void
LibertyGroup::findAttrInt(std::string_view attr_name,
                          // Return values.
                          int &value,
                          bool &exists) const
{
  const LibertySimpleAttr *attr = findSimpleAttr(attr_name);
  if (attr) {
    const LibertyAttrValue &attr_value = attr->value();
    if (attr_value.isFloat()) {
      auto [value1, exists1] = attr_value.floatValue();
      value = static_cast<int>(value1);
      exists = exists1;
      return;
    }
  }
  exists = false;
}

////////////////////////////////////////////////////////////////

LibertySimpleAttr::LibertySimpleAttr(std::string &&name,
                                     LibertyAttrValue value,
                                     int line) :
  name_(std::move(name)),
  line_(line),
  value_(std::move(value))
{
}

////////////////////////////////////////////////////////////////

LibertyComplexAttr::LibertyComplexAttr(std::string &&name,
                                       LibertyAttrValueSeq values,
                                       int line) :
  name_(std::move(name)),
  values_(std::move(values)),
  line_(line)
{
}

LibertyComplexAttr::~LibertyComplexAttr() { deleteContents(values_); }

const LibertyAttrValue *
LibertyComplexAttr::firstValue() const
{
  if (!values_.empty())
    return values_[0];
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

LibertyAttrValue::LibertyAttrValue(std::string &&value) :
  string_value_(std::move(value))
{
}

LibertyAttrValue::LibertyAttrValue(float value) :
  float_value_(value)
{
}

bool
LibertyAttrValue::isFloat() const
{
  return string_value_.empty();
}

bool
LibertyAttrValue::isString() const
{
  return !string_value_.empty();
}

std::pair<float, bool>
LibertyAttrValue::floatValue() const
{
  if (string_value_.empty())
    return {float_value_, true};
  else
    return stringFloat(string_value_);
}

////////////////////////////////////////////////////////////////

LibertyDefine::LibertyDefine(std::string &&name,
                             LibertyGroupType group_type,
                             LibertyAttrType value_type,
                             int line) :
  name_(std::move(name)),
  group_type_(group_type),
  value_type_(value_type),
  line_(line)
{
}

////////////////////////////////////////////////////////////////

LibertyVariable::LibertyVariable(std::string var,
                                 float value,
                                 int line) :
  var_(std::move(var)),
  value_(value),
  line_(line)
{
}

}  // namespace sta
