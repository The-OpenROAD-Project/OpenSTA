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

%module properties

%{

#include "Property.hh"
#include "Sta.hh"

using namespace sta;

%}

%inline %{

PropertyValue
library_property(const Library *lib,
		 const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(lib, property);
}

PropertyValue
liberty_library_property(const LibertyLibrary *lib,
			 const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(lib, property);
}

PropertyValue
cell_property(const Cell *cell,
	      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(cell, property);
}

PropertyValue
liberty_cell_property(const LibertyCell *cell,
		      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(cell, property);
}

PropertyValue
port_property(const Port *port,
	      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(port, property);
}

PropertyValue
liberty_port_property(const LibertyPort *port,
		      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(port, property);
}

PropertyValue
instance_property(const Instance *inst,
		  const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(inst, property);
}

PropertyValue
pin_property(const Pin *pin,
	     const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(pin, property);
}

PropertyValue
net_property(const Net *net,
	     const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(net, property);
}

PropertyValue
edge_property(Edge *edge,
	      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(edge, property);
}

PropertyValue
clock_property(Clock *clk,
	       const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(clk, property);
}

PropertyValue
path_end_property(PathEnd *end,
		  const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(end, property);
}

PropertyValue
path_property(Path *path,
              const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(path, property);
}

PropertyValue
timing_arc_property(TimingArcSet *arc_set,
                    const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(arc_set, property);
}

%} // inline
