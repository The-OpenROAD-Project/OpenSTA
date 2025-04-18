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

#pragma once

#include "NetworkClass.hh"

namespace sta {

class PortDirection
{
public:
  static void init();
  static void destroy();
  // Singleton accessors.
  static PortDirection *input() { return input_; }
  static PortDirection *output() { return output_; }
  static PortDirection *tristate() { return tristate_; }
  static PortDirection *bidirect() { return bidirect_; }
  static PortDirection *internal() { return internal_; }
  static PortDirection *ground() { return ground_; }
  static PortDirection *power() { return power_; }
  static PortDirection *unknown() { return unknown_; }
  static PortDirection *find(const char *dir_name);
  const char *name() const { return name_; }
  int index() const { return index_; }
  bool isInput() const { return this == input_; }
  // Input or bidirect.
  bool isAnyInput() const;
  bool isOutput() const { return this == output_; }
  // Output, tristate or bidirect.
  bool isAnyOutput() const;
  bool isTristate() const { return this == tristate_; }
  bool isBidirect() const { return this == bidirect_; }
  // Bidirect or tristate.
  bool isAnyTristate() const;
  bool isGround() const { return this == ground_; }
  bool isPower() const { return this == power_; }
  // Ground or power.
  bool isPowerGround() const;
  bool isInternal() const { return this == internal_; }
  bool isUnknown() const { return this == unknown_; }

private:
  PortDirection(const char *name,
		int index);

  const char *name_;
  int index_;

  static PortDirection *input_;
  static PortDirection *output_;
  static PortDirection *tristate_;
  static PortDirection *bidirect_;
  static PortDirection *internal_;
  static PortDirection *ground_;
  static PortDirection *power_;
  static PortDirection *unknown_;
};

} // namespace
