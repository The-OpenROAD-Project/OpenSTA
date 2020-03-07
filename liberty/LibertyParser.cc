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

#include <stdio.h>
#include <string.h>
#include "Machine.hh"
#include "Report.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "LibertyParser.hh"

// Global namespace

int
LibertyParse_parse();
extern FILE *LibertyLex_in;

namespace sta {

typedef Vector<LibertyGroup*> LibertyGroupSeq;

static const char *liberty_filename;
static int liberty_line;
// Previous lex reader state for include files.
static const char *liberty_filename_prev;
static int liberty_line_prev;
static FILE *liberty_stream_prev;

static LibertyGroupVisitor *liberty_group_visitor;
static LibertyGroupSeq liberty_group_stack;
static Report *liberty_report;

static LibertyStmt *
makeLibertyDefine(LibertyAttrValueSeq *values,
		  int line);
static LibertyAttrType
attrValueType(const char *value_type_name);
static LibertyGroupType
groupType(const char *group_type_name);

////////////////////////////////////////////////////////////////

void
parseLibertyFile(const char *filename,
		 LibertyGroupVisitor *library_visitor,
		 Report *report)
{
  LibertyLex_in = fopen(filename, "r");
  if (LibertyLex_in) {
    liberty_group_visitor = library_visitor;
    liberty_group_stack.clear();
    liberty_filename = filename;
    liberty_filename_prev = nullptr;
    liberty_stream_prev = nullptr;
    liberty_line = 1;
    liberty_report = report;
    LibertyParse_parse();
    fclose(LibertyLex_in);
  }
  else
    throw FileNotReadable(filename);
}

void
libertyGroupBegin(const char *type,
		  LibertyAttrValueSeq *params,
		  int line)
{
  LibertyGroup *group = new LibertyGroup(type, params, line);
  liberty_group_visitor->begin(group);
  liberty_group_stack.push_back(group);
}

LibertyGroup *
libertyGroupEnd()
{
  LibertyGroup *group = libertyGroup();
  liberty_group_visitor->end(group);
  liberty_group_stack.pop_back();
  LibertyGroup *parent =
    liberty_group_stack.empty() ? nullptr : liberty_group_stack.back();
  if (parent && liberty_group_visitor->save(group)) {
    parent->addSubgroup(group);
    return group;
  }
  else {
    delete group;
    return nullptr;
  }
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

LibertyStmt *
makeLibertySimpleAttr(const char *name,
		      LibertyAttrValue *value,
		      int line)
{
  LibertyAttr *attr = new LibertySimpleAttr(name, value, line);
  if (liberty_group_visitor)
    liberty_group_visitor->visitAttr(attr);
  LibertyGroup *group = libertyGroup();
  if (group && liberty_group_visitor->save(attr)) {
    group->addAttribute(attr);
    return attr;
  }
  else {
    delete attr;
    return nullptr;
  }
}

LibertyGroup *
libertyGroup()
{
  return liberty_group_stack.back();
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
  internalError("valueIterator called for LibertySimpleAttribute");
  return nullptr;
}

LibertyStmt *
makeLibertyComplexAttr(const char *name,
		       LibertyAttrValueSeq *values,
		       int line)
{
  // Defines have the same syntax as complex attributes.
  // Detect and convert them.
  if (stringEq(name, "define")) {
    LibertyStmt *define = makeLibertyDefine(values, line);
    stringDelete(name);
    LibertyAttrValueSeq::Iterator attr_iter(values);
    while (attr_iter.hasNext())
      delete attr_iter.next();
    delete values;
    return define;
  }
  else {
    LibertyAttr *attr = new LibertyComplexAttr(name, values, line);
    if (liberty_group_visitor)
      liberty_group_visitor->visitAttr(attr);
    if (liberty_group_visitor->save(attr)) {
      LibertyGroup *group = libertyGroup();
      group->addAttribute(attr);
      return attr;
    }
    else {
      delete attr;
      return nullptr;
    }
  }
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

LibertyAttrValue *
makeLibertyStringAttrValue(char *value)
{
  return new LibertyStringAttrValue(value);
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
  internalError("LibertyStringAttrValue called for float value");
  return 0.0;
}

const char *
LibertyStringAttrValue::stringValue()
{
  return value_;
}

LibertyAttrValue *
makeLibertyFloatAttrValue(float value)
{
  return new LibertyFloatAttrValue(value);
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
  internalError("LibertyStringAttrValue called for float value");
  return nullptr;
}

////////////////////////////////////////////////////////////////

static LibertyStmt *
makeLibertyDefine(LibertyAttrValueSeq *values,
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
    LibertyGroup *group = libertyGroup();
    group->addDefine(define);
  }
  else
    liberty_report->fileWarn(liberty_filename, line,
			     "define does not have three arguments.\n");
  return define;
}

// The Liberty User Guide Version 2001.08 fails to define the strings
// used to define valid attribute types.  Beyond "string" these are
// guesses.
static LibertyAttrType
attrValueType(const char *value_type_name)
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

static LibertyGroupType
groupType(const char *group_type_name)
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

LibertyStmt *
makeLibertyVariable(char *var,
		    float value,
		    int line)
{
  LibertyVariable *variable = new LibertyVariable(var, value, line);
  liberty_group_visitor->visitVariable(variable);
  if (liberty_group_visitor->save(variable))
    return variable;
  else {
    delete variable;
    return nullptr;
  }
}

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

bool
libertyInInclude()
{
  return liberty_filename_prev != nullptr;
}

FILE *
libertyIncludeBegin(const char *filename)
{
  FILE *stream = fopen(filename, "r" );
  if (stream == nullptr)
    libertyParseError("cannot open include file %s.\n", filename);
  else {
    liberty_filename_prev = liberty_filename;
    liberty_line_prev = liberty_line;
    liberty_stream_prev = LibertyLex_in;

    liberty_filename = filename;
    liberty_line = 1;
  }
  return stream;
}

void
libertyIncludeEnd()
{
  fclose(LibertyLex_in);
  liberty_filename = liberty_filename_prev;
  liberty_line = liberty_line_prev;
  LibertyLex_in = liberty_stream_prev;
  liberty_filename_prev = nullptr;
  liberty_stream_prev = nullptr;
}

void
libertyIncrLine()
{
  sta::liberty_line++;
}

int
libertyLine()
{
  return liberty_line;
}

void
libertyParseError(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  sta::liberty_report->vfileError(sta::liberty_filename, sta::liberty_line,
				  fmt, args);
  va_end(args);
}

} // namespace

////////////////////////////////////////////////////////////////
// Global namespace

void libertyParseFlushBuffer();

int
LibertyParse_error(const char *msg)
{
  sta::liberty_report->fileError(sta::liberty_filename, sta::liberty_line,
				 "%s.\n", msg);
  libertyParseFlushBuffer();
  return 0;
}
