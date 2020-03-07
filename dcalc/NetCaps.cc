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
#include "NetCaps.hh"

namespace sta {

NetCaps::NetCaps()
{
}

NetCaps::NetCaps(float pin_cap,
		 float wire_cap,
		 float fanout,
		 bool has_set_load) :
  pin_cap_(pin_cap),
  wire_cap_(wire_cap),
  fanout_(fanout),
  has_set_load_(has_set_load)
{
}

void
NetCaps::init(float pin_cap,
	      float wire_cap,
	      float fanout,
	      bool has_set_load)
{
  pin_cap_ = pin_cap;
  wire_cap_ = wire_cap;
  fanout_ = fanout;
  has_set_load_ = has_set_load;
}

} // namespace
