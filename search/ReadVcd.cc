// OpenSTA, Static Timing Analyzer
// Copyright (c) 2022, Parallax Software, Inc.
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

#include <string>
#include <cctype>
#include <vector>
#include <map>
#include <algorithm>

#include "ReadVcd.hh"

#include "Zlib.hh"
#include "Report.hh"
#include "StaState.hh"

namespace sta {

using std::string;
using std::isspace;
using std::vector;
using std::map;
using std::max;
using std::min;

class VcdVar;
class VcdValue;
typedef vector<VcdValue> VcdValues;
typedef int64_t VarTime;

class VcdReader : public StaState
{
public:
  VcdReader(StaState *sta);
  void read(const char *filename);
  void reportWaveforms();

private:
  void parseTimescale();
  void parseVar();
  void parseScope();
  void parseUpscope();
  void parseDumpvars();
  void parseVarValues();
  void makeVarIdMap();
  void appendVarValue(string id,
                      char value);
  size_t varMaxValueCount();
  VarTime varMinTimeDelta();
  string getToken();
  string readStmtString();
  vector<string> readStmtTokens();


  gzFile stream_;
  string token_;
  const char *filename_;
  int file_line_;
  int stmt_line_;

  string date_;
  string comment_;
  string version_;
  double time_scale_;
  string time_unit_;
  double time_unit_scale_;
  vector<VcdVar> vars_;
  size_t max_var_name_length_;
  int max_var_width_;
  map<string, VcdVar*> id_var_map_;
  VarTime time_;
  VarTime time_max_;
};

class VcdVar
{
public:
  enum VarType {wire, reg};
  VcdVar(string name,
         VarType type,
         int width,
         string id);
  const string& name() const { return name_; }
  VarType type() const { return type_; }
  int width() const { return width_; }
  const string& id() const { return id_; }
  void pushValue(VarTime time,
                 char value);
  void pushBusValue(VarTime time,
                    VarTime bus_value);
  VcdValues values() { return values_; }

private:
  string name_;
  VarType type_;
  int width_;
  string id_;
  VcdValues values_;
};

class VcdValue
{
public:
  VcdValue(VarTime time,
           char value,
           uint64_t bus_value);
  VarTime time() const { return time_; }
  char value() const { return value_; }
  uint64_t busValue() const { return bus_value_; }

private:
  VarTime time_;
  // 01XUZ or '\0' when width > 1 to use bus_value_.
  char value_;
  uint64_t bus_value_;
};

void
readVcdFile(const char *filename,
            StaState *sta)

{
  VcdReader reader(sta);
  reader.read(filename);
  reader.reportWaveforms();
}

void
VcdReader::read(const char *filename)
{
  stream_ = gzopen(filename, "r");
  if (stream_) {
    filename_ = filename;
    file_line_ = 1;
    stmt_line_ = 1;
    string token = getToken();
    while (!token.empty()) {
      if (token == "$date")
        date_ = readStmtString();
      if (token == "$comment")
        comment_ = readStmtString();
      else if (token == "$version")
        version_ = readStmtString();
      else if (token == "$timescale")
        parseTimescale();
      if (token == "$var")
        parseVar();
      else if (token == "$scope")
        parseScope();
      else if (token == "$upscope")
        parseUpscope();
      else if (token == "$dumpvars")
        parseDumpvars();
      else if (token == "$enddefinitions") {
        // empty body
        readStmtString();
        makeVarIdMap();
        parseVarValues();
      }
      token = getToken();

    }
    gzclose(stream_);
  }
}

VcdReader::VcdReader(StaState *sta) :
  StaState(sta),
  stmt_line_(0),
  time_unit_scale_(0.0),
  max_var_name_length_(0),
  max_var_width_(0),
  time_(0),
  time_max_(0)
{
}

void
VcdReader::parseTimescale()
{
  string timescale = readStmtString();
  size_t last;
  time_scale_ = std::stod(timescale, &last);
  if (last == token_.size())
    report_->fileError(800, filename_, stmt_line_, "Missing timescale units.");
  time_unit_ = timescale.substr(last + 1);
  if (time_unit_ == "fs")
    time_unit_scale_ = 1e-15;
  else if (time_unit_ == "ps")
    time_unit_scale_ = 1e-12;
  else if (time_unit_ == "ns")
    time_unit_scale_ = 1e-9;
  else
    report_->fileError(801, filename_, stmt_line_, "Unknown timescale unit.");
}

void
VcdReader::parseVar()
{
  vector<string> tokens = readStmtTokens();
  if (tokens.size() != 4)
    report_->fileError(802, filename_, stmt_line_, "Variable syntax error.");
  else {
    string type_name = tokens[0];
    VcdVar::VarType type = VcdVar::wire;
    if (type_name == "wire")
      type = VcdVar::wire;
    else if (type_name == "reg")
      type = VcdVar::reg;
    else
      report_->fileError(803, filename_, stmt_line_,
                         "Unknown variable type %s.",
                         type_name.c_str());

    int width = stoi(tokens[1]);
    string id = tokens[2];
    string name = tokens[3];
    vars_.push_back(VcdVar(name, type, width,  id));
    max_var_name_length_ = std::max(max_var_name_length_, name.size());
    max_var_width_ = std::max(max_var_width_, width);
  }
}

void
VcdReader::parseScope()
{
}

void
VcdReader::parseUpscope()
{
}

void
VcdReader::parseDumpvars()
{
}

// vars_ grows so the map has to be built after the vars.
void
VcdReader::makeVarIdMap()
{
  for (VcdVar &var : vars_)
    id_var_map_[var.id()] = &var;
}

void
VcdReader::parseVarValues()
{
  string token = getToken();
  while (!token.empty()) {
    if (token[0] == '#') {
      time_ = stoll(token.substr(1));
      //printf("time = %lld\n", time_);
    }
    else if (token[0] == '0'
             || token[0] == '1'
             || token[0] == 'X'
             || token[0] == 'U'
             || token[0] == 'Z')
      appendVarValue(token.substr(1), token[0]);
    else if (token[0] == 'b') {
      if (token[1] == 'X'
          || token[1] == 'U'
          || token[1] == 'Z') {
        string id = getToken();
        // Mixed 0/1/X/U not supported.
        appendVarValue(id, token[1]);
      }
      else {
        string bin = token.substr(1);
        char *end;
        int64_t bus_value = strtol(bin.c_str(), &end, 2);
        string id = getToken();
        auto var_itr = id_var_map_.find(id);
        if (var_itr == id_var_map_.end())
          ("unknown variable %s\n", id.c_str());
        VcdVar *var = var_itr->second;
        //printf("%s = %lld\n", var->name().c_str(), bus_value);
        var->pushBusValue(time_, bus_value);
      }
    }
    token = getToken();
  }
  time_max_ = time_;
}

void
VcdReader::appendVarValue(string id,
                          char value)
{
  auto var_itr = id_var_map_.find(id);
  if (var_itr == id_var_map_.end())
    report_->fileError(800, filename_, stmt_line_, "Unknown var %s",
                       id.c_str());
  VcdVar *var = var_itr->second;
  var->pushValue(time_, value);
}

size_t
VcdReader::varMaxValueCount()
{
  size_t max_count = 0;
  for (VcdVar &var : vars_)
    max_count = max(max_count, var.values().size());
  return max_count;
}

VarTime
VcdReader::varMinTimeDelta()
{
  VarTime min_delta = std::numeric_limits<VarTime>::max();
  for (VcdVar &var : vars_) {
    VcdValues var_values = var.values();
    VarTime prev_time = var_values[0].time();
    for (VcdValue &value : var_values) {
      VarTime time_delta = value.time() - prev_time;
      if (time_delta > 0)
        min_delta = min(min_delta, time_delta);
      prev_time = value.time();
    }
  }
  return min_delta;
}

string
VcdReader::readStmtString()
{
  stmt_line_ = file_line_;
  string line;
  string token = getToken();
  while (!token.empty() && token != "$end") {
    if (!line.empty())
      line += " ";
    line += token;
    token = getToken();
  }
  return line;
}

vector<string>
VcdReader::readStmtTokens()
{
  stmt_line_ = file_line_;
  vector<string> tokens;
  string token = getToken();
  while (!token.empty() && token != "$end") {
    tokens.push_back(token);
    token = getToken();
  }
  return tokens;
}

string
VcdReader::getToken()
{
  string token;
  int ch = gzgetc(stream_);
  if (ch == '\n')
    file_line_++;
  // skip whitespace
  while (ch != EOF && isspace(ch)) {
    ch = gzgetc(stream_);
    if (ch == '\n')
      file_line_++;
  }
  while (ch != EOF && !isspace(ch)) {
    token.push_back(ch);
    ch = gzgetc(stream_);
    if (ch == '\n')
      file_line_++;
  }
  if (ch == EOF)
    return "";
  else
    return token;
}

void
VcdReader::reportWaveforms()
{
  printf("Date: %s\n", date_.c_str());
  printf("Timescale: %.2f%s\n", time_scale_, time_unit_.c_str());
  // Characters per time sample.
  int zoom = (max_var_width_ + 7) / 4;
  int time_delta = varMinTimeDelta();
  // for (double time = 0.0; time < time_max_; time += time_delta) {
  //   //printf("%-*g", time_delta * zoom - 1, time * time_scale_ * time_unit_scale_);
  //   printf("%g", time * time_scale_ * time_unit_scale_);
  // }
  // printf("\n");

  for (VcdVar &var : vars_) {
    printf(" %-*s",
           static_cast<int>(max_var_name_length_),
           var.name().c_str());
    VcdValues var_values = var.values();
    size_t value_index = 0;
    VcdValue var_value = var_values[value_index];
    VcdValue prev_var_value = var_values[value_index];
    VarTime next_value_time = var_values[value_index + 1].time();
    for (double time = 0.0; time < time_max_; time += time_delta) {
      if (time >= next_value_time) {
        if (value_index < var_values.size() - 1)
          value_index++;
        var_value = var_values[value_index];
        if (value_index < var_values.size())
          next_value_time = var_values[value_index + 1].time();
      }
      if (var_value.value()) {
        // 01UZX
        char value = var_value.value();
        char prev_value = prev_var_value.value();
        if (var.width() == 1) {
          if (value == '0' || value == '1') {
            for (int z = 0; z < zoom; z++) {
              if (z == 0
                  && value != prev_value
                  && (prev_value == '0'
                      || prev_value == '1'))
                printf("%s", prev_value == '1' ? "╲" : "╱");
              else
                printf("%s", value == '1' ? "▔" : "▁");
            }
          }
          else
            printf("%-*c", zoom, value);
        }
        else
          printf("%-*c", zoom, value);
      }
      else
        // bus
        printf("%-*llX", zoom, var_value.busValue());
      prev_var_value = var_value;
    }
    printf("\n");
  }
}

////////////////////////////////////////////////////////////////

VcdVar::VcdVar(string name,
               VarType type,
               int width,
               string id) :
  name_(name),
  type_(type),
  width_(width),
  id_(id)
{
}

void
VcdVar::pushValue(VarTime time,
                  char value)
{
  values_.push_back(VcdValue(time, value, 0));
}

void
VcdVar::pushBusValue(VarTime time,
                     int64_t bus_value)
{
  values_.push_back(VcdValue(time, '\0', bus_value));
}

VcdValue::VcdValue(VarTime time,
                   char value,
                   uint64_t bus_value) :
  time_(time),
  value_(value),
  bus_value_(bus_value)
{
}

}
