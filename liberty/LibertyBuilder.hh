// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include <string>
#include <string_view>

#include "MinMax.hh"
#include "Transition.hh"
#include "LibertyClass.hh"
#include "ConcreteLibrary.hh"

namespace sta {

class TimingArcAttrs;
class Debug;
class Report;

class LibertyBuilder
{
public:
  LibertyBuilder(Debug *debug,
                 Report *report);
  LibertyCell *makeCell(LibertyLibrary *library,
                        std::string_view name,
                        std::string_view filename);
  LibertyPort *makePort(LibertyCell *cell,
                        std::string_view name);
  LibertyPort *makeBusPort(LibertyCell *cell,
                           std::string_view bus_name,
                           int from_index,
                           int to_index,
                           BusDcl *bus_dcl);
  LibertyPort *makeBundlePort(LibertyCell *cell,
                              std::string_view name,
                              ConcretePortSeq *members);
  // Build timing arc sets and their arcs given a type and sense.
  // Port functions and cell latches are also used by this builder
  // to get the correct roles.
  TimingArcSet *makeTimingArcs(LibertyCell *cell,
                               LibertyPort *from_port,
                               LibertyPort *to_port,
                               LibertyPort *related_out,
                               const TimingArcAttrsPtr &attrs);
  TimingArcSet *makeFromTransitionArcs(LibertyCell *cell,
                                       LibertyPort *from_port,
                                       LibertyPort *to_port,
                                       LibertyPort *related_out,
                                       const RiseFall *from_rf,
                                       const TimingRole *role,
                                       const TimingArcAttrsPtr &attrs);
  TimingArcSet *makeCombinationalArcs(LibertyCell *cell,
                                      LibertyPort *from_port,
                                      LibertyPort *to_port,
                                      bool to_rise,
                                      bool to_fall,
                                      const TimingArcAttrsPtr &attrs);
  TimingArcSet *makeClockTreePathArcs(LibertyCell *cell,
                                      LibertyPort *to_port,
                                      const TimingRole *role,
                                      const TimingArcAttrsPtr &attrs);
  TimingArcSet *makeMinPulseWidthArcs(LibertyCell *cell,
                                      LibertyPort *from_port,
                                      LibertyPort *to_port,
                                      LibertyPort *related_out,
                                      const TimingRole *role,
                                      const TimingArcAttrsPtr &attrs);

protected:
  ConcretePort *makeBusPort(std::string_view name,
                            int from_index,
                            int to_index,
                            ConcretePortSeq *members);
  void makeBusPortBits(ConcreteLibrary *library,
                       LibertyCell *cell,
                       ConcretePort *bus_port,
                       std::string_view bus_name,
                       int from_index,
                       int to_index);
  // Bus port bit (internal to makeBusPortBits).
  LibertyPort *makePort(LibertyCell *cell,
                        std::string_view bit_name,
                        int bit_index);
  void makeBusPortBit(ConcreteLibrary *library,
                      LibertyCell *cell,
                      ConcretePort *bus_port,
                      std::string_view bus_name,
                      int index);
  TimingArc *makeTimingArc(TimingArcSet *set,
                           const Transition *from_rf,
                           const Transition *to_rf,
                           TimingModel *model);
  TimingArc *makeTimingArc(TimingArcSet *set,
                           const RiseFall *from_rf,
                           const RiseFall *to_rf,
                           TimingModel *model);
  TimingArcSet *makeLatchDtoQArcs(LibertyCell *cell,
                                  LibertyPort *from_port,
                                  LibertyPort *to_port,
                                  TimingSense sense,
                                  const TimingArcAttrsPtr &attrs);
  TimingArcSet *makeRegLatchArcs(LibertyCell *cell,
                                 LibertyPort *from_port,
                                 LibertyPort *to_port,
                                 const RiseFall *from_rf,
                                 const TimingArcAttrsPtr &attrs);
  TimingArcSet *makePresetClrArcs(LibertyCell *cell,
                                  LibertyPort *from_port,
                                  LibertyPort *to_port,
                                  const RiseFall *to_rf,
                                  const TimingArcAttrsPtr &attrs);
  TimingArcSet *makeTristateEnableArcs(LibertyCell *cell,
                                       LibertyPort *from_port,
                                       LibertyPort *to_port,
                                       bool to_rise,
                                       bool to_fall,
                                       const TimingArcAttrsPtr &attrs);
  TimingArcSet *makeTristateDisableArcs(LibertyCell *cell,
                                        LibertyPort *from_port,
                                        LibertyPort *to_port,
                                        bool to_rise,
                                        bool to_fall,
                                        const TimingArcAttrsPtr &attrs);

  Debug *debug_;
  Report *report_;
};

} // namespace sta
