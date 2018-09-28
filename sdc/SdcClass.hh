// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
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

#ifndef STA_SDC_CLASS_H
#define STA_SDC_CLASS_H

#include "Map.hh"
#include "Set.hh"
#include "Vector.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "MinMaxValues.hh"
#include "PinPair.hh"

namespace sta {

class Sdc;
class Clock;
class ClockEdge;
class CycleAccting;
class CycleAcctingLess;
class InputDelay;
class OutputDelay;
class FalsePath;
class PathDelay;
class MultiCyclePath;
class FilterPath;
class GroupPath;
class ExceptionFromTo;
class ExceptionFrom;
class ExceptionThru;
class ExceptionTo;
class ExceptionPt;
class InputDrive;
class MinMax;
class MinMaxAll;
class RiseFallMinMax;
class DisabledInstancePorts;
class DisabledCellPorts;
class ExceptionPath;
class DataCheck;
class Wireload;
class ClockLatency;
class ClockInsertion;
class ClockGroup;
class ClockGroups;
class ClockVertexPinIterator;
class ClockPinIterator;

typedef enum {
  analysis_type_single,
  analysis_type_bc_wc,
  analysis_type_on_chip_variation
} AnalysisType;

typedef enum {
  exception_type_false,
  exception_type_loop,
  exception_type_multi_cycle,
  exception_type_path_delay,
  exception_type_group_path,
  exception_type_filter,
  exception_type_any
} ExceptionPathType;

typedef enum {
  clk_sense_positive,
  clk_sense_negative,
  clk_sense_stop
} ClockSense;

typedef std::pair<const Clock*, const Clock*> ClockPair;

typedef Vector<float> FloatSeq;
typedef Vector<int> IntSeq;
typedef Vector<Clock*> ClockSeq;
typedef Set<Clock*> ClockSet;
typedef ClockSeq::ConstIterator ClockIterator;
typedef Vector<PinSet*> PinSetSeq;
typedef MinMax SetupHold;
typedef MinMaxAll SetupHoldAll;
typedef Vector<ExceptionThru*> ExceptionThruSeq;
typedef Set<LibertyPortPair*, LibertyPortPairLess> LibertyPortPairSet;
typedef Map<Instance*, DisabledInstancePorts*> DisabledInstancePortsMap;
typedef Map<LibertyCell*, DisabledCellPorts*> DisabledCellPortsMap;
typedef MinMaxValues<float> ClockUncertainties;
typedef Set<ExceptionPath*> ExceptionPathSet;
typedef PinPair EdgePins;
typedef PinPairSet EdgePinsSet;
typedef Map<const Pin*, LogicValue> LogicValueMap;
typedef Set<ClockGroup*> ClockGroupSet;

// For Search.
class ExceptionState;
class ExceptionPath;
typedef Set<ExceptionState*> ExceptionStateSet;

typedef enum {
  crpr_mode_same_pin,
  crpr_mode_same_transition
} CrprMode;

// Constraint applies to clock or data paths.
typedef enum {
  path_clk,
  path_data
} PathClkOrData;

const int path_clk_or_data_count = 2;

typedef enum {
  timing_derate_cell_delay,
  timing_derate_cell_check,
  timing_derate_net_delay
} TimingDerateType;

const int timing_derate_type_count = 3;
const int timing_derate_cell_type_count = 2;

} // namespace
#endif
