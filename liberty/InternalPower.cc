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

#include "Machine.hh"
#include "FuncExpr.hh"
#include "TableModel.hh"
#include "Liberty.hh"
#include "InternalPower.hh"

namespace sta {

InternalPowerAttrs::InternalPowerAttrs() :
  when_(nullptr),
  models_{nullptr, nullptr},
  related_pg_pin_(nullptr)
{
}

InternalPowerAttrs::~InternalPowerAttrs()
{
}

void
InternalPowerAttrs::deleteContents()
{
  for (auto tr_index : RiseFall::rangeIndex()) {
    InternalPowerModel *model = models_[tr_index];
    if (model)
      delete model;
  }
  if (when_)
    when_->deleteSubexprs();
  stringDelete(related_pg_pin_);
}

InternalPowerModel *
InternalPowerAttrs::model(RiseFall *rf) const
{
  return models_[rf->index()];
}

void
InternalPowerAttrs::setModel(RiseFall *rf,
			     InternalPowerModel *model)
{
  models_[rf->index()] = model;
}

void
InternalPowerAttrs::setRelatedPgPin(const char *related_pg_pin)
{
  stringDelete(related_pg_pin_);
  related_pg_pin_ = stringCopy(related_pg_pin);
}

////////////////////////////////////////////////////////////////

InternalPower::InternalPower(LibertyCell *cell,
			     LibertyPort *port,
			     LibertyPort *related_port,
			     InternalPowerAttrs *attrs) :
  port_(port),
  related_port_(related_port),
  when_(attrs->when()),
  related_pg_pin_(attrs->relatedPgPin())
{
  for (auto tr : RiseFall::range()) {
    int tr_index = tr->index();
    models_[tr_index] = attrs->model(tr);
  }
  cell->addInternalPower(this);
}

InternalPower::~InternalPower()
{
  // models_, when_ and related_pg_pin_ are owned by InternalPowerAttrs.
}

LibertyCell *
InternalPower::libertyCell() const
{
  return port_->libertyCell();
}

float
InternalPower::power(RiseFall *rf,
		     const Pvt *pvt,
		     float in_slew,
		     float load_cap)
{
  InternalPowerModel *model = models_[rf->index()];
  if (model)
    return model->power(libertyCell(), pvt, in_slew, load_cap);
  else
    return 0.0;
}

////////////////////////////////////////////////////////////////

InternalPowerModel::InternalPowerModel(TableModel *model) :
  model_(model)
{
}

InternalPowerModel::~InternalPowerModel()
{
  delete model_;
}

float
InternalPowerModel::power(const LibertyCell *cell,
			  const Pvt *pvt,
			  float in_slew,
			  float load_cap) const
{
  if (model_) {
    float axis_value1, axis_value2, axis_value3;
    findAxisValues(in_slew, load_cap,
		   axis_value1, axis_value2, axis_value3);
    const LibertyLibrary *library = cell->libertyLibrary();
    return model_->findValue(library, cell, pvt,
			     axis_value1, axis_value2, axis_value3);
  }
  else
    return 0.0;
}

void
InternalPowerModel::reportPower(const LibertyCell *cell,
				const Pvt *pvt,
				float in_slew,
				float load_cap,
				int digits,
				string *result) const
{
  if (model_) {
    float axis_value1, axis_value2, axis_value3;
    findAxisValues(in_slew, load_cap,
		   axis_value1, axis_value2, axis_value3);
    const LibertyLibrary *library = cell->libertyLibrary();
    model_->reportValue("Power", library, cell, pvt,
			axis_value1, nullptr, axis_value2, axis_value3, digits, result);
  }
}

void
InternalPowerModel::findAxisValues(float in_slew,
				   float load_cap,
				   // Return values.
				   float &axis_value1,
				   float &axis_value2,
				   float &axis_value3) const
{
  switch (model_->order()) {
  case 0:
    axis_value1 = 0.0;
    axis_value2 = 0.0;
    axis_value3 = 0.0;
    break;
  case 1:
    axis_value1 = axisValue(model_->axis1(), in_slew, load_cap);
    axis_value2 = 0.0;
    axis_value3 = 0.0;
    break;
  case 2:
    axis_value1 = axisValue(model_->axis1(), in_slew, load_cap);
    axis_value2 = axisValue(model_->axis2(), in_slew, load_cap);
    axis_value3 = 0.0;
    break;
  case 3:
    axis_value1 = axisValue(model_->axis1(), in_slew, load_cap);
    axis_value2 = axisValue(model_->axis2(), in_slew, load_cap);
    axis_value3 = axisValue(model_->axis3(), in_slew, load_cap);
    break;
  default:
    internalError("unsupported table order");
  }
}

float
InternalPowerModel::axisValue(TableAxis *axis,
			      float in_slew,
			      float load_cap) const
{
  TableAxisVariable var = axis->variable();
  if (var == TableAxisVariable::input_transition_time)
    return in_slew;
  else if (var == TableAxisVariable::total_output_net_capacitance)
    return load_cap;
  else {
    internalError("unsupported table axes");
    return 0.0;
  }
}

bool
InternalPowerModel::checkAxes(const TableModel *model)
{
  TableAxis *axis1 = model->axis1();
  TableAxis *axis2 = model->axis2();
  TableAxis *axis3 = model->axis3();
  bool axis_ok = true;
  if (axis1)
    axis_ok &= checkAxis(model->axis1());
  if (axis2)
    axis_ok &= checkAxis(model->axis2());
  axis_ok &= (axis3 == nullptr);
  return axis_ok;
}

bool
InternalPowerModel::checkAxis(TableAxis *axis)
{
  TableAxisVariable var = axis->variable();
  return var == TableAxisVariable::constrained_pin_transition
    || var == TableAxisVariable::related_pin_transition
    || var == TableAxisVariable::related_out_total_output_net_capacitance;
}

} // namespace
