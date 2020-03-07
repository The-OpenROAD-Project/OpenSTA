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
#include "Vector.hh"
#include "LibertyClass.hh"

namespace sta {

class WireloadForArea;

typedef std::pair<float,float> FanoutLength;
typedef Vector<FanoutLength*> FanoutLengthSeq;
typedef Vector<WireloadForArea*> WireloadForAreaSeq;

const char *
wireloadTreeString(WireloadTree tree);
WireloadTree
stringWireloadTree(const char *tree);

const char *
wireloadModeString(WireloadMode wire_load_mode);
WireloadMode
stringWireloadMode(const char *wire_load_mode);

class Wireload
{
public:
  Wireload(const char *name,
	   LibertyLibrary *library);
  Wireload(const char *name,
	   LibertyLibrary *library,
	   float area,
	   float resistance,
	   float capacitance,
	   float slope);
  virtual ~Wireload();
  const char *name() const { return name_; }
  void setArea(float area);
  void setResistance(float res);
  void setCapacitance(float cap);
  void setSlope(float slope);
  void addFanoutLength(float fanout,
		       float length);
  // Find wireload resistance/capacitance for fanout.
  virtual void findWireload(float fanout,
			    const OperatingConditions *op_cond,
			    float &cap,
			    float &res) const;

protected:
  const char *name_;
  LibertyLibrary *library_;
  float area_;
  float resistance_;
  float capacitance_;
  // Fanout length extrapolation slope.
  float slope_;
  FanoutLengthSeq fanout_lengths_;

private:
  DISALLOW_COPY_AND_ASSIGN(Wireload);
};

class WireloadSelection
{
public:
  explicit WireloadSelection(const char *name);
  ~WireloadSelection();
  const char *name() const { return name_; }
  void addWireloadFromArea(float min_area,
			   float max_area,
			   const Wireload *wireload);
  const Wireload *findWireload(float area) const;

private:
  DISALLOW_COPY_AND_ASSIGN(WireloadSelection);

  const char *name_;
  WireloadForAreaSeq wireloads_;
};

} // namespace
