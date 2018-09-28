// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
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
#include "PortDirection.hh"

namespace sta {

PortDirection *PortDirection::input_;
PortDirection *PortDirection::output_;
PortDirection *PortDirection::tristate_;
PortDirection *PortDirection::bidirect_;
PortDirection *PortDirection::internal_;
PortDirection *PortDirection::ground_;
PortDirection *PortDirection::power_;
PortDirection *PortDirection::unknown_;

void
PortDirection::init()
{
  input_ = new PortDirection("input", 0);
  output_ = new PortDirection("output", 1);
  tristate_ = new PortDirection("tristate", 2);
  bidirect_ = new PortDirection("bidirect", 3);
  internal_ = new PortDirection("internal", 4);
  ground_ = new PortDirection("ground", 5);
  power_ = new PortDirection("power", 6);
  unknown_ = new PortDirection("unknown", 7);
}

void
PortDirection::destroy()
{
  delete input_;
  input_ = NULL;
  delete output_;
  output_ = NULL;
  delete tristate_;
  tristate_ = NULL;
  delete bidirect_;
  bidirect_ = NULL;
  delete internal_;
  internal_ = NULL;
  delete ground_;
  ground_ = NULL;
  delete power_;
  power_ = NULL;
  delete unknown_;
  unknown_ = NULL;
}

PortDirection::PortDirection(const char *name,
			     int index) :
  name_(name),
  index_(index)
{
}

bool
PortDirection::isAnyInput() const
{
  return this == input_
    || this == bidirect_;
}

bool
PortDirection::isAnyOutput() const
{
  return this == output_
    || this == tristate_
    || this == bidirect_;
}

bool
PortDirection::isAnyTristate() const
{
  return this == tristate_
    || this == bidirect_;
}

bool
PortDirection::isPowerGround() const
{
  return this == ground_
    || this == power_;
}

} // namespace
