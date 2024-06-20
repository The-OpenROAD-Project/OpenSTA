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

#include <map>

#include "UnorderedSet.hh"
#include "SdcClass.hh"
#include "StaState.hh"
#include "Transition.hh"
#include "SearchClass.hh"
#include "SearchPred.hh"
#include "PathVertex.hh"

namespace sta {

class ClkSkew;
class SearchPred;

typedef std::map<const Clock*, ClkSkew> ClkSkewMap;

class FanOutSrchPred : public SearchPred1
{
public:
  FanOutSrchPred(const StaState *sta);
  virtual bool searchThru(Edge *edge);
};

// Find and report clock skews between source/target registers.
class ClkSkews : public StaState
{
public:
  ClkSkews(StaState *sta);
  // Report clk skews for clks.
  void reportClkSkew(ConstClockSeq &clks,
		     const Corner *corner,
		     const SetupHold *setup_hold,
                     bool include_internal_latency,
		     int digits);
  // Find worst clock skew between src/target registers.
  float findWorstClkSkew(const Corner *corner,
                         const SetupHold *setup_hold,
                         bool include_internal_latency);
  
protected:
  ClkSkewMap findClkSkew(ConstClockSeq &clks,
                         const Corner *corner,
                         const SetupHold *setup_hold,
                         bool include_internal_latency);
  bool hasClkPaths(Vertex *vertex);
  void findClkSkewFrom(Vertex *src_vertex,
		       ClkSkewMap &skews);
  void findClkSkewFrom(Vertex *src_vertex,
		       Vertex *q_vertex,
		       const RiseFallBoth *src_rf,
		       ClkSkewMap &skews);
  void findClkSkew(Vertex *src_vertex,
		   const RiseFallBoth *src_rf,
		   Vertex *tgt_vertex,
		   const RiseFallBoth *tgt_rf,
		   ClkSkewMap &skews);
  VertexSet findFanout(Vertex *from);
  void findFanout1(Vertex *from,
                   UnorderedSet<Vertex*> &visited,
                   VertexSet &endpoints);
  void reportClkSkew(ClkSkew &clk_skew,
                     int digits);

  ConstClockSet clk_set_;
  const Corner *corner_;
  const SetupHold *setup_hold_;
  bool include_internal_latency_;
  FanOutSrchPred fanout_pred_;
};

} // namespace
