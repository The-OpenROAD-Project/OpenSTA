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

#include "Zlib.hh"
#include "ReadVcd.hh"

namespace sta {

using std::string;
using std::isspace;
using std::vector;
using std::map;

class VcdVar;
class VcdValue;

class VcdReader
{
public:
  VcdReader();
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
  string getToken();
  string readStmtString();
  vector<string> readStmtTokens();


  gzFile stream_;
  string token_;

  string date_;
  string comment_;
  string version_;
  double time_scale_;
  string time_unit_;
  double time_unit_scale_;
  vector<VcdVar> vars_;
  map<string, VcdVar*> id_var_map_;
  int64_t time_;
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
  void pushValue(int64_t time,
                 char value);
  void pushBusValue(int64_t time,
                    int64_t bus_value);

private:
  string name_;
  VarType type_;
  int width_;
  string id_;
  vector<VcdValue> values_;
};

class VcdValue
{
public:
  VcdValue(uint64_t time,
           char value,
           uint64_t bus_value);
  uint64_t time() const { return time_; }
  char value() const { return value_; }
  uint64_t busValue() const { return bus_value_; }

private:
  uint64_t time_;
  // 01XUZ or '\0' when width > 1 to use bus_value_.
  char value_;
  uint64_t bus_value_;
};

void
readVcdFile(const char *filename)

{
  VcdReader reader;
  reader.read(filename);
  reader.reportWaveforms();
}

void
VcdReader::read(const char *filename)
{
  stream_ = gzopen(filename, "r");
  if (stream_) {
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

VcdReader::VcdReader() :
  time_(0)
{
}

void
VcdReader::parseTimescale()
{
  string timescale = readStmtString();
  size_t last;
  time_scale_ = std::stod(timescale, &last);
  if (last == token_.size())
    printf("Missing timescale units\n");
  time_unit_ = timescale.substr(last + 1);
  if (time_unit_ == "fs")
    time_unit_scale_ = 1e-15;
  else if (time_unit_ == "ps")
    time_unit_scale_ = 1e-12;
  else if (time_unit_ == "ns")
    time_unit_scale_ = 1e-9;
  else
    printf("unknown timescale unit\n");
}

void
VcdReader::parseVar()
{
  vector<string> tokens = readStmtTokens();
  if (tokens.size() != 4)
    printf("$var syntax error\n");
  else {
    string type_name = tokens[0];
    VcdVar::VarType type = VcdVar::wire;
    if (type_name == "wire")
      type = VcdVar::wire;
    else if (type_name == "reg")
      type = VcdVar::reg;
    else
    printf("$var unknown variable type\n");

    int width = stoi(tokens[1]);
    string id = tokens[2];
    string name = tokens[3];
    vars_.push_back(VcdVar(name, type, width,  id));
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
      printf("time = %lld\n", time_);
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
        int64_t bus_value = stoll(token.substr(1));
        string id = getToken();
        auto var_itr = id_var_map_.find(id);
        if (var_itr == id_var_map_.end())
          printf("unknown var %s\n", id.c_str());
        VcdVar *var = var_itr->second;
        printf("%s = %lld\n", var->name().c_str(), bus_value);
        var->pushBusValue(time_, bus_value);
      }
    }
    token = getToken();
  }
}

void
VcdReader::appendVarValue(string id,
                          char value)
{
  auto var_itr = id_var_map_.find(id);
  if (var_itr == id_var_map_.end())
    printf("unknown var %s\n", id.c_str());
  VcdVar *var = var_itr->second;
  printf("%s = %c\n", var->name().c_str(), value);
  var->pushValue(time_, value);
}

string
VcdReader::readStmtString()
{
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
  // skip whitespace
  while (ch != EOF && isspace(ch))
    ch = gzgetc(stream_);
  while (ch != EOF && !isspace(ch)) {
    token.push_back(ch);
    ch = gzgetc(stream_);
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
  for (VcdVar &var : vars_) {
    printf(" %s %d %s\n",
           var.name().c_str(),
           var.width(),
           var.id().c_str());
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
VcdVar::pushValue(int64_t time,
                  char value)
{
  values_.push_back(VcdValue(time, value, 0));
}

void
VcdVar::pushBusValue(int64_t time,
                     int64_t bus_value)
{
  values_.push_back(VcdValue(time, '\0', bus_value));
}

VcdValue::VcdValue(uint64_t time,
                   char value,
                   uint64_t bus_value) :
  time_(time),
  value_(value),
  bus_value_(bus_value)
{
}

}
