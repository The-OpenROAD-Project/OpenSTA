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

#include <vector>

#include "GraphClass.hh"
#include "Delay.hh"
#include "StaState.hh"
#include "SearchClass.hh"
#include "Path.hh"
#include "MinMax.hh"

namespace sta {

class MaxSkewCheck
{
public:
  MaxSkewCheck();
  MaxSkewCheck(Path *clk_path,
               Path *ref_path,
               TimingArc *check_arc,
               Edge *check_edge);
  bool isNull() const { return clk_path_ == nullptr; }
  const Path *clkPath() const { return clk_path_; }
  Pin *clkPin(const StaState *sta) const;
  const Path *refPath() const { return ref_path_; }
  Pin *refPin(const StaState *sta) const;
  Delay skew() const;
  ArcDelay maxSkew(const StaState *sta) const;
  Slack slack(const StaState *sta) const;
  TimingArc *checkArc() const { return check_arc_; }

private:
  Path *clk_path_;
  Path *ref_path_;
  TimingArc *check_arc_;
  Edge *check_edge_;
};

using MaxSkewCheckSeq = std::vector<MaxSkewCheck>;

class CheckMaxSkews
{
public:
  CheckMaxSkews(StaState *sta);
  ~CheckMaxSkews();
  void clear();
  // Return max skew checks.
  // net=null check all nets
  MaxSkewCheckSeq &check(const Net *net,
                         size_t max_count,
                         bool violators,
                         const SceneSeq &scenes);

protected:
  void check(Vertex *vertex,
             bool violators);

  SceneSet scenes_;
  MaxSkewCheckSeq checks_;
  StaState *sta_;
};

class MaxSkewSlackLess
{
public:
  MaxSkewSlackLess(const StaState *sta);
  bool operator()(const MaxSkewCheck &check1,
                  const MaxSkewCheck &check2) const;

protected:
  const StaState *sta_;
};

} // namespace
