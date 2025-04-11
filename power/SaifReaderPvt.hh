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
#include <vector>
#include <string>
#include <set>
#include <array>

#include "Zlib.hh"
#include "NetworkClass.hh"
#include "StaState.hh"

// Header for SaifReader.cc to communicate with SaifLex.cc, SaifParse.cc

// global namespace

namespace sta {

class Sta;
class Power;
class SaifScanner;

enum class SaifState { T0, T1, TX, TZ, TB, TC, IG };

typedef std::array<uint64_t, static_cast<int>(SaifState::IG)+1> SaifStateDurations;

class SaifReader : public StaState
{
public:
  SaifReader(const char *filename,
             const char *scope,
             Sta *sta);
  bool read();

  void setDivider(char divider);
  void setTimescale(uint64_t multiplier,
                    const char *units);
  void setDuration(uint64_t duration);
  void instancePush(const char *instance_name);
  void instancePop();
  void setNetDurations(const char *net_name,
                       SaifStateDurations &durations);
  const char *filename() { return filename_; }

private:
  std::string unescaped(const char *token);

  const char *filename_;
  const char *scope_;           // Divider delimited scope to begin annotation.

  char divider_;
  char escape_;
  double timescale_;
  int64_t duration_;

  std::vector<std::string> saif_scope_;   // Scope during parsing.
  size_t in_scope_level_;
  std::vector<Instance*> path_;      // Path within scope.
  std::set<const Pin*> annotated_pins_;
  Power *power_;
};

} // namespace
