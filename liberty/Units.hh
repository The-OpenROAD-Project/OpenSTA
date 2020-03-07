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

namespace sta {

class Unit
{
public:
  Unit();
  ~Unit();
  Unit(float scale,
       const char *suffix,
       int digits);
  void operator=(const Unit &unit);
  float scale() const { return scale_; }
  void setScale(float scale);
  const char *suffix() const { return suffix_; }
  void setSuffix(const char *suffix);
  int digits() const { return digits_; }
  void setDigits(int digits);
  int width() const;
  const char *asString(float value) const;
  const char *asString(double value) const;
  const char *asString(float value,
		       int digits) const;

private:
  float scale_;			// multiplier from user units to internal units
  const char *suffix_;		// print suffix
  int digits_;			// print digits (after decimal pt)
};

// User interface units.
// Sta internal units are always seconds, farads, volts, amps.
class Units
{
public:
  Units() {}
  Unit *find(const char *unit_name);
  void operator=(const Units &units);
  Unit *timeUnit() { return &time_unit_; }
  const Unit *timeUnit() const { return &time_unit_; }
  Unit *capacitanceUnit() { return &capacitance_unit_; }
  const Unit *capacitanceUnit() const { return &capacitance_unit_; }
  Unit *voltageUnit() { return &voltage_unit_; }
  const Unit *voltageUnit() const { return &voltage_unit_; }
  Unit *resistanceUnit() { return &resistance_unit_; }
  const Unit *resistanceUnit() const { return &resistance_unit_; }
  Unit *pullingResistanceUnit() { return &pulling_resistance_unit_; }
  const Unit *pullingResistanceUnit() const {return &pulling_resistance_unit_;}
  Unit *currentUnit() { return &current_unit_; }
  const Unit *currentUnit() const { return &current_unit_; }
  Unit *powerUnit() { return &power_unit_; }
  const Unit *powerUnit() const { return &power_unit_; }
  Unit *distanceUnit() { return &distance_unit_; }
  const Unit *distanceUnit() const { return &distance_unit_; }
  Unit *scalarUnit() { return &scalar_unit_; }
  const Unit *scalarUnit() const { return &scalar_unit_; }

private:
  Unit time_unit_;
  Unit capacitance_unit_;
  Unit voltage_unit_;
  Unit resistance_unit_;
  Unit pulling_resistance_unit_;
  Unit current_unit_;
  Unit power_unit_;
  Unit distance_unit_;
  Unit scalar_unit_;
};

} // namespace
