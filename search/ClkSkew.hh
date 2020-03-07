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

#include "Map.hh"
#include "SdcClass.hh"

namespace sta {

class ClkSkew;

typedef Map<Clock*, ClkSkew*> ClkSkewMap;

// Find and report min clock skews.
class ClkSkews : public StaState
{
public:
  ClkSkews(StaState *sta);
  // Report clk skews for clks.
  void reportClkSkew(ClockSet *clks,
		     const Corner *corner,
		     const SetupHold *setup_hold,
		     int digits);

protected:
  void findClkSkew(ClockSet *clks,
		   const Corner *corner,
		   const SetupHold *setup_hold,
		   ClkSkewMap &skews);
  bool hasClkPaths(Vertex *vertex,
		   ClockSet *clks);
  void findClkSkewFrom(Vertex *src_vertex,
		       Vertex *q_vertex,
		       RiseFallBoth *src_rf,
		       ClockSet *clks,
		       const Corner *corner,
		       const SetupHold *setup_hold,
		       ClkSkewMap &skews);
  void findClkSkew(Vertex *src_vertex,
		   RiseFallBoth *src_rf,
		   Vertex *tgt_vertex,
		   RiseFallBoth *tgt_rf,
		   ClockSet *clks,
		   const Corner *corner,
		   const SetupHold *setup_hold,
		   ClkSkewMap &skews);
  void findFanout(Vertex *from,
		  // Return value.
		  VertexSet &endpoints);
};
    
} // namespace
