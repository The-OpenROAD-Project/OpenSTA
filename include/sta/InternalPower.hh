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

#include <array>
#include <memory>
#include <string>

#include "LibertyClass.hh"
#include "Transition.hh"

namespace sta {

class InternalPowerModel;

using InternalPowerModels =
  std::array<std::shared_ptr<InternalPowerModel>, RiseFall::index_count>;

class InternalPower
{
public:
  InternalPower(LibertyPort *port,
                LibertyPort *related_port,
                LibertyPort *related_pg_pin,
                const std::shared_ptr<FuncExpr> &when,
                InternalPowerModels &models);
  //InternalPower(InternalPower &&other) noexcept;
  LibertyCell *libertyCell() const;
  LibertyPort *port() const { return port_; }
  LibertyPort *relatedPort() const { return related_port_; }
  FuncExpr *when() const { return when_.get(); }
  LibertyPort *relatedPgPin() const { return related_pg_pin_; }
  float power(const RiseFall *rf,
              const Pvt *pvt,
              float in_slew,
              float load_cap) const;
  const InternalPowerModel *model(const RiseFall *rf) const;

protected:
  LibertyPort *port_;
  LibertyPort *related_port_;
  LibertyPort *related_pg_pin_;
  std::shared_ptr<FuncExpr> when_;
  InternalPowerModels models_;
};

class InternalPowerModel
{
public:
  InternalPowerModel(TableModel *model);
  ~InternalPowerModel();
  float power(const LibertyCell *cell,
              const Pvt *pvt,
              float in_slew,
              float load_cap) const;
  std::string reportPower(const LibertyCell *cell,
                          const Pvt *pvt,
                          float in_slew,
                          float load_cap,
                          int digits) const;
  const TableModel *model() const { return model_; }

protected:
  void findAxisValues(float in_slew,
                      float load_cap,
                      // Return values.
                      float &axis_value1,
                      float &axis_value2,
                      float &axis_value3) const;
  float axisValue(const TableAxis *axis,
                  float in_slew,
                  float load_cap) const;
  bool checkAxes(const TableModel *model);
  bool checkAxis(const TableAxis *axis);

  TableModel *model_;
};

} // namespace
