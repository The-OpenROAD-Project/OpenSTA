// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include <set>
#include <array>

#include "Zlib.hh"
#include "NetworkClass.hh"
#include "StaState.hh"

// Header for SaifReader.cc to communicate with SaifLex.cc, SaifParse.cc

// global namespace

#define YY_INPUT(buf,result,max_size) \
  sta::saif_reader->getChars(buf, result, max_size)
int
SaifParse_error(const char *msg);

namespace sta {

class Sta;
class Power;

using std::vector;
using std::string;

enum class SaifState { T0, T1, TX, TZ, TB, TC, IG };

typedef std::array<uint64_t, static_cast<int>(SaifState::IG)+1> SaifStateDurations;

class SaifReader : public StaState
{
public:
  SaifReader(const char *filename,
             const char *scope,
             Sta *sta);
  ~SaifReader();
  bool read();

  void setDivider(char divider);
  void setTimescale(uint64_t multiplier,
                    const char *units);
  void setDuration(uint64_t duration);
  void instancePush(const char *instance_name);
  void instancePop();
  void setNetDurations(const char *net_name,
                       SaifStateDurations &durations);

  // flex YY_INPUT yy_n_chars arg changed definition from int to size_t,
  // so provide both forms.
  void getChars(char *buf,
		size_t &result,
		size_t max_size);
  void getChars(char *buf,
		int &result,
		size_t max_size);
  void incrLine();
  const char *filename() { return filename_; }
  int line() { return line_; }
  void saifWarn(int id,
                const char *fmt, ...);
  void saifError(int id,
                 const char *fmt,
                 ...);
  void notSupported(const char *feature);

private:
  string unescaped(const char *token);

  const char *filename_;
  const char *scope_;           // Divider delimited scope to begin annotation.

  gzFile stream_;
  int line_;
  char divider_;
  char escape_;
  double timescale_;
  int64_t duration_;
  double clk_period_;

  vector<string> saif_scope_;   // Scope during parsing.
  size_t in_scope_level_;
  vector<Instance*> path_;      // Path within scope.
  std::set<const Pin*> annotated_pins_;
  Power *power_;
};

extern SaifReader *saif_reader;

} // namespace
