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

#include "LibertyClass.hh"
#include "Transition.hh"

namespace sta {

class InternalPowerAttrs;
class InternalPowerModel;

class InternalPowerAttrs
{
public:
  InternalPowerAttrs();
  virtual ~InternalPowerAttrs();
  void deleteContents();
  FuncExpr *when() const { return when_; }
  void setWhen(FuncExpr *when);
  void setModel(const RiseFall *rf,
		InternalPowerModel *model);
  InternalPowerModel *model(const RiseFall *rf) const;
  const char *relatedPgPin() const { return related_pg_pin_; }
  void setRelatedPgPin(const char *related_pg_pin);

protected:
  FuncExpr *when_;
  InternalPowerModel *models_[RiseFall::index_count];
  const  char *related_pg_pin_;
};

class InternalPower
{
public:
  InternalPower(LibertyCell *cell,
		LibertyPort *port,
		LibertyPort *related_port,
		InternalPowerAttrs *attrs);
  ~InternalPower();
  LibertyCell *libertyCell() const;
  LibertyPort *port() const { return port_; }
  LibertyPort *relatedPort() const { return related_port_; }
  FuncExpr *when() const { return when_; }
  const char *relatedPgPin() const { return related_pg_pin_; }
  float power(const RiseFall *rf,
	      const Pvt *pvt,
	      float in_slew,
	      float load_cap);

protected:
  LibertyPort *port_;
  LibertyPort *related_port_;
  FuncExpr *when_;
  const  char *related_pg_pin_;
  InternalPowerModel *models_[RiseFall::index_count];
};

class InternalPowerModel
{
public:
  explicit InternalPowerModel(TableModel *model);
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
