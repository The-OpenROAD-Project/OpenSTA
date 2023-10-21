%module power

%{

// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include "Sta.hh"
#include "power/Power.hh"
#include "power/VcdReader.hh"
#include "power/ReadVcdActivities.hh"

using namespace sta;

%}

%inline %{

static void
pushPowerResultFloats(PowerResult &power,
		      FloatSeq &powers)
{
  powers.push_back(power.internal());
  powers.push_back(power.switching());
  powers.push_back(power.leakage());
  powers.push_back(power.total());
}

FloatSeq
design_power(const Corner *corner)
{
  cmdLinkedNetwork();
  PowerResult total, sequential, combinational, clock, macro, pad;
  Sta::sta()->power(corner, total, sequential, combinational, clock, macro, pad);
  FloatSeq powers;
  pushPowerResultFloats(total, powers);
  pushPowerResultFloats(sequential, powers);
  pushPowerResultFloats(combinational, powers);
  pushPowerResultFloats(clock, powers);
  pushPowerResultFloats(macro, powers);
  pushPowerResultFloats(pad, powers);
  return powers;
}

FloatSeq
instance_power(Instance *inst,
	       const Corner *corner)
{
  cmdLinkedNetwork();
  PowerResult power = Sta::sta()->power(inst, corner);
  FloatSeq powers;
  powers.push_back(power.internal());
  powers.push_back(power.switching());
  powers.push_back(power.leakage());
  powers.push_back(power.total());
  return powers;
}

void
set_power_global_activity(float activity,
			  float duty)
{
  Sta::sta()->power()->setGlobalActivity(activity, duty);
}

void
set_power_input_activity(float activity,
			 float duty)
{
  return Sta::sta()->power()->setInputActivity(activity, duty);
}

void
set_power_input_port_activity(const Port *input_port,
			      float activity,
			      float duty)
{
  return Sta::sta()->power()->setInputPortActivity(input_port, activity, duty);
}

void
set_power_pin_activity(const Pin *pin,
		       float activity,
		       float duty)
{
  return Sta::sta()->power()->setUserActivity(pin, activity, duty,
					      PwrActivityOrigin::user);
}

void
read_vcd_activities(const char *filename,
                    const char *scope)
{
  readVcdActivities(filename, scope, Sta::sta());
}

void
report_vcd_waveforms(const char *filename)
{
  reportVcdWaveforms(filename, Sta::sta());
}

// debugging
void
report_vcd_var_values(const char *filename,
                      const char *var_name)
{
  reportVcdVarValues(filename, var_name, Sta::sta());
}

%} // inline
