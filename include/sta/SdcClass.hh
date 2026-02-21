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

#pragma once

#include <map>
#include <set>
#include <vector>

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

using ClockPair = std::pair<const Clock*, const Clock*>;

class ClockIndexLess
{
public:
  bool operator()(const Clock *clk1,
                  const Clock *clk2) const;
};

using FloatSeq = std::vector<float>;
using IntSeq = std::vector<int>;
using ClockSeq = std::vector<Clock*>;
using ConstClockSeq = std::vector<const Clock*>;
using ClockSet = std::set<Clock*, ClockIndexLess>;
using ConstClockSet = std::set<const Clock*, ClockIndexLess>;
using ClockGroup = ClockSet;
using PinSetSeq = std::vector<PinSet*>;
using SetupHold = MinMax;
using SetupHoldAll = MinMaxAll;
using ExceptionThruSeq = std::vector<ExceptionThru*>;
using LibertyPortPairSet = std::set<LibertyPortPair, LibertyPortPairLess>;
using DisabledInstancePortsMap = std::map<const Instance*, DisabledInstancePorts*>;
using DisabledCellPortsMap = std::map<LibertyCell*, DisabledCellPorts*>;
using ClockUncertainties = MinMaxValues<float>;
using ExceptionPathSet = std::set<ExceptionPath*>;
using EdgePins = PinPair;
using EdgePinsSet = PinPairSet;
using LogicValueMap = std::map<const Pin*, LogicValue>;

class ClockSetLess
{
public:
  bool operator()(const ClockSet *set1,
                  const ClockSet *set2) const;
};

using ClockGroupSet = std::set<ClockGroup*, ClockSetLess>;

// For Search.
class ExceptionState;

class ExceptionStateLess
{
public:
  bool operator()(const ExceptionState *state1,
                  const ExceptionState *state2) const;
};

class ExceptionPath;
using ExceptionStateSet = std::set<ExceptionState*, ExceptionStateLess>;

// Constraint applies to clock or data paths.
enum class PathClkOrData { clk, data };

const int path_clk_or_data_count = 2;

enum class TimingDerateType { cell_delay, cell_check, net_delay };
constexpr int timing_derate_type_count = 3;
enum class TimingDerateCellType { cell_delay, cell_check };
constexpr int timing_derate_cell_type_count = 2;

} // namespace
