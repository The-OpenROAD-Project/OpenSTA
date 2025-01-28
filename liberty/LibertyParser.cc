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

#include "LibertyParser.hh"

#include <cstdio>
#include <cstring>
#include <regex>

#include "Zlib.hh"
#include "Report.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "LibertyScanner.hh"

namespace sta {

void
parseLibertyFile(const char *filename,
		 LibertyGroupVisitor *library_visitor,
		 Report *report)
{
  std::istream *stream = new gzstream::igzstream(filename);
  if (stream->good()) {
    LibertyParser reader(filename, library_visitor, report);
    LibertyScanner scanner(stream, filename, &reader, report);
    LibertyParse parser(&scanner, &reader);
    parser.parse();
    delete stream;
  }
  else {
    delete stream;
    throw FileNotReadable(filename);
  }
}

LibertyParser::LibertyParser(const char *filename,
                             LibertyGroupVisitor *library_visitor,
                             Report *report) :
  filename_(filename),
  group_visitor_(library_visitor),
  report_(report)
{
}

void
LibertyParser::setFilename(const string &filename)
{
  filename_ = filename;
}

LibertyStmt *
LibertyParser::makeDefine(LibertyAttrValueSeq *values,
                          int line)
{
  LibertyDefine *define = nullptr;
  if (values->size() == 3) {
    const char *define_name = (*values)[0]->stringValue();
    const char *group_type_name = (*values)[1]->stringValue();
    const char *value_type_name = (*values)[2]->stringValue();
    LibertyAttrType value_type = attrValueType(value_type_name);
    LibertyGroupType group_type = groupType(group_type_name);
    define = new LibertyDefine(stringCopy(define_name), group_type,
			       value_type, line);
    LibertyGroup *group = this->group();
    group->addDefine(define);
  }
  else
    report_->fileWarn(24, filename_.c_str(), line,
                      "define does not have three arguments.");
  return define;
}

// The Liberty User Guide Version 2001.08 fails to define the strings
// used to define valid attribute types.  Beyond "string" these are
// guesses.
LibertyAttrType
LibertyParser::attrValueType(const char *value_type_name)
{
  if (stringEq(value_type_name, "string"))
    return LibertyAttrType::attr_string;
  else if (stringEq(value_type_name, "integer"))
    return LibertyAttrType::attr_int;
  else if (stringEq(value_type_name, "float"))
    return LibertyAttrType::attr_double;
  else if (stringEq(value_type_name, "boolean"))
    return LibertyAttrType::attr_boolean;
  else
    return LibertyAttrType::attr_unknown;
}

LibertyGroupType
LibertyParser::groupType(const char *group_type_name)
{
  if (stringEq(group_type_name, "library"))
    return LibertyGroupType::library;
  else if (stringEq(group_type_name, "cell"))
    return LibertyGroupType::cell;
  else if (stringEq(group_type_name, "pin"))
    return LibertyGroupType::pin;
  else if (stringEq(group_type_name, "timing"))
    return LibertyGroupType::timing;
  else
    return LibertyGroupType::unknown;
}

void
LibertyParser::groupBegin(const char *type,
                          LibertyAttrValueSeq *params,
                          int line)
{
  LibertyGroup *group = new LibertyGroup(type, params, line);
  group_visitor_->begin(group);
  group_stack_.push_back(group);
}

LibertyGroup *
LibertyParser::groupEnd()
{
  LibertyGroup *group = this->group();
  group_visitor_->end(group);
  group_stack_.pop_back();
  LibertyGroup *parent =
    group_stack_.empty() ? nullptr : group_stack_.back();
  if (parent && group_visitor_->save(group)) {
    parent->addSubgroup(group);
    return group;
  }
  else {
    delete group;
    return nullptr;
  }
}

LibertyGroup *
LibertyParser::group()
{
  return group_stack_.back();
}

void
LibertyParser::deleteGroups()
{
  group_stack_.deleteContentsClear();
}

LibertyStmt *
LibertyParser::makeSimpleAttr(const char *name,
                              LibertyAttrValue *value,
                              int line)
{
  LibertyAttr *attr = new LibertySimpleAttr(name, value, line);
  group_visitor_->visitAttr(attr);
  LibertyGroup *group = this->group();
  if (group && group_visitor_->save(attr)) {
    group->addAttribute(attr);
    return attr;
  }
  else {
    delete attr;
    return nullptr;
  }
}

LibertyStmt *
LibertyParser::makeComplexAttr(const char *name,
                               LibertyAttrValueSeq *values,
                               int line)
{
  // Defines have the same syntax as complex attributes.
  // Detect and convert them.
  if (stringEq(name, "define")) {
    LibertyStmt *define = makeDefine(values, line);
    stringDelete(name);
    LibertyAttrValueSeq::Iterator attr_iter(values);
    while (attr_iter.hasNext())
      delete attr_iter.next();
    delete values;
    return define;
  }
  else {
    LibertyAttr *attr = new LibertyComplexAttr(name, values, line);
    group_visitor_->visitAttr(attr);
    if (group_visitor_->save(attr)) {
      LibertyGroup *group = this->group();
      group->addAttribute(attr);
      return attr;
    }
    delete attr;
    return nullptr;
  }
}

LibertyStmt *
LibertyParser::makeVariable(char *var,
                            float value,
                            int line)
{
  LibertyVariable *variable = new LibertyVariable(var, value, line);
  group_visitor_->visitVariable(variable);
  if (group_visitor_->save(variable))
    return variable;
  else {
    delete variable;
    return nullptr;
  }
}

LibertyAttrValue *
LibertyParser::makeStringAttrValue(char *value)
{
  return new LibertyStringAttrValue(value);
}

LibertyAttrValue *
LibertyParser::makeFloatAttrValue(float value)
{
  return new LibertyFloatAttrValue(value);
}

////////////////////////////////////////////////////////////////

LibertyStmt::LibertyStmt(int line) :
  line_(line)
{
}

LibertyGroup::LibertyGroup(const char *type,
			   LibertyAttrValueSeq *params,
			   int line) :
  LibertyStmt(line),
  type_(type),
  params_(params),
  attrs_(nullptr),
  attr_map_(nullptr),
  subgroups_(nullptr),
  define_map_(nullptr)
{
}

void
LibertyGroup::addSubgroup(LibertyGroup *subgroup)
{
  if (subgroups_ == nullptr)
    subgroups_ = new LibertyGroupSeq;
  subgroups_->push_back(subgroup);
}

void
LibertyGroup::addDefine(LibertyDefine *define)
{
  if (define_map_ == nullptr)
    define_map_ = new LibertyDefineMap;
  const char *define_name = define->name();
  LibertyDefine *prev_define = define_map_->findKey(define_name);
  if (prev_define) {
    define_map_->erase(define_name);
    delete prev_define;
  }
  (*define_map_)[define_name] = define;
}

void
LibertyGroup::addAttribute(LibertyAttr *attr)
{
  if (attrs_ == nullptr)
    attrs_ = new LibertyAttrSeq;
  attrs_->push_back(attr);
  if (attr_map_)
    (*attr_map_)[attr->name()] = attr;
}

LibertyGroup::~LibertyGroup()
{
  stringDelete(type_);
  if (params_) {
    params_->deleteContents();
    delete params_;
  }
  if (attrs_) {
    LibertyAttrSeq::Iterator iter(attrs_);
    attrs_->deleteContents();
    delete attrs_;
    delete attr_map_;
  }
  if (subgroups_) {
    subgroups_->deleteContents();
    delete subgroups_;
  }
  if (define_map_) {
    define_map_->deleteContents();
    delete define_map_;
  }
}

const char *
LibertyGroup::firstName()
{
  if (params_ && params_->size() > 0) {
    LibertyAttrValue *value = (*params_)[0];
    if (value->isString())
      return value->stringValue();
  }
  return nullptr;
}

const char *
LibertyGroup::secondName()
{
  if (params_ && params_->size() > 1) {
    LibertyAttrValue *value = (*params_)[1];
    if (value->isString())
      return value->stringValue();
  }
  return nullptr;
}

LibertyAttr *
LibertyGroup::findAttr(const char *name)
{
  if (attrs_) {
    if (attr_map_ == nullptr) {
      // Build attribute name map on demand.
      LibertyAttrSeq::Iterator attr_iter(attrs_);
      while (attr_iter.hasNext()) {
	LibertyAttr *attr = attr_iter.next();
	(*attr_map_)[attr->name()] = attr;
      }
    }
    return attr_map_->findKey(name);
  }
  else
    return nullptr;
}

LibertySubgroupIterator::LibertySubgroupIterator(LibertyGroup *group) :
  LibertyGroupSeq::Iterator(group->subgroups())
{
}

LibertyAttrIterator::LibertyAttrIterator(LibertyGroup *group) :
  LibertyAttrSeq::Iterator(group->attrs())
{
}

////////////////////////////////////////////////////////////////

LibertyAttr::LibertyAttr(const char *name,
			 int line) :
  LibertyStmt(line),
  name_(name)
{
}

LibertyAttr::~LibertyAttr()
{
  stringDelete(name_);
}

LibertySimpleAttr::LibertySimpleAttr(const char *name,
				     LibertyAttrValue *value,
				     int line) :
  LibertyAttr(name, line),
  value_(value)
{
}

LibertySimpleAttr::~LibertySimpleAttr()
{
  delete value_;
}

LibertyAttrValueSeq *
LibertySimpleAttr::values() const
{
  criticalError(1125, "valueIterator called for LibertySimpleAttribute");
  return nullptr;
}

LibertyComplexAttr::LibertyComplexAttr(const char *name,
				       LibertyAttrValueSeq *values,
				       int line) :
  LibertyAttr(name, line),
  values_(values)
{
}

LibertyComplexAttr::~LibertyComplexAttr()
{
  if (values_) {
    values_->deleteContents();
    delete values_;
  }
}

LibertyAttrValue *
LibertyComplexAttr::firstValue()
{
  if (values_ && values_->size() > 0)
    return (*values_)[0];
  else
    return nullptr;
}

LibertyStringAttrValue::LibertyStringAttrValue(const char *value) :
  LibertyAttrValue(),
  value_(value)
{
}

LibertyStringAttrValue::~LibertyStringAttrValue()
{
  stringDelete(value_);
}

float
LibertyStringAttrValue::floatValue()
{
  criticalError(1126, "LibertyStringAttrValue called for float value");
  return 0.0;
}

const char *
LibertyStringAttrValue::stringValue()
{
  return value_;
}

LibertyFloatAttrValue::LibertyFloatAttrValue(float value) :
  value_(value)
{
}

float
LibertyFloatAttrValue::floatValue()
{
  return value_;
}

const char *
LibertyFloatAttrValue::stringValue()
{
  criticalError(1127, "LibertyStringAttrValue called for float value");
  return nullptr;
}

////////////////////////////////////////////////////////////////

LibertyDefine::LibertyDefine(const char *name,
			     LibertyGroupType group_type,
			     LibertyAttrType value_type,
			     int line) :
  LibertyStmt(line),
  name_(name),
  group_type_(group_type),
  value_type_(value_type)
{
}

LibertyDefine::~LibertyDefine()
{
  stringDelete(name_);
}

////////////////////////////////////////////////////////////////

LibertyVariable::LibertyVariable(const char *var,
				 float value,
				 int line) :
  LibertyStmt(line),
  var_(var),
  value_(value)
{
}

LibertyVariable::~LibertyVariable()
{
  stringDelete(var_);
}

////////////////////////////////////////////////////////////////

LibertyScanner::LibertyScanner(std::istream *stream,
                               const char *filename,
                               LibertyParser *reader,
                               Report *report) :
  yyFlexLexer(stream),
  stream_(stream),
  filename_(filename),
  reader_(reader),
  report_(report),
  stream_prev_(nullptr)
{
}

bool
LibertyScanner::includeBegin()
{
  if (stream_prev_ != nullptr)
    error("nested include_file's are not supported");
  else {
    // include_file(filename);
    std::regex include_regexp("include_file *\\( *([^)]+) *\\) *;?");
    std::cmatch matches;
    if (std::regex_match(yytext, matches, include_regexp)) {
      string filename = matches[1].str();
      gzstream::igzstream *stream = new gzstream::igzstream(filename.c_str());
      if (stream->is_open()) {
        yypush_buffer_state(yy_create_buffer(stream, 256));

        filename_prev_ = filename_;
        stream_prev_ = stream_;

        filename_ = filename;
        reader_->setFilename(filename);
        stream_ = stream;
        return true;
      }
      else {
        report_->fileWarn(25, filename_.c_str(), yylineno,
                          "cannot open include file %s.", filename.c_str());
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
  report_->fileError(1866, filename_.c_str(), lineno(), "%s", msg);
}

void
LibertyParse::error(const location_type &loc,
                    const string &msg)
{
  reader->report()->fileError(164, reader->filename().c_str(),
                              loc.begin.line, "%s", msg.c_str());
}

} // namespace
