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
#include "SdcClass.hh"
#include "SearchClass.hh"
#include "StaState.hh"
#include "PathRef.hh"

namespace sta {

class RiseFall;
class MinPulseWidthCheck;
class MinPulseWidthCheckVisitor;

class CheckMinPulseWidths
{
public:
  explicit CheckMinPulseWidths(StaState *sta);
  ~CheckMinPulseWidths();
  void clear();
  // Min pulse width checks for pins.
  // corner=nullptr checks all corners.
  MinPulseWidthCheckSeq &check(PinSeq *pins,
			       const Corner *corner);
  // All min pulse width checks.
  // corner=nullptr checks all corners.
  MinPulseWidthCheckSeq &check(const Corner *corner);
  // All violating min pulse width checks.
  // corner=nullptr checks all corners.
  MinPulseWidthCheckSeq &violations(const Corner *corner);
  // Min pulse width check with the least slack.
  // corner=nullptr checks all corners.
  MinPulseWidthCheck *minSlackCheck(const Corner *corner);

protected:
  void visitMinPulseWidthChecks(MinPulseWidthCheckVisitor *visitor);
  void visitMinPulseWidthChecks(Vertex *vertex,
				MinPulseWidthCheckVisitor *visitor);

  MinPulseWidthCheckSeq checks_;
  StaState *sta_;

private:
  DISALLOW_COPY_AND_ASSIGN(CheckMinPulseWidths);
};

class MinPulseWidthCheck
{
public:
  explicit MinPulseWidthCheck();
  MinPulseWidthCheck(Path *open_path);
  MinPulseWidthCheck *copy();
  Pin *pin(const StaState *sta) const;
  const RiseFall *openTransition(const StaState *sta) const;
  Arrival width(const StaState *sta) const;
  float minWidth(const StaState *sta) const;
  Slack slack(const StaState *sta) const;
  Path *openPath() { return &open_path_; }
  Corner *corner(const StaState *sta) const;
  const Path *openPath() const { return &open_path_; }
  Arrival openArrival(const StaState *sta) const;
  void closePath(const StaState *sta,
		 // Return value.
		 PathVertex &close) const;
  Arrival closeArrival(const StaState *sta) const;
  Arrival openDelay(const StaState *sta) const;
  Arrival closeDelay(const StaState *sta) const;
  float closeOffset(const StaState *sta) const;
  ClockEdge *openClkEdge(const StaState *sta) const;
  ClockEdge *closeClkEdge(const StaState *sta) const;
  Crpr commonClkPessimism(const StaState *sta) const;

protected:
  // Open path of the pulse.
  PathRef open_path_;

private:
  DISALLOW_COPY_AND_ASSIGN(MinPulseWidthCheck);
};

class MinPulseWidthSlackLess
{
public:
  explicit MinPulseWidthSlackLess(const StaState *sta);
  bool operator()(const MinPulseWidthCheck *check1,
		  const MinPulseWidthCheck *check2) const;

private:
  const StaState *sta_;
};

} // namespace
