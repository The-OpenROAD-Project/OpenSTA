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

namespace sta {

// Constraints::pinNetCap return values.
class NetCaps
{
public:
  NetCaps();
  NetCaps(float pin_cap,
	  float wire_cap,
	  float fanout,
	  bool has_set_load);
  void init(float pin_cap,
	    float wire_cap,
	    float fanout,
	    bool has_set_load);
  float pinCap() const { return pin_cap_; }
  float wireCap() const{ return wire_cap_; }
  float fanout() const{ return fanout_; }
  bool hasSetLoad() const { return has_set_load_; }

private:
  DISALLOW_COPY_AND_ASSIGN(NetCaps);

  float pin_cap_;
  float wire_cap_;
  float fanout_;
  bool has_set_load_;
};

} // namespace
