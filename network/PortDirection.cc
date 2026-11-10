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

#include "PortDirection.hh"

#include "StringUtil.hh"

namespace sta {

const PortDirection PortDirection::input_("input", 0);
const PortDirection PortDirection::output_("output", 1);
const PortDirection PortDirection::tristate_("tristate", 2);
const PortDirection PortDirection::bidirect_("bidirect", 3);
const PortDirection PortDirection::internal_("internal", 4);
const PortDirection PortDirection::ground_("ground", 5);
const PortDirection PortDirection::power_("power", 6);
const PortDirection PortDirection::well_("well", 7);
const PortDirection PortDirection::unknown_("unknown", 8);

const PortDirection *
PortDirection::find(const char *dir_name)
{
  if (stringEqual(dir_name, "input"))
    return &input_;
  else if (stringEqual(dir_name, "output"))
    return &output_;
  else if (stringEqual(dir_name, "tristate"))
    return &tristate_;
  else if (stringEqual(dir_name, "bidirect"))
    return &bidirect_;
  else if (stringEqual(dir_name, "internal"))
    return &internal_;
  else if (stringEqual(dir_name, "ground"))
    return &ground_;
  else if (stringEqual(dir_name, "power"))
    return &power_;
  else if (stringEqual(dir_name, "well"))
    return &well_;
  else
    return nullptr;
}

bool
PortDirection::isAnyInput() const
{
  return this == &input_
    || this == &bidirect_;
}

bool
PortDirection::isAnyOutput() const
{
  return this == &output_
    || this == &tristate_
    || this == &bidirect_;
}

bool
PortDirection::isAnyTristate() const
{
  return this == &tristate_
    || this == &bidirect_;
}

bool
PortDirection::isPowerGround() const
{
  return this == &ground_
    || this == &power_
    || this == &well_;
}

} // namespace sta
