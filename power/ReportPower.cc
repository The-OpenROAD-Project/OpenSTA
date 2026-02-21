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

#include "ReportPower.hh"

#include <cmath>
#include <algorithm>

#include "Report.hh"
#include "Network.hh"
#include "StringUtil.hh"

namespace sta {

ReportPower::ReportPower(StaState *sta) :
  StaState(sta)
{
}

void
ReportPower::reportDesign(PowerResult &total,
                          PowerResult &sequential,
                          PowerResult &combinational,
                          PowerResult &clock,
                          PowerResult &macro,
                          PowerResult &pad,
                          int digits)
{
  float design_internal = total.internal();
  float design_switching = total.switching();
  float design_leakage = total.leakage();
  float design_total = total.total();
  
  int field_width = std::max(digits + 6, 10);
  
  reportTitle5("Group", "Internal", "Switching", "Leakage", "Total",
               field_width);
  reportTitle5Units("     ", "Power", "Power", "Power", "Power", "(Watts)",
                    field_width);
  reportTitleDashes5(field_width);
  
  reportRow("Sequential", sequential, design_total, field_width, digits);
  reportRow("Combinational", combinational, design_total, field_width, digits);
  reportRow("Clock", clock, design_total, field_width, digits);
  reportRow("Macro", macro, design_total, field_width, digits);
  reportRow("Pad", pad, design_total, field_width, digits);
  
  reportTitleDashes5(field_width);
  
  // Report total row using the totals PowerResult
  reportRow("Total", total, design_total, field_width, digits);
  
  // Report percentage line
  std::string percent_line = stdstrPrint("%-20s", "");
  percent_line += powerColPercent(design_internal, design_total, field_width);
  percent_line += powerColPercent(design_switching, design_total, field_width);
  percent_line += powerColPercent(design_leakage, design_total, field_width);
  report_->reportLineString(percent_line);
}

void
ReportPower::reportInsts(const InstPowers &inst_pwrs,
                         int digits)
{
  int field_width = std::max(digits + 6, 10);
  
  reportTitle4("Internal", "Switching", "Leakage", "Total",
               field_width);
  reportTitle4Units("Power", "Power", "Power", "Power", "(Watts)",
                    field_width);
  reportTitleDashes4(field_width);
  
  for (const InstPower &inst_pwr : inst_pwrs) {
    reportInst(inst_pwr.first, inst_pwr.second, field_width, digits);
  }
}

void
ReportPower::reportInst(const Instance *inst,
                        const PowerResult &power,
                        int field_width,
                        int digits)
{
  float internal = power.internal();
  float switching = power.switching();
  float leakage = power.leakage();
  float total = power.total();
  
  std::string line = powerCol(internal, field_width, digits);
  line += powerCol(switching, field_width, digits);
  line += powerCol(leakage, field_width, digits);
  line += powerCol(total, field_width, digits);
  line += " ";
  line += network_->pathName(inst);
  report_->reportLineString(line);
}

std::string
ReportPower::powerCol(float pwr,
                      int field_width,
                      int digits)
{
  if (std::isnan(pwr))
    return stdstrPrint(" %*s", field_width, "NaN");
  else
    return stdstrPrint(" %*.*e", field_width, digits, pwr);
}

std::string
ReportPower::powerColPercent(float col_total,
                             float total,
                             int field_width)
{
  float percent = 0.0;
  if (total != 0.0 && !std::isnan(total)) {
    percent = col_total / total * 100.0;
  }
  return stdstrPrint("%*.*f%%", field_width, 1, percent);
}

void
ReportPower::reportTitle5(const char *title1,
                           const char *title2,
                           const char *title3,
                           const char *title4,
                           const char *title5,
                           int field_width)
{
  report_->reportLine("%-20s %*s %*s %*s %*s",
                      title1,
                      field_width, title2,
                      field_width, title3,
                      field_width, title4,
                      field_width, title5);
}

void
ReportPower::reportTitle5Units(const char *title1,
                                const char *title2,
                                const char *title3,
                                const char *title4,
                                const char *title5,
                                const char *units,
                                int field_width)
{
  report_->reportLine("%-20s %*s %*s %*s %*s %s",
                      title1,
                      field_width, title2,
                      field_width, title3,
                      field_width, title4,
                      field_width, title5,
                      units);
}

void
ReportPower::reportTitleDashes5(int field_width)
{
  int count = 20 + (field_width + 1) * 4;
  std::string dashes(count, '-');
  report_->reportLineString(dashes);
}

void
ReportPower::reportRow(const char *type,
                       const PowerResult &power,
                       float design_total,
                       int field_width,
                       int digits)
{
  float internal = power.internal();
  float switching = power.switching();
  float leakage = power.leakage();
  float total = power.total();
  
  float percent = 0.0;
  if (design_total != 0.0 && !std::isnan(design_total))
    percent = total / design_total * 100.0;
  
  std::string line = stdstrPrint("%-20s", type);
  line += powerCol(internal, field_width, digits);
  line += powerCol(switching, field_width, digits);
  line += powerCol(leakage, field_width, digits);
  line += powerCol(total, field_width, digits);
  line += stdstrPrint(" %5.1f%%", percent);
  report_->reportLineString(line);
}

void
ReportPower::reportTitle4(const char *title1,
                          const char *title2,
                          const char *title3,
                          const char *title4,
                          int field_width)
{
  report_->reportLine(" %*s %*s %*s %*s",
                      field_width, title1,
                      field_width, title2,
                      field_width, title3,
                      field_width, title4);
}

void
ReportPower::reportTitle4Units(const char *title1,
                               const char *title2,
                               const char *title3,
                               const char *title4,
                               const char *units,
                               int field_width)
{
  report_->reportLine(" %*s %*s %*s %*s %s",
                      field_width, title1,
                      field_width, title2,
                      field_width, title3,
                      field_width, title4,
                      units);
}

void
ReportPower::reportTitleDashes4(int field_width)
{
  int count = (field_width + 1) * 4;
  std::string dashes(count, '-');
  report_->reportLineString(dashes);
}

} // namespace
