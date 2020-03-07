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
#include "Vector.hh"
#include "Transition.hh"
#include "LibertyClass.hh"

namespace sta {

class LibertyBuilder
{
public:
  LibertyBuilder() {}
  virtual ~LibertyBuilder() {}
  virtual LibertyCell *makeCell(LibertyLibrary *library,
				const char *name,
				const char *filename);
  virtual LibertyPort *makePort(LibertyCell *cell,
				const char *name);
  virtual LibertyPort *makeBusPort(LibertyCell *cell,
				   const char *name,
				   int from_index,
				   int to_index);
  virtual LibertyPort *makeBundlePort(LibertyCell *cell,
				      const char *name,
				      ConcretePortSeq *members);
  // Build timing arc sets and their arcs given a type and sense.
  // Port functions and cell latches are also used by this builder
  // to get the correct roles.
  TimingArcSet *makeTimingArcs(LibertyCell *cell,
			       LibertyPort *from_port,
			       LibertyPort *to_port,
			       LibertyPort *related_out,
			       TimingArcAttrs *attrs);
  InternalPower *makeInternalPower(LibertyCell *cell,
				   LibertyPort *port,
				   LibertyPort *related_port,
				   InternalPowerAttrs *attrs);
  LeakagePower *makeLeakagePower(LibertyCell *cell,
				 LeakagePowerAttrs *attrs);

protected:
  ConcretePort *makeBusPort(const char *name,
			    int from_index,
			    int to_index,
			    ConcretePortSeq *members);
  void makeBusPortBits(ConcreteLibrary *library,
		       LibertyCell *cell,
		       ConcretePort *bus_port,
		       const char *name,
		       int from_index,
		       int to_index);
  // Bus port bit (internal to makeBusPortBits).
  virtual ConcretePort *makePort(LibertyCell *cell,
				 const char *bit_name,
				 int bit_index);
  void makeBusPortBit(ConcreteLibrary *library,
		      LibertyCell *cell,
		      ConcretePort *bus_port,
		      const char *name,
		      int index);
  virtual TimingArcSet *makeTimingArcSet(LibertyCell *cell,
					 LibertyPort *from,
					 LibertyPort *to,
					 LibertyPort *related_out,
					 TimingRole *role,
					 TimingArcAttrs *attrs);
  virtual TimingArc *makeTimingArc(TimingArcSet *set,
				   Transition *from_rf,
				   Transition *to_rf,
				   TimingModel *model);
  TimingArc *makeTimingArc(TimingArcSet *set,
			   RiseFall *from_rf,
			   RiseFall *to_rf,
			   TimingModel *model);
  TimingArcSet *makeCombinationalArcs(LibertyCell *cell,
				      LibertyPort *from_port,
				      LibertyPort *to_port,
				      LibertyPort *related_out,
				      bool to_rise,
				      bool to_fall,
				      TimingArcAttrs *attrs);
  TimingArcSet *makeLatchDtoQArcs(LibertyCell *cell,
				  LibertyPort *from_port,
				  LibertyPort *to_port,
				  LibertyPort *related_out,
				  TimingArcAttrs *attrs);
  TimingArcSet *makeRegLatchArcs(LibertyCell *cell,
				 LibertyPort *from_port,
				 LibertyPort *to_port,
				 LibertyPort *related_out,
				 RiseFall *from_rf,
				 TimingArcAttrs *attrs);
  TimingArcSet *makeFromTransitionArcs(LibertyCell *cell,
				       LibertyPort *from_port,
				       LibertyPort *to_port,
				       LibertyPort *related_out,
				       RiseFall *from_rf,
				       TimingRole *role,
				       TimingArcAttrs *attrs);
  TimingArcSet *makePresetClrArcs(LibertyCell *cell,
				  LibertyPort *from_port,
				  LibertyPort *to_port,
				  LibertyPort *related_out,
				  RiseFall *to_rf,
				  TimingArcAttrs *attrs);
  TimingArcSet *makeTristateEnableArcs(LibertyCell *cell,
				       LibertyPort *from_port,
				       LibertyPort *to_port,
				       LibertyPort *related_out,
				       bool to_rise,
				       bool to_fall,
				       TimingArcAttrs *attrs);
  TimingArcSet *makeTristateDisableArcs(LibertyCell *cell,
					LibertyPort *from_port,
					LibertyPort *to_port,
					LibertyPort *related_out,
					bool to_rise,
					bool to_fall,
					TimingArcAttrs *attrs);
private:
  DISALLOW_COPY_AND_ASSIGN(LibertyBuilder);
};

} // namespace
