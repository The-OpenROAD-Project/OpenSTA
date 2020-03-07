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

#include "Machine.hh"
#include "Units.hh"
#include "Liberty.hh"
#include "LinearModel.hh"

namespace sta {

GateLinearModel::GateLinearModel(float intrinsic,
				 float resistance) :
  intrinsic_(intrinsic),
  resistance_(resistance)
{
}

void
GateLinearModel::gateDelay(const LibertyCell *,
			   const Pvt *,
			   float,
			   float load_cap,
			   float,
			   bool,
			   // return values
			   ArcDelay &gate_delay,
			   Slew &drvr_slew) const
{
  gate_delay = intrinsic_ + resistance_ * load_cap;
  drvr_slew = 0.0;
}

void
GateLinearModel::reportGateDelay(const LibertyCell *cell,
				 const Pvt *,
				 float,
				 float load_cap,
				 float,
				 bool,
				 int digits,
				 string *result) const
{
  const LibertyLibrary *library = cell->libertyLibrary();
  const Units *units = library->units();
  const Unit *time_unit = units->timeUnit();
  const Unit *res_unit = units->resistanceUnit();
  const Unit *cap_unit = units->capacitanceUnit();
  *result += "Delay = ";
  *result += time_unit->asString(intrinsic_, digits);
  *result += " + ";
  *result += res_unit->asString(resistance_, digits);
  *result += " * ";
  *result += cap_unit->asString(load_cap, digits);
  *result += " = ";
  float delay = intrinsic_ + resistance_ * load_cap;
  *result += time_unit->asString(delay, digits);
}

float
GateLinearModel::driveResistance(const LibertyCell *,
				 const Pvt *) const
{
  return resistance_;
}

void
GateLinearModel::setIsScaled(bool)
{
}

CheckLinearModel::CheckLinearModel(float intrinsic) :
  intrinsic_(intrinsic)
{
}

void
CheckLinearModel::checkDelay(const LibertyCell *,
			     const Pvt *,
			     float,
			     float,
			     float,
			     bool,
			     ArcDelay &margin) const
{
  margin = intrinsic_;
}

void
CheckLinearModel::reportCheckDelay(const LibertyCell *cell,
				   const Pvt *,
				   float,
				   const char *,
				   float,
				   float,
				   bool,
				   int digits,
				   string *result) const
{
  const LibertyLibrary *library = cell->libertyLibrary();
  const Units *units = library->units();
  const Unit *time_unit = units->timeUnit();
  *result += "Check = ";
  *result += time_unit->asString(intrinsic_, digits);
}

void
CheckLinearModel::setIsScaled(bool)
{
}

} // namespace
