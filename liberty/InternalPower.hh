// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
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

#pragma once

#include "DisallowCopyAssign.hh"
#include "Transition.hh"
#include "LibertyClass.hh"

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
  FuncExpr *&whenRef() { return when_; }
  void setModel(RiseFall *rf,
		InternalPowerModel *model);
  InternalPowerModel *model(RiseFall *rf) const;
  const char *relatedPgPin() const { return related_pg_pin_; }
  void setRelatedPgPin(const char *related_pg_pin);

protected:
  FuncExpr *when_;
  InternalPowerModel *models_[RiseFall::index_count];
  const  char *related_pg_pin_;

private:
  DISALLOW_COPY_AND_ASSIGN(InternalPowerAttrs);
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
  float power(RiseFall *rf,
	      const Pvt *pvt,
	      float in_slew,
	      float load_cap);

protected:
  LibertyPort *port_;
  LibertyPort *related_port_;
  FuncExpr *when_;
  const  char *related_pg_pin_;
  InternalPowerModel *models_[RiseFall::index_count];

private:
  DISALLOW_COPY_AND_ASSIGN(InternalPower);
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
  void reportPower(const LibertyCell *cell,
		   const Pvt *pvt,
		   float in_slew,
		   float load_cap,
		   int digits,
		   string *result) const;

protected:
  void findAxisValues(float in_slew,
		      float load_cap,
		      // Return values.
		      float &axis_value1,
		      float &axis_value2,
		      float &axis_value3) const;
  float axisValue(TableAxis *axis,
		  float in_slew,
		  float load_cap) const;
  bool checkAxes(const TableModel *model);
  bool checkAxis(TableAxis *axis);

  TableModel *model_;

private:
  DISALLOW_COPY_AND_ASSIGN(InternalPowerModel);
};

} // namespace
