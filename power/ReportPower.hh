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

#include "StaState.hh"
#include "NetworkClass.hh"
#include "PowerClass.hh"

namespace sta {

class ReportPower : public StaState
{
public:
  ReportPower(StaState *sta);
  void reportDesign(PowerResult &total,
                    PowerResult &sequential,
                    PowerResult &combinational,
                    PowerResult &clock,
                    PowerResult &macro,
                    PowerResult &pad,
                    int digits);
  void reportInsts(const InstPowers &inst_pwrs,
                   int digits);

private:
  std::string powerCol(float pwr,
                       int field_width,
                       int digits);
  std::string powerColPercent(float col_total,
                              float total,
                              int field_width);
  void reportTitle5(const char *title1,
                    const char *title2,
                    const char *title3,
                    const char *title4,
                    const char *title5,
                    int field_width);
  void reportTitle5Units(const char *title1,
                         const char *title2,
                         const char *title3,
                         const char *title4,
                         const char *title5,
                         const char *units,
                         int field_width);
  void reportTitleDashes5(int field_width);
  void reportRow(const char *type,
                 const PowerResult &power,
                 float design_total,
                 int field_width,
                 int digits);
  void reportTitle4(const char *title1,
                    const char *title2,
                    const char *title3,
                    const char *title4,
                    int field_width);
  void reportTitle4Units(const char *title1,
                         const char *title2,
                         const char *title3,
                         const char *title4,
                         const char *units,
                         int field_width);
  void reportTitleDashes4(int field_width);
  void reportInst(const Instance *inst,
                  const PowerResult &power,
                  int field_width,
                  int digits);
};

} // namespace
