// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

%module power

%{
#include "Sta.hh"
#include "power/Power.hh"
#include "power/VcdReader.hh"
#include "power/ReadVcdActivities.hh"
#include "power/SaifReader.hh"

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
  Sta *sta = Sta::sta();
  PowerResult power = sta->power(inst, corner);
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
  Power *power = Sta::sta()->power();
  power->setGlobalActivity(activity, duty);
}

void
set_power_input_activity(float activity,
			 float duty)
{
  Power *power = Sta::sta()->power();
  return power->setInputActivity(activity, duty);
}

void
set_power_input_port_activity(const Port *input_port,
			      float activity,
			      float duty)
{
  Power *power = Sta::sta()->power();
  return power->setInputPortActivity(input_port, activity, duty);
}

void
set_power_pin_activity(const Pin *pin,
		       float activity,
		       float duty)
{
  Power *power = Sta::sta()->power();
  return power->setUserActivity(pin, activity, duty, PwrActivityOrigin::user);
}

////////////////////////////////////////////////////////////////

void
read_vcd_file(const char *filename,
              const char *scope)
{
  Sta *sta = Sta::sta();
  cmdLinkedNetwork();
  readVcdActivities(filename, scope, sta);
}

void
report_vcd_waveforms(const char *filename)
{
  Sta *sta = Sta::sta();
  reportVcdWaveforms(filename, sta);
}

// debugging
void
report_vcd_var_values(const char *filename,
                      const char *var_name)
{
  Sta *sta = Sta::sta();
  reportVcdVarValues(filename, var_name, sta);
}

////////////////////////////////////////////////////////////////

bool
read_saif_file(const char *filename,
               const char *scope)
{
  Sta *sta = Sta::sta();
  cmdLinkedNetwork();
  return readSaif(filename, scope, sta);
}

%} // inline
