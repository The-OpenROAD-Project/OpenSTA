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

#include <string>
#include <string_view>
#include <vector>

#include "SdcClass.hh"
#include "SearchClass.hh"
#include "StringUtil.hh"

namespace sta {

class Sta;
class Report;

PortSeq
filterPorts(std::string_view filter_expression,
            PortSeq *objects,
            bool bool_props_as_int,
            Sta *sta);

InstanceSeq
filterInstances(std::string_view filter_expression,
                InstanceSeq *objects,
                bool bool_props_as_int,
                Sta *sta);

PinSeq
filterPins(std::string_view filter_expression,
           PinSeq *objects,
           bool bool_props_as_int,
           Sta *sta);

NetSeq
filterNets(std::string_view filter_expression,
           NetSeq *objects,
           bool bool_props_as_int,
           Sta *sta);

ClockSeq
filterClocks(std::string_view filter_expression,
             ClockSeq *objects,
             bool bool_props_as_int,
             Sta *sta);

LibertyCellSeq
filterLibCells(std::string_view filter_expression,
               LibertyCellSeq *objects,
               bool bool_props_as_int,
               Sta *sta);

LibertyPortSeq
filterLibPins(std::string_view filter_expression,
              LibertyPortSeq *objects,
              bool bool_props_as_int,
              Sta *sta);

LibertyLibrarySeq
filterLibertyLibraries(std::string_view filter_expression,
                       LibertyLibrarySeq *objects,
                       bool bool_props_as_int,
                       Sta *sta);

EdgeSeq
filterTimingArcs(std::string_view filter_expression,
                  EdgeSeq *objects,
                  bool bool_props_as_int,
                  Sta *sta);

PathEndSeq
filterPathEnds(std::string_view filter_expression,
               PathEndSeq *objects,
               bool bool_props_as_int,
               Sta *sta);

// For FilterExpr unit tests.
StringSeq
filterExprToPostfix(std::string_view expr,
                    bool bool_props_as_int,
                    Report *report);

} // namespace
