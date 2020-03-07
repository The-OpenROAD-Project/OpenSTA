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

#include "DisallowCopyAssign.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SdcClass.hh"
#include "MinMax.hh"
#include "Transition.hh"
#include "Delay.hh"
#include "StaState.hh"
#include "SearchClass.hh"

namespace sta {

class DcalcAnalysisPt;

// Abstract base class for Path API.
class Path
{
public:
  Path() {}
  virtual ~Path() {}
  virtual const char *name(const StaState *sta) const;
  virtual bool isNull() const = 0;
  virtual Path *path() { return isNull() ? nullptr : this; }
  virtual const Path *path() const { return isNull() ? nullptr : this; }
  virtual void setRef(PathRef *ref) const = 0;
  virtual void setRef(PathRef &ref) const { setRef(&ref); }
  virtual Vertex *vertex(const StaState *sta) const = 0;
  virtual VertexId vertexId(const StaState *sta) const = 0;
  virtual Pin *pin(const StaState *sta) const;
  virtual Tag *tag(const StaState *sta) const = 0;
  virtual TagIndex tagIndex(const StaState *sta) const;
  virtual ClkInfo *clkInfo(const StaState *sta) const;
  virtual ClockEdge *clkEdge(const StaState *sta) const;
  virtual Clock *clock(const StaState *sta) const;
  virtual bool isClock(const StaState *sta) const;
  virtual const RiseFall *transition(const StaState *sta) const = 0;
  virtual int rfIndex(const StaState *sta) const;
  virtual const MinMax *minMax(const StaState *sta) const;
  virtual PathAnalysisPt *pathAnalysisPt(const StaState *sta) const = 0;
  virtual PathAPIndex pathAnalysisPtIndex(const StaState *sta) const;
  virtual DcalcAnalysisPt *dcalcAnalysisPt(const StaState *sta) const;
  virtual Arrival arrival(const StaState *sta) const = 0;
  virtual void setArrival(Arrival arrival,
			  const StaState *sta) = 0;
  virtual void initArrival(const StaState *sta);
  virtual bool arrivalIsInitValue(const StaState *sta) const;
  virtual const Required &required(const StaState *sta) const = 0;
  virtual void setRequired(const Required &required,
			   const StaState *sta) = 0;
  virtual void initRequired(const StaState *sta);
  virtual bool requiredIsInitValue(const StaState *sta) const;
  virtual Slack slack(const StaState *sta) const;
  virtual Slew slew(const StaState *sta) const;
  // This takes the same time as prevPath and prevArc combined.
  virtual void prevPath(const StaState *sta,
			// Return values.
			PathRef &prev_path,
			TimingArc *&prev_arc) const = 0;
  virtual void prevPath(const StaState *sta,
			// Return values.
			PathRef &prev_path) const;
  virtual TimingArc *prevArc(const StaState *sta) const;
  // Find the previous edge given the previous arc found above.
  Edge *prevEdge(const TimingArc *prev_arc,
		 const StaState *sta) const;

  static bool less(const Path *path1,
		   const Path *path2,
		   const StaState *sta);
  static int cmp(const Path *path1,
		 const Path *path2,
		 const StaState *sta);
  // Compare all path attributes (vertex, transition, tag, analysis point).
  static bool equal(const Path *path1,
		    const Path *path2,
		    const StaState *sta);
  // Compare pin name and transition and source clock edge.
  static int cmpPinTrClk(const Path *path1,
			 const Path *path2,
			 const StaState *sta);
  // Compare source clock edge.
  static int cmpClk(const Path *path1,
		    const Path *path2,
		    const StaState *sta);
  // Compare vertex, transition, path ap and tag without crpr clk pin.
  static int cmpNoCrpr(const Path *path1,
		       const Path *path2,
		       const StaState *sta);
  // Search back on each path until finding a difference.
  static int cmpAll(const Path *path1,
		    const Path *path2,
		    const StaState *sta);
  static bool lessAll(const Path *path1,
		      const Path *path2,
		      const StaState *sta);
};

// Compare all path attributes (vertex, transition, tag, analysis point).
class PathLess
{
public:
  explicit PathLess(const StaState *sta);
  bool operator()(const Path *path1,
		  const Path *path2) const;

protected:
  const StaState *sta_;
};

} // namespace
