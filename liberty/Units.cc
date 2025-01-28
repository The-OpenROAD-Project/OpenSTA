// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "Units.hh"

#include <cmath> // abs

#include "StringUtil.hh"
#include "MinMax.hh"  // INF
#include "Fuzzy.hh"

namespace sta {

using std::abs;


Unit::Unit(const char *suffix) :
  scale_(1.0),
  suffix_(suffix),
  digits_(3)
{
  setScaledSuffix();
}

Unit::Unit(float scale,
	   const char *suffix,
	   int digits) :
  scale_(scale),
  suffix_(suffix),
  digits_(digits)
{
  setScaledSuffix();
}

void
Unit::setScaledSuffix()
{
  scaled_suffix_ = scaleAbbreviation() + suffix_;
}

void
Unit::operator=(const Unit &unit)
{
  scale_ = unit.scale_;
  suffix_ = unit.suffix_;
  scaled_suffix_ = unit.scaled_suffix_;
  digits_ = unit.digits_;
}

double
Unit::staToUser(double value)
{
  return value / scale_;
}

double
Unit::userToSta(double value)
{
  return value * scale_;
}

void
Unit::setScale(float scale)
{
  scale_ = scale;
  setScaledSuffix();
}

const char *
Unit::scaleAbbreviation() const
{
  if (fuzzyEqual(scale_, 1E+6))
    return "M";
  else if (fuzzyEqual(scale_, 1E+3))
    return "k";
  if (fuzzyEqual(scale_, 1.0))
    return "";
  else if (fuzzyEqual(scale_, 1E-3))
    return "m";
  else if (fuzzyEqual(scale_, 1E-6))
    return "u";
  else if (fuzzyEqual(scale_, 1E-9))
    return "n";
  else if (fuzzyEqual(scale_, 1E-12))
    return "p";
  else if (fuzzyEqual(scale_, 1E-15))
    return "f";
  else
    return "?";
}

void
Unit::setSuffix(const char *suffix)
{
  suffix_ = suffix;
  setScaledSuffix();
}

void
Unit::setDigits(int digits)
{
  digits_ = digits;
}

int
Unit::width() const
{
  return digits_ + 2;
}

const char *
Unit::asString(float value) const
{
  return asString(value, digits_);
}

const char *
Unit::asString(double value) const
{
  return asString(static_cast<float>(value), digits_);
}

const char *
Unit::asString(float value,
	       int digits) const
{
  // Special case INF because it blows up otherwise.
  if (abs(value) >= INF * .1)
    return (value > 0.0) ? "INF" : "-INF";
  else {
    float scaled_value = value / scale_;
    // prevent "-0.00" on slowaris
    if (abs(scaled_value) < 1E-6)
      scaled_value = 0.0;
    return stringPrintTmp("%.*f", digits, scaled_value);
  }
}

////////////////////////////////////////////////////////////////

Units::Units() :
  time_unit_("s"),
  resistance_unit_("ohm"),
  capacitance_unit_("F"),
  voltage_unit_("v"),
  current_unit_("A"),
  power_unit_("W"),
  distance_unit_("m"),
  scalar_unit_("")
{
}

Unit *
Units::find(const char *unit_name)
{
  if (stringEq(unit_name, "time"))
    return &time_unit_;
  else if (stringEq(unit_name, "resistance"))
    return &resistance_unit_;
  else if (stringEq(unit_name, "capacitance"))
    return &capacitance_unit_;
  else if (stringEq(unit_name, "voltage"))
    return &voltage_unit_;
  else if (stringEq(unit_name, "current"))
    return &current_unit_;
  else if (stringEq(unit_name, "power"))
    return &power_unit_;
  else if (stringEq(unit_name, "distance"))
    return &distance_unit_;
  else
    return nullptr;
}

void
Units::operator=(const Units &units)
{
  time_unit_ = *units.timeUnit();
  resistance_unit_ = *units.resistanceUnit();
  capacitance_unit_ = *units.capacitanceUnit();
  voltage_unit_ = *units.voltageUnit();
  current_unit_ = *units.currentUnit();
  power_unit_ = *units.powerUnit();
  distance_unit_ = *units.distanceUnit();
  scalar_unit_ = *units.scalarUnit();
}

} // namespace
