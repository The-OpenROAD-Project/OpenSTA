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

#include "VcdParse.hh"

#include <cctype>
#include <cinttypes>

#include "Stats.hh"
#include "Report.hh"
#include "Error.hh"
#include "EnumNameMap.hh"

namespace sta {

using std::isspace;

// Very imprecise syntax definition
// https://en.wikipedia.org/wiki/Value_change_dump#Structure.2FSyntax
// Much better syntax definition
// https://web.archive.org/web/20120323132708/http://www.beyondttl.com/vcd.php

void
VcdParse::read(const char *filename,
               VcdReader *reader)
{
  stream_ = gzopen(filename, "r");
  if (stream_) {
    Stats stats(debug_, report_);
    filename_ = filename;
    reader_ = reader;
    file_line_ = 1;
    stmt_line_ = 1;
    string token = getToken();
    while (!token.empty()) {
      if (token == "$date")
        reader_->setDate(readStmtString());
      else if (token == "$comment")
        reader_->setComment(readStmtString());
      else if (token == "$version")
        reader_->setVersion(readStmtString());
      else if (token == "$timescale")
        parseTimescale();
      else if (token == "$var")
        parseVar();
      else if (token == "$scope")
        parseScope();
      else if (token == "$upscope")
        parseUpscope();
      else if (token == "$enddefinitions")
        // empty body
        readStmtString();
      else if (token == "$dumpall")
        parseVarValues();
      else if (token == "$dumpvars")
        // Initial values.
        parseVarValues();
      else if (token[0] == '$')
        report_->fileError(800, filename_, stmt_line_, "unhandled vcd command.");
      else
        parseVarValues();
      token = getToken();
    }
    gzclose(stream_);
    stats.report("Read VCD");
  }
  else
    throw FileNotReadable(filename);
}

VcdParse::VcdParse(Report *report,
                   Debug *debug) :
  reader_(nullptr),
  file_line_(0),
  stmt_line_(0),
  time_(0),
  prev_time_(0),
  report_(report),
  debug_(debug)
{
}

void
VcdParse::parseTimescale()
{
  vector<string> tokens = readStmtTokens();
  if (tokens.size() == 1) {
    size_t last;
    double time_scale = std::stod(tokens[0], &last);
    setTimeUnit(tokens[0].substr(last), time_scale);
  }
  else if (tokens.size() == 2) {
    double time_scale = std::stod(tokens[0]);
    setTimeUnit(tokens[1], time_scale);
  }
  else
    report_->fileError(801, filename_, stmt_line_, "timescale syntax error.");
}

void
VcdParse::setTimeUnit(const string &time_unit,
                      double time_scale)
{
  double time_unit_scale = 1.0;
  if (time_unit == "fs")
    time_unit_scale = 1e-15;
  else if (time_unit == "ps")
    time_unit_scale = 1e-12;
  else if (time_unit == "ns")
    time_unit_scale = 1e-9;
  else
    report_->fileError(802, filename_, stmt_line_, "Unknown timescale unit.");
  reader_->setTimeUnit(time_unit, time_unit_scale, time_scale);
}

static EnumNameMap<VcdVarType> vcd_var_type_map =
  {{VcdVarType::wire, "wire"},
   {VcdVarType::reg, "reg"},
   {VcdVarType::parameter, "parameter"},
   {VcdVarType::integer, "integer"},
   {VcdVarType::real, "real"},
   {VcdVarType::supply0, "supply0"},
   {VcdVarType::supply1, "supply1"},
   {VcdVarType::time, "time"},
   {VcdVarType::tri, "tri"},
   {VcdVarType::triand, "triand"},
   {VcdVarType::trior, "trior"},
   {VcdVarType::trireg, "trireg"},
   {VcdVarType::tri0, "tri0"},
   {VcdVarType::tri1, "tri1"},
   {VcdVarType::wand, "wand"},
   {VcdVarType::wor, "wor"}
  };

void
VcdParse::parseVar()
{
  vector<string> tokens = readStmtTokens();
  if (tokens.size() == 4
      || tokens.size() == 5) {
    string type_name = tokens[0];
    VcdVarType type = vcd_var_type_map.find(type_name, VcdVarType::unknown);
    if (type == VcdVarType::unknown)
      report_->fileWarn(1370, filename_, stmt_line_,
                        "Unknown variable type %s.",
                        type_name.c_str());
    else {
      size_t width = stoi(tokens[1]);
      string &id = tokens[2];
      string name = tokens[3];
      // iverilog separates bus base name from bit range.
      if (tokens.size() == 5) {
        // Preserve space after esacaped name.
        if (name[0] == '\\')
          name += ' ';
        name += tokens[4];
      }

      reader_->makeVar(scope_, name, type, width, id);
    }
  }
  else
    report_->fileError(804, filename_, stmt_line_, "Variable syntax error.");
}

void
VcdParse::parseScope()
{
  vector<string> tokens = readStmtTokens();
  string &scope = tokens[1];
  scope_.push_back(scope);
}

void
VcdParse::parseUpscope()
{
  readStmtTokens();
  scope_.pop_back();
}

void
VcdParse::parseVarValues()
{
  string token = getToken();
  while (!token.empty()) {
    char char0 = toupper(token[0]);
    if (char0 == '#' && token.size() > 1) {
      prev_time_ = time_;
      time_ = stoll(token.substr(1));
      if (time_ > prev_time_)
        reader_->varMinDeltaTime(time_ - prev_time_);
    }
    else if (char0 == '0'
             || char0 == '1'
             || char0 == 'X'
             || char0 == 'U'
             || char0 == 'Z') {
      string id = token.substr(1);
      if (!reader_->varIdValid(id))
        report_->fileError(805, filename_, stmt_line_,
                           "unknown variable %s", id.c_str());
      reader_->varAppendValue(id, time_, char0);
    }
    else if (char0 == 'B') {
      char char1 = toupper(token[1]);
      if (char1 == 'X'
          || char1 == 'U'
          || char1 == 'Z') {
        string id = getToken();
        if (!reader_->varIdValid(id))
          report_->fileError(806, filename_, stmt_line_,
                             "unknown variable %s", id.c_str());
        // Bus mixed 0/1/X/U not supported.
        reader_->varAppendValue(id, time_, char1);
      }
      else {
        string bin = token.substr(1);
        char *end;
        int64_t bus_value = strtol(bin.c_str(), &end, 2);
        string id = getToken();
        if (!reader_->varIdValid(id))
          report_->fileError(807, filename_, stmt_line_,
                             "unknown variable %s", id.c_str());
        else
          reader_->varAppendBusValue(id, time_, bus_value);
      }
    }
    token = getToken();
  }
  reader_->setTimeMax(time_);
}

string
VcdParse::readStmtString()
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
VcdParse::readStmtTokens()
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
VcdParse::getToken()
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

////////////////////////////////////////////////////////////////

char
VcdValue::value(int value_bit) const
{
  if (value_ == '\0')
    return ((bus_value_ >> value_bit) & 0x1) ? '1' : '0';
  else
    return value_;
}

void
VcdValue::setValue(VcdTime time,
                   char value)
{
  time_ = time;
  value_ = value;
}

} // namespace
