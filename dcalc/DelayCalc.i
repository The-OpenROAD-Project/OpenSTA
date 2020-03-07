%module dcalc

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

%}

%inline %{

TmpStringSeq *
delay_calc_names()
{
  return sta::delayCalcNames();
}

bool
is_delay_calc_name(const char *alg)
{
  return sta::isDelayCalcName(alg);
}

void
set_delay_calculator_cmd(const char *alg)
{
  sta::Sta::sta()->setArcDelayCalc(alg);
}

void
set_delay_calc_incremental_tolerance(float tol)
{
  sta::Sta::sta()->setIncrementalDelayTolerance(tol);
}

%} // inline
