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

#pragma once

#include <queue>
#include "DisallowCopyAssign.hh"
#include "Iterator.hh"
#include "Vector.hh"
#include "StaState.hh"
#include "SearchClass.hh"
#include "Path.hh"

namespace sta {

class Diversion;
class PathEnumFaninVisitor;
class PathEnumed;
class DiversionGreater;

typedef Vector<Diversion*> DiversionSeq;
typedef Vector<PathEnumed*> PathEnumedSeq;
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
  PathEnum(int group_count,
	   int endpoint_count,
	   bool unique_pins,
	   bool cmp_slack,
	   const StaState *sta);
  // Insert path ends that are enumerated in slack/arrival order.
  void insert(PathEnd *path_end);
  virtual ~PathEnum();
  virtual bool hasNext();
  virtual PathEnd *next();

private:
  DISALLOW_COPY_AND_ASSIGN(PathEnum);
  void makeDiversions(PathEnd *path_end,
		      Path *before);
  void makeDiversion(PathEnd *div_end,
		     PathEnumed *after_div_copy);
  void makeDivertedPath(Path *path,
			Path *before_div,
			Path *after_div,
			TimingArc *div_arc,
			// Returned values.
			PathEnumed *&div_path,
			PathEnumed *&after_div_copy);
  void updatePathHeadDelays(PathEnumedSeq &path,
			    Path *after_div);
  Arrival divSlack(Path *path,
		   Path *after_div,
		   TimingArc *div_arc,
		   const PathAnalysisPt *path_ap);
  void reportDiversionPath(Diversion *div);
  void pruneDiversionQueue();
  Edge *divEdge(Path *before_div,
		TimingArc *div_arc);
  void findNext();

  bool cmp_slack_;
  int group_count_;
  int endpoint_count_;
  bool unique_pins_;
  DiversionQueue div_queue_;
  int div_count_;
  // Number of paths returned for each endpoint (limited to endpoint_count).
  VertexPathCountMap path_counts_;
  bool inserts_pruned_;
  PathEnd *next_;

  friend class PathEnumFaninVisitor;
};

} // namespace
