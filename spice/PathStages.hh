// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include <iterator>
#include <string>

#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "PathExpanded.hh"

namespace sta {

using Stage = int;

// Stage "accessors".
//
//           stage
//      |---------------|
//        |\             |\   .
// -------| >---/\/\/----| >---
//  gate  |/ drvr    load|/
//  input
//
// A path from an input port has no GateInputPath (the input port is the drvr).
// Internally a stage index from stageFirst() to stageLast()
// is turned into an index into path_expanded_.
class PathStages : public StaState
{
public:
  PathStages(PathExpanded &path_expanded,
             const StaState *sta);
  Stage stageFirst() const;
  Stage stageLast() const;
  std::string stageName(Stage stage) const;
  int stageGateInputPathIndex(Stage stage) const;
  int stageDrvrPathIndex(Stage stage) const;
  int stageLoadPathIndex(Stage stage) const;
  const Path *stageGateInputPath(Stage stage) const;
  const Path *stageDrvrPath(Stage stage) const;
  const Path *stageLoadPath(Stage stage) const;
  const TimingArc *stageGateArc(Stage stage) const;
  const TimingArc *stageWireArc(Stage stage) const;
  Edge *stageGateEdge(Stage stage) const;
  const Edge *stageWireEdge(Stage stage) const;
  const Pin *stageGateInputPin(Stage stage) const;
  const Pin *stageDrvrPin(Stage stage) const;
  const LibertyPort *stageGateInputPort(Stage stage) const;
  const LibertyPort *stageDrvrPort(Stage stage) const;
  const Pin *stageLoadPin(Stage stage) const;
  std::string stageGateInputPinName(Stage stage) const;
  std::string stageDrvrPinName(Stage stage) const;
  std::string stageLoadPinName(Stage stage) const;
  const LibertyCell *stageLibertyCell(Stage stage) const;
  const Instance *stageInstance(Stage stage) const;

  class Iterator
  {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Stage;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = Stage;

    Iterator() = default;
    explicit Iterator(Stage stage) : stage_(stage) {}

    Stage operator*() const { return stage_; }

    Iterator &operator++()
    {
      ++stage_;
      return *this;
    }

    Iterator operator++(int)
    {
      Iterator tmp(*this);
      ++*this;
      return tmp;
    }

    bool operator==(const Iterator &other) const { return stage_ == other.stage_; }
    bool operator!=(const Iterator &other) const { return !(*this == other); }

  private:
    Stage stage_{0};
  };

  Iterator begin() const { return Iterator(stageFirst()); }
  Iterator end() const { return Iterator(stageLast() + 1); }

protected:
  PathExpanded &path_expanded_;
};

} // namespace sta
