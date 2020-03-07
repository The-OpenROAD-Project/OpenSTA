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

#include "SdcClass.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"

namespace sta {

class StaState;
class PathEndVisitor;

class VisitPathEnds : public StaState
{
public:
  VisitPathEnds(const StaState *sta);
  // All corners, unfiltered.
  void visitPathEnds(Vertex *vertex,
		     PathEndVisitor *visitor);
  // Use corner nullptr to visit PathEnds for all corners.
  void visitPathEnds(Vertex *vertex,
		     const Corner *corner,
		     const MinMaxAll *min_max,
		     bool filtered,
		     PathEndVisitor *visitor);
  bool checkEdgeEnabled(Edge *edge) const;

protected:
  void visitClkedPathEnds(const Pin *pin,
			  Vertex *vertex,
			  const Corner *corner,
			  const MinMaxAll *min_max,
			  bool filtered,
			  PathEndVisitor *visitor,
			  bool &is_constrained);
  virtual void visitCheckEnd(const Pin *pin,
			     Vertex *vertex,
			     Path *path,
			     const RiseFall *end_rf,
			     const PathAnalysisPt *path_ap,
			     bool filtered,
			     PathEndVisitor *visitor,
			     bool &is_constrained);
  void visitCheckEndUnclked(const Pin *pin,
			    Vertex *vertex,
			    Path *path,
			    const RiseFall *end_rf,
			    const PathAnalysisPt *path_ap,
			    bool filtered,
			    PathEndVisitor *visitor,
			    bool &is_constrained);
  void visitOutputDelayEnd(const Pin *pin,
			   Path *path,
			   const RiseFall *end_rf,
			   const PathAnalysisPt *path_ap,
			   bool filtered,
			   PathEndVisitor *visitor,
			   bool &is_constrained);
  virtual void visitOutputDelayEnd1(OutputDelay *output_delay,
				    const Pin *pin,
				    Path *path,
				    const RiseFall *end_rf,
				    const ClockEdge *tgt_clk_edge,
				    PathVertex *ref_path,
				    const MinMax *min_max,
				    PathEndVisitor *visitor,
				    bool &is_constrained);
  virtual void visitGatedClkEnd(const Pin *pin,
				Vertex *vertex,
				Path *path,
				const RiseFall *end_rf,
				const PathAnalysisPt *path_ap,
				bool filtered,
				PathEndVisitor *visitor,
				bool &is_constrained);
  float clockGatingMargin(const Clock *clk,
			  const Pin *clk_pin,
			  const Pin *enable_pin,
			  const RiseFall *enable_rf,
			  const SetupHold *setup_hold);
  void visitDataCheckEnd(const Pin *pin,
			 Path *path,
			 const RiseFall *end_rf,
			 const PathAnalysisPt *path_ap,
			 bool filtered,
			 PathEndVisitor *visitor,
			 bool &is_constrained);
  bool visitDataCheckEnd1(DataCheck *check,
			  const Pin *pin,
			  Path *path,
			  const Clock *src_clk,
			  const RiseFall *end_rf,
			  const MinMax *min_max,
			  const PathAnalysisPt *clk_ap,
			  const Pin *from_pin,
			  Vertex *from_vertex,
			  RiseFall *from_rf,
			  bool filtered,
			  PathEndVisitor *visitor,
			  bool &is_constrained);
  virtual void visitUnconstrainedPathEnds(const Pin *pin,
					  Vertex *vertex,
					  const Corner *corner,
					  const MinMaxAll *min_max,
					  bool filtered,
					  PathEndVisitor *visitor);
  bool falsePathTo(Path *path,
		   const Pin *pin,
		   const RiseFall *rf,
		   const MinMax *min_max);
  PathDelay *pathDelayTo(Path *path,
			 const Pin *pin,
			 const RiseFall *rf,
			 const MinMax *min_max);
  ExceptionPath *exceptionTo(const Path *path,
			     const Pin *pin,
			     const RiseFall *rf,
			     const ClockEdge *clk_edge,
			     const MinMax *min_max) const;
};

// Abstract base class used by visitPathEnds to visit vertex path ends.
class PathEndVisitor
{
public:
  virtual ~PathEndVisitor() {}
  virtual PathEndVisitor *copy() = 0;
  // Begin visiting the path ends for a vertex / path_index.
  virtual void vertexBegin(Vertex *) {}
  // Visit a path end.  path_end is only valid during the call.
  virtual void visit(PathEnd *path_end) = 0;
  // End visiting the path ends for a vertex / path_index.
  virtual void vertexEnd(Vertex *) {}
};

} // namespace
