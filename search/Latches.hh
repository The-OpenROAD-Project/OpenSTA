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

namespace sta {

#include "GraphClass.hh"
#include "SearchClass.hh"

enum class LatchEnableState { enabled, open, closed };

// Latches class defines latch behavior.
class Latches : public StaState
{
public:
  Latches(StaState *sta);
  void latchTimeGivenToStartpoint(Path *d_path,
				  Path *q_path,
				  Edge *d_q_edge,
				  // Return values.
				  Arrival &time_given,
				  PathVertex &enable_path);
  void latchRequired(const Path *data_path,
		     const PathVertex *enable_path,
		     const PathVertex *disable_path,
		     MultiCyclePath *mcp,
		     PathDelay *path_delay,
		     Arrival src_clk_latency,
		     const ArcDelay &margin,
		     // Return values.
		     Required &required,
		     Delay &borrow,
		     Arrival &adjusted_data_arrival,
		     Delay &time_given_to_startpoint);
  void latchRequired(const Path *data_path,
		     const PathVertex *enable_path,
		     const PathVertex *disable_path,
		     const PathAnalysisPt *path_ap,
		     // Return values.
		     Required &required,
		     Delay &borrow,
		     Arrival &adjusted_data_arrival,
		     Delay &time_given_to_startpoint);
  void latchBorrowInfo(const Path *data_path,
		       const PathVertex *enable_path,
		       const PathVertex *disable_path,
		       const ArcDelay &margin,
		       bool ignore_clk_latency,
		       // Return values.
		       float &nom_pulse_width,
		       Delay &open_latency,
		       Delay &latency_diff,
		       float &open_uncertainty,
		       Crpr &open_crpr,
		       Crpr &crpr_diff,
		       Delay &max_borrow,
		       bool &borrow_limit_exists);
  bool isLatchDtoQ(Edge *edge) const;
  // Find the latch EN->Q edge for a D->Q edge.
  void latchDtoQEnable(Edge *d_q_edge,
		       const Instance *inst,
		       // Return values.
		       Vertex *&enable_vertex,
		       RiseFall *&enable_rf,
		       LatchEnableState &state) const;
  LatchEnableState latchDtoQState(Edge *d_q_edge) const;
  void latchEnableOtherPath(Path *path,
			    const PathAnalysisPt *tgt_clk_path_ap,
			    // Return value.
			    PathVertex &other_path);
  void latchEnablePath(Path *q_path, Edge *d_q_edge,
		       // Return value.
		       PathVertex &enable_path) const;
  void latchOutArrival(Path *data_path,
		       TimingArc *d_q_arc,
		       Edge *d_q_edge,
		       const PathAnalysisPt *path_ap,
		       Tag *&q_tag,
		       ArcDelay &arc_delay,
		       Arrival &q_arrival);

protected:
  ArcDelay latchSetupMargin(Vertex *data_vertex,
			    const RiseFall *data_rf,
			    const Path *disable_path,
			    const PathAnalysisPt *path_ap);
  ExceptionPath *exceptionTo(Path *data_path,
			     ClockEdge *en_clk_edge);
};

} // namespace
