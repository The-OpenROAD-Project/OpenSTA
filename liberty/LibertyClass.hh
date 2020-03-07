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

#include "Vector.hh"
#include "Map.hh"
#include "Set.hh"

namespace sta {

class Units;
class Unit;
class LibertyLibrary;
class LibertyCell;
class LibertyPort;
class Pvt;
class OperatingConditions;
class BusDcl;
class ModeDef;
class ModeValueDef;
class TestCell;
class TableTemplate;
class Table;
class TableModel;
class TableAxis;
class GateTimingModel;
class CheckTimingModel;
class ScaleFactors;
class Group;
class Wireload;
class WireloadSelection;
class TimingArcSet;
class TimingArc;
class InternalPower;
class LeakagePower;
class Sequential;
class FuncExpr;
class TimingModel;
class TimingRole;
class Transition;
class RiseFall;
class RiseFallBoth;
class LibertyCellSequentialIterator;

typedef Vector<LibertyLibrary*> LibertyLibrarySeq;
typedef Vector<LibertyCell*> LibertyCellSeq;
typedef Vector<Sequential*> SequentialSeq;
typedef Map<LibertyCell*, LibertyCellSeq*> LibertyCellEquivMap;
typedef Vector<LibertyPort*> LibertyPortSeq;
typedef Set<LibertyPort*> LibertyPortSet;
typedef std::pair<const LibertyPort*,const LibertyPort*> LibertyPortPair;
typedef Set<LibertyCell*> LibertyCellSet;
typedef Vector<float> FloatSeq;
typedef Vector<FloatSeq*> FloatTable;

enum class ScaleFactorType : unsigned {
  pin_cap,
  wire_cap,
  wire_res,
  min_period,
  // Liberty attributes have rise/fall suffix.
  cell,
  hold,
  setup,
  recovery,
  removal,
  nochange,
  skew,
  leakage_power,
  internal_power,
  // Liberty attributes have rise/fall prefix.
  transition,
  // Liberty attributes have low/high suffix (indexed as rise/fall).
  min_pulse_width,
  unknown,
};
const int scale_factor_type_count = int(ScaleFactorType::unknown) + 1;
// Enough bits to hold a ScaleFactorType enum.
const int scale_factor_bits = 4;

enum class WireloadTree { worst_case, best_case, balanced, unknown };

enum class WireloadMode { top, enclosed, segmented, unknown };

enum class TimingSense {
  positive_unate,
  negative_unate,
  non_unate,
  none,
  unknown
};
const int timing_sense_count = int(TimingSense::unknown) + 1;
const int timing_sense_bit_count = 3;

enum class TableAxisVariable {
  total_output_net_capacitance,
  equal_or_opposite_output_net_capacitance,
  input_net_transition,
  input_transition_time,
  related_pin_transition,
  constrained_pin_transition,
  output_pin_transition,
  connect_delay,
  related_out_total_output_net_capacitance,
  time,
  iv_output_voltage,
  input_noise_width,
  input_noise_height,
  input_voltage,
  output_voltage,
  path_depth,
  path_distance,
  normalized_voltage,
  unknown
};

enum class PathType { clk, data, clk_and_data };
const int path_type_count = 2;

// Rise/fall to rise/fall.
const int timing_arc_index_bit_count = 2;
const int timing_arc_index_max = (1<<timing_arc_index_bit_count)-1;
const int timing_arc_set_index_bit_count = 18;
const int timing_arc_set_index_max=(1<<timing_arc_set_index_bit_count)-1;

class LibertyPortNameLess
{
public:
  bool operator()(const LibertyPort *port1, const LibertyPort *port2) const;
};

class LibertyPortPairLess
{
public:
  bool operator()(const LibertyPortPair *pair1,
		  const LibertyPortPair *pair2) const;
  bool operator()(const LibertyPortPair &pair1,
		  const LibertyPortPair &pair2) const;
};

bool
timingArcSetLess(const TimingArcSet *set1,
		 const TimingArcSet *set2);

class TimingArcSetLess
{
public:
  bool
  operator()(const TimingArcSet *set1,
	     const TimingArcSet *set2) const
  {
    return timingArcSetLess(set1, set2);
  }
};

} // namespace
