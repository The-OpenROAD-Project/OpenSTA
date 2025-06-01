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

void
set_library_property(const Library *lib,
                     const char *property,
                     PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(lib, property, value);
}

PropertyValue
liberty_library_property(const LibertyLibrary *lib,
			 const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(lib, property);
}

void
set_liberty_library_property(const LibertyLibrary *lib,
                             const char *property,
                             PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(lib, property, value);
}

PropertyValue
cell_property(const Cell *cell,
	      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(cell, property);
}

void
set_cell_property(const Cell *cell,
                  const char *property,
                  PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(cell, property, value);
}

PropertyValue
liberty_cell_property(const LibertyCell *cell,
		      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(cell, property);
}

void
set_liberty_cell_property(const LibertyCell *cell,
                          const char *property,
                          PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(cell, property, value);
}

PropertyValue
port_property(const Port *port,
	      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(port, property);
}

void
set_port_property(const Port *port,
                  const char *property,
                  PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(port, property, value);
}

PropertyValue
liberty_port_property(const LibertyPort *port,
		      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(port, property);
}

void
set_liberty_port_property(const LibertyPort *port,
                          const char *property,
                          PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(port, property, value);
}

PropertyValue
instance_property(const Instance *inst,
		  const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(inst, property);
}

void
set_instance_property(const Instance *inst,
                      const char *property,
                      PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(inst, property, value);
}

PropertyValue
pin_property(const Pin *pin,
	     const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(pin, property);
}

void
set_pin_property(const Pin *pin,
                 const char *property,
                 PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(pin, property, value);
}

PropertyValue
net_property(const Net *net,
	     const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(net, property);
}

void
set_net_property(const Net *net,
                 const char *property,
                 PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(net, property, value);
}

PropertyValue
edge_property(Edge *edge,
	      const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(edge, property);
}

void
set_edge_property(Edge *edge,
                  const char *property,
                  PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(edge, property, value);
}

PropertyValue
clock_property(Clock *clk,
	       const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(clk, property);
}

void
set_clock_property(const Clock *clk,
                   const char *property,
                   PropertyValue value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(clk, property, value);
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
timing_arc_set_property(TimingArcSet *arc_set,
			const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(arc_set, property);
}

%} // inline
