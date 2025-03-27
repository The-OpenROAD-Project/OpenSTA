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

#include <queue>

#include "Iterator.hh"
#include "Vector.hh"
#include "StaState.hh"
#include "SearchClass.hh"
#include "Path.hh"

namespace sta {

class Diversion;
class PathEnumFaninVisitor;
class DiversionGreater;

typedef Vector<Diversion*> DiversionSeq;
typedef std::priority_queue<Diversion*,DiversionSeq,
			    DiversionGreater> DiversionQueue;

class DiversionGreater
{
public:
  DiversionGreater();
  DiversionGreater(const StaState *sta);
  bool operator()(Diversion *div1,
		  Diversion *div2) const;

private:
  const StaState *sta_;
};

// Iterator to enumerate sucessively slower paths.
class PathEnum : public Iterator<PathEnd*>, StaState
{
public:
  PathEnum(size_t group_path_count,
	   size_t endpoint_path_count,
	   bool unique_pins,
	   bool cmp_slack,
	   const StaState *sta);
  // Insert path ends that are enumerated in slack/arrival order.
  void insert(PathEnd *path_end);
  virtual ~PathEnum();
  virtual bool hasNext();
  virtual PathEnd *next();

private:
  void makeDiversions(PathEnd *path_end,
		      Path *before);
  void makeDiversion(PathEnd *div_end,
		     Path *after_div_copy);
  void makeDivertedPath(Path *path,
			Path *before_div,
			Path *after_div,
                        Edge *div_edge,
			TimingArc *div_arc,
			// Returned values.
			Path *&div_path,
			Path *&after_div_copy);
  void updatePathHeadDelays(PathSeq &path,
			    Path *after_div);
  Arrival divSlack(Path *path,
		   Path *after_div,
                   const Edge *div_edge,
		   const TimingArc *div_arc,
		   const PathAnalysisPt *path_ap);
  void reportDiversionPath(Diversion *div);
  void pruneDiversionQueue();
  void findNext();

  bool cmp_slack_;
  size_t group_path_count_;
  size_t endpoint_path_count_;
  bool unique_pins_;
  DiversionQueue div_queue_;
  int div_count_;
  // Number of paths returned for each endpoint (limit to endpoint_path_count).
  VertexPathCountMap path_counts_;
  bool inserts_pruned_;
  PathEnd *next_;

  friend class PathEnumFaninVisitor;
};

} // namespace
