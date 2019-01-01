// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#ifndef STA_LIBERTY_CLASS_H
#define STA_LIBERTY_CLASS_H

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
class TransRiseFall;
class TransRiseFallBoth;
class LibertyCellSequentialIterator;

typedef Vector<LibertyCell*> LibertyCellSeq;
typedef Vector<Sequential*> SequentialSeq;
typedef Map<LibertyCell*, LibertyCellSeq*> LibertyCellEquivMap;
typedef Vector<LibertyPort*> LibertyPortSeq;
typedef Set<LibertyPort*> LibertyPortSet;
typedef std::pair<const LibertyPort*,const LibertyPort*> LibertyPortPair;
typedef Set<LibertyCell*> LibertyCellSet;
typedef Vector<float> FloatSeq;
typedef Vector<FloatSeq*> FloatTable;

typedef enum {
  scale_factor_pin_cap,
  scale_factor_wire_cap,
  scale_factor_wire_res,
  scale_factor_min_period,
  // Liberty attributes have rise/fall suffix.
  scale_factor_cell,
  scale_factor_hold,
  scale_factor_setup,
  scale_factor_recovery,
  scale_factor_removal,
  scale_factor_nochange,
  scale_factor_skew,
  scale_factor_leakage_power,
  scale_factor_internal_power,
  // Liberty attributes have rise/fall prefix.
  scale_factor_transition,
  // Liberty attributes have low/high suffix (indexed as rise/fall).
  scale_factor_min_pulse_width,
  scale_factor_unknown,
  scale_factor_count
} ScaleFactorType;

// Enough bits to hold a ScaleFactorType enum.
const int scale_factor_bits = 4;

typedef enum {
  wire_load_worst_case_tree,
  wire_load_best_case_tree,
  wire_load_balanced_tree,
  wire_load_unknown_tree
} WireloadTree;

typedef enum {
  wire_load_mode_top,
  wire_load_mode_enclosed,
  wire_load_mode_segmented,
  wire_load_mode_unknown
} WireloadMode;

typedef enum {
  timing_sense_positive_unate,
  timing_sense_negative_unate,
  timing_sense_non_unate,
  timing_sense_none,
  timing_sense_unknown
} TimingSense;

typedef enum {
  table_axis_total_output_net_capacitance,
  table_axis_equal_or_opposite_output_net_capacitance,
  table_axis_input_net_transition,
  table_axis_input_transition_time,
  table_axis_related_pin_transition,
  table_axis_constrained_pin_transition,
  table_axis_output_pin_transition,
  table_axis_connect_delay,
  table_axis_related_out_total_output_net_capacitance,
  table_axis_time,
  table_axis_iv_output_voltage,
  table_axis_input_noise_width,
  table_axis_input_noise_height,
  table_axis_input_voltage,
  table_axis_output_voltage,
  table_axis_path_depth,
  table_axis_path_distance,
  table_axis_normalized_voltage,
  table_axis_unknown
} TableAxisVariable;

typedef enum {
  path_type_clk,
  path_type_data,
  path_type_clk_and_data
} PathType;

const int path_type_count = 2;

const int timing_sense_count = timing_sense_unknown + 1;
const int timing_sense_bit_count = 3;
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
#endif
