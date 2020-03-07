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

#include <cmath> // abs
#include <limits>
#include "Machine.hh"
#include "StringUtil.hh"
#include "MinMax.hh"  // INF
#include "Units.hh"

namespace sta {

using std::abs;

Unit::Unit() :
  scale_(1.0),
  suffix_(stringCopy("")),
  digits_(4)
{
}

Unit::Unit(float scale,
	   const char *suffix,
	   int digits) :
  scale_(scale),
  suffix_(stringCopy(suffix)),
  digits_(digits)
{
}

Unit::~Unit()
{
  stringDelete(suffix_);
}

void
Unit::operator=(const Unit &unit)
{
  scale_ = unit.scale_;
  stringDelete(suffix_);
  suffix_ = stringCopy(unit.suffix_);
  digits_ = unit.digits_;
}

void
Unit::setScale(float scale)
{
  scale_ = scale;
}

void
Unit::setSuffix(const char *suffix)
{
  stringDelete(suffix_);
  suffix_ = stringCopy(suffix);
}

void
Unit::setDigits(int digits)
{
  digits_ = digits;
}

int
Unit::width() const
{
  return digits_ + (suffix_ ? strlen(suffix_) : 0) + 2;
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
    return stringPrintTmp("%.*f%s", digits, scaled_value, suffix_);
  }
}

////////////////////////////////////////////////////////////////

Unit *
Units::find(const char *unit_name)
{
  if (stringEq(unit_name, "time"))
    return &time_unit_;
  else if (stringEq(unit_name, "capacitance"))
    return &capacitance_unit_;
  else if (stringEq(unit_name, "resistance"))
    return &resistance_unit_;
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
  capacitance_unit_ = *units.capacitanceUnit();
  resistance_unit_ = *units.resistanceUnit();
  voltage_unit_ = *units.voltageUnit();
  current_unit_ = *units.currentUnit();
  power_unit_ = *units.powerUnit();
  distance_unit_ = *units.distanceUnit();
  scalar_unit_ = *units.scalarUnit();
}

} // namespace
