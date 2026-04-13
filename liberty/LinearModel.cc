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

#include "LinearModel.hh"

#include <string>
#include <string_view>

#include "Delay.hh"
#include "LibertyClass.hh"
#include "MinMax.hh"
#include "PocvMode.hh"
#include "TimingModel.hh"
#include "Units.hh"
#include "Liberty.hh"

namespace sta {

GateLinearModel::GateLinearModel(LibertyCell *cell,
                                 float intrinsic,
                                 float resistance) :
  GateTimingModel(cell),
  intrinsic_(intrinsic),
  resistance_(resistance)
{
}

void
GateLinearModel::gateDelay(const Pvt *,
                           float,
                           float load_cap,
                           // return values
                           float &gate_delay,
                           float &drvr_slew) const
{
  gate_delay = intrinsic_ + resistance_ * load_cap;
  drvr_slew = 0.0;
}

void
GateLinearModel::gateDelayPocv(const Pvt *,
                               float,
                               float,
                               const MinMax *,
                               PocvMode,
                               // return values
                               ArcDelay &,
                               Slew &) const
{
}

std::string
GateLinearModel::reportGateDelay(const Pvt *,
                                 float,
                                 float load_cap,
                                 const MinMax *,
                                 PocvMode,
                                 int digits) const
{
  const LibertyLibrary *library = cell_->libertyLibrary();
  const Units *units = library->units();
  const Unit *time_unit = units->timeUnit();
  const Unit *res_unit = units->resistanceUnit();
  const Unit *cap_unit = units->capacitanceUnit();
  std::string result = "Delay = ";
  result += time_unit->asString(intrinsic_, digits);
  result += " + ";
  result += res_unit->asString(resistance_, digits);
  result += " * ";
  result += cap_unit->asString(load_cap, digits);
  result += " = ";
  float delay = intrinsic_ + resistance_ * load_cap;
  result += time_unit->asString(delay, digits);
  return result;
}

float
GateLinearModel::driveResistance(const Pvt *) const
{
  return resistance_;
}

void
GateLinearModel::setIsScaled(bool)
{
}

CheckLinearModel::CheckLinearModel(LibertyCell *cell,
                                   float intrinsic) :
  CheckTimingModel(cell),
  intrinsic_(intrinsic)
{
}

ArcDelay
CheckLinearModel::checkDelay(const Pvt *,
                             float,
                             float,
                             float,
                             const MinMax *,
                             PocvMode) const
{
  return intrinsic_;
}

std::string
CheckLinearModel::reportCheckDelay(const Pvt *,
                                   float,
                                   std::string_view,
                                   float,
                                   float,
                                   const MinMax *,
                                   PocvMode,
                                   int digits) const
{
  const LibertyLibrary *library = cell_->libertyLibrary();
  const Units *units = library->units();
  const Unit *time_unit = units->timeUnit();
  std::string result = "Check = ";
  result += time_unit->asString(intrinsic_, digits);
  return result;
}

void
CheckLinearModel::setIsScaled(bool)
{
}

} // namespace sta
