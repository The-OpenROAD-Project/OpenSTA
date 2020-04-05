%module parasitics

%{

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

#include "Sta.hh"

using sta::Sta;
using sta::cmdLinkedNetwork;
using sta::Instance;
using sta::MinMaxAll;
using sta::ReduceParasiticsTo;
using sta::RiseFall;
using sta::Pin;
using sta::TmpFloatSeq;

%}

%inline %{

bool
read_spef_cmd(const char *filename,
	      Instance *instance,
	      MinMaxAll *min_max,
	      bool increment,
	      bool pin_cap_included,
	      bool keep_coupling_caps,
	      float coupling_cap_factor,
	      ReduceParasiticsTo reduce_to,
	      bool delete_after_reduce,
	      bool quiet,
	      bool save)
{
  cmdLinkedNetwork();
  return Sta::sta()->readSpef(filename, instance, min_max,
			      increment, pin_cap_included,
			      keep_coupling_caps, coupling_cap_factor,
			      reduce_to, delete_after_reduce,
			      save, quiet);
}

TmpFloatSeq *
find_pi_elmore(Pin *drvr_pin,
	       RiseFall *rf,
	       MinMax *min_max)
{
  float c2, rpi, c1;
  bool exists;
  Sta::sta()->findPiElmore(drvr_pin, rf, min_max, c2, rpi, c1, exists);
  TmpFloatSeq *floats = new FloatSeq;
  if (exists) {
    floats->push_back(c2);
    floats->push_back(rpi);
    floats->push_back(c1);
  }
  return floats;
}

float
find_elmore(Pin *drvr_pin,
	    Pin *load_pin,
	    RiseFall *rf,
	    MinMax *min_max)
{
  float elmore = 0.0;
  bool exists;
  Sta::sta()->findElmore(drvr_pin, load_pin, rf, min_max, elmore, exists);
  return elmore;
}

void
set_pi_model_cmd(Pin *drvr_pin,
		 RiseFall *rf,
		 MinMaxAll *min_max,
		 float c2,
		 float rpi,
		 float c1)
{
  Sta::sta()->makePiElmore(drvr_pin, rf, min_max, c2, rpi, c1);
}

void
set_elmore_cmd(Pin *drvr_pin,
	       Pin *load_pin,
	       RiseFall *rf,
	       MinMaxAll *min_max,
	       float elmore)
{
  Sta::sta()->setElmore(drvr_pin, load_pin, rf, min_max, elmore);
}

%} // inline
