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

#pragma once

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
class ClockGroups;
class PortDelay;

enum class AnalysisType { single, bc_wc, ocv };

enum class ExceptionPathType { false_path, loop, multi_cycle, path_delay,
			       group_path, filter, any};

enum class ClockSense { positive, negative, stop };

typedef std::pair<const Clock*, const Clock*> ClockPair;

class ClockIndexLess
{
public:
  bool operator()(const Clock *clk1,
                  const Clock *clk2) const;
};

typedef Vector<float> FloatSeq;
typedef Vector<int> IntSeq;
typedef Vector<Clock*> ClockSeq;
typedef std::vector<const Clock*> ConstClockSeq;
typedef Set<Clock*, ClockIndexLess> ClockSet;
typedef std::set<const Clock*, ClockIndexLess> ConstClockSet;
typedef ClockSet ClockGroup;
typedef Vector<PinSet*> PinSetSeq;
typedef MinMax SetupHold;
typedef MinMaxAll SetupHoldAll;
typedef Vector<ExceptionThru*> ExceptionThruSeq;
typedef Set<LibertyPortPair, LibertyPortPairLess> LibertyPortPairSet;
typedef Map<const Instance*, DisabledInstancePorts*> DisabledInstancePortsMap;
typedef Map<LibertyCell*, DisabledCellPorts*> DisabledCellPortsMap;
typedef MinMaxValues<float> ClockUncertainties;
typedef Set<ExceptionPath*> ExceptionPathSet;
typedef PinPair EdgePins;
typedef PinPairSet EdgePinsSet;
typedef Map<const Pin*, LogicValue> LogicValueMap;

class ClockSetLess
{
public:
  bool operator()(const ClockSet *set1,
                  const ClockSet *set2) const;
};

typedef Set<ClockGroup*, ClockSetLess> ClockGroupSet;

// For Search.
class ExceptionState;

class ExceptionStateLess
{
public:
  bool operator()(const ExceptionState *state1,
                  const ExceptionState *state2) const;
};

class ExceptionPath;
typedef Set<ExceptionState*, ExceptionStateLess> ExceptionStateSet;

enum class CrprMode { same_pin, same_transition };

// Constraint applies to clock or data paths.
enum class PathClkOrData { clk, data };

const int path_clk_or_data_count = 2;

enum class TimingDerateType { cell_delay, cell_check, net_delay };
constexpr int timing_derate_type_count = 3;
enum class TimingDerateCellType { cell_delay, cell_check };
constexpr int timing_derate_cell_type_count = 2;

} // namespace
