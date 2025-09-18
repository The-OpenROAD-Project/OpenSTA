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

#include <cstdint>
#include <string>
#include <vector>

#include "Zlib.hh"
#include "StaState.hh"

namespace sta {

typedef int64_t VcdTime;
typedef std::vector<std::string> VcdScope;

enum class VcdVarType {
  wire,
  reg,
  parameter,
  integer,
  real,
  supply0,
  supply1,
  time,
  tri,
  triand,
  trior,
  trireg,
  tri0,
  tri1,
  wand,
  wor,
  unknown
};

class VcdReader;

class VcdParse : public StaState
{
public:
  VcdParse(Report *report,
           Debug *debug);
  void read(const char *filename,
            VcdReader *reader);

private:
  void parseTimescale();
  void setTimeUnit(const std::string &time_unit,
                   double time_scale);
  void parseVar();
  void parseScope();
  void parseUpscope();
  void parseVarValues();
  std::string getToken();
  std::string readStmtString();
  std::vector<std::string> readStmtTokens();

  VcdReader *reader_;
  gzFile stream_;
  std::string token_;
  const char *filename_;
  int file_line_;
  int stmt_line_;

  VcdTime time_;
  VcdTime prev_time_;
  VcdScope scope_;

  Report *report_;
  Debug *debug_;
};

// Abstract class for VcdParse callbacks.
class VcdReader
{
public:
  virtual ~VcdReader() {}
  virtual void setDate(const std::string &date) = 0;
  virtual void setComment(const std::string &comment) = 0;
  virtual void setVersion(const std::string &version) = 0;
  virtual void setTimeUnit(const std::string &time_unit,
                           double time_unit_scale,
                           double time_scale) = 0;
  virtual void setTimeMin(VcdTime time) = 0;
  virtual void setTimeMax(VcdTime time) = 0;
  virtual void varMinDeltaTime(VcdTime min_delta_time) = 0;
  virtual bool varIdValid(const std::string &id) = 0;
  virtual void makeVar(const VcdScope &scope,
                       const std::string &name,
                       VcdVarType type,
                       size_t width,
                       const std::string &id) = 0;
  virtual void varAppendValue(const std::string &id,
                              VcdTime time,
                              char value) = 0;
  virtual void varAppendBusValue(const std::string &id,
                                 VcdTime time,
                                 int64_t bus_value) = 0;
};

class VcdValue
{
public:
  VcdValue();
  VcdTime time() const { return time_; }
  char value() const { return value_; }
  void setValue(VcdTime time,
                char value);
  uint64_t busValue() const { return bus_value_; }
  char value(int value_bit) const;

private:
  VcdTime time_;
  // 01XUZ or '\0' when width > 1 to use bus_value_.
  char value_;
  uint64_t bus_value_;
};

} // namespace
