// OpenSTA, Static Timing Analyzer
// Copyright (c) 2022, Parallax Software, Inc.
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

#include "MakeTimingModel.hh"

#include "Debug.hh"
#include "Units.hh"
#include "Transition.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "liberty/LibertyBuilder.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "dcalc/GraphDelayCalc1.hh"
#include "Sdc.hh"
#include "StaState.hh"
#include "PathEnd.hh"
#include "Sta.hh"

namespace sta {

MakeTimingModel::MakeTimingModel(const Corner *corner,
                                 Sta *sta) :
  StaState(sta),
  sta_(sta),
  corner_(corner),
  min_max_(MinMax::max()),
  lib_builder_(new LibertyBuilder)
{
}

MakeTimingModel::~MakeTimingModel()
{
  delete lib_builder_;
}

LibertyLibrary *
MakeTimingModel::makeTimingModel(const char *cell_name,
                                 const char *filename)
{
  makeLibrary(cell_name, filename);
  makeCell(cell_name, filename);
  makePorts();

  for (Clock *clk : *sdc_->clocks())
    sta_->setPropagatedClock(clk);

  findInputToOutputPaths();
  findInputSetupHolds();
  findClkedOutputPaths();
  cell_->finish(false, report_, debug_);
  return library_;
}

void
MakeTimingModel::makeLibrary(const char *cell_name,
                             const char *filename)
{
  library_ = network_->makeLibertyLibrary(cell_name, filename);
  LibertyLibrary *default_lib = network_->defaultLibertyLibrary();
  *library_->units()->timeUnit() = *default_lib->units()->timeUnit();
  *library_->units()->capacitanceUnit() = *default_lib->units()->capacitanceUnit();
  *library_->units()->voltageUnit() = *default_lib->units()->voltageUnit();
  *library_->units()->resistanceUnit() = *default_lib->units()->resistanceUnit();
  *library_->units()->pullingResistanceUnit() = *default_lib->units()->pullingResistanceUnit();
  *library_->units()->powerUnit() = *default_lib->units()->powerUnit();
  *library_->units()->distanceUnit() = *default_lib->units()->distanceUnit();

  for (RiseFall *rf : RiseFall::range()) {
    library_->setInputThreshold(rf, default_lib->inputThreshold(rf));
    library_->setOutputThreshold(rf, default_lib->outputThreshold(rf));
    library_->setSlewLowerThreshold(rf, default_lib->slewLowerThreshold(rf));
    library_->setSlewUpperThreshold(rf, default_lib->slewUpperThreshold(rf));
  }

  library_->setDelayModelType(default_lib->delayModelType());
  library_->setNominalProcess(default_lib->nominalProcess());
  library_->setNominalVoltage(default_lib->nominalVoltage());
  library_->setNominalTemperature(default_lib->nominalTemperature());
}

void
MakeTimingModel::makeCell(const char *cell_name,
                          const char *filename)
{
  cell_ = lib_builder_->makeCell(library_, cell_name, filename);
}

void
MakeTimingModel::makePorts()
{
  const DcalcAnalysisPt *dcalc_ap = corner_->findDcalcAnalysisPt(min_max_);
  InstancePinIterator *pin_iter = network_->pinIterator(network_->topInstance());
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();    
    Port *port = network_->port(pin);
    LibertyPort *lib_port = lib_builder_->makePort(cell_, network_->name(port));
    lib_port->setDirection(network_->direction(port));
    float load_cap = graph_delay_calc_->loadCap(pin, dcalc_ap);
    lib_port->setCapacitance(load_cap);
  }
  delete pin_iter;
}

// input -> output combinational paths
void
MakeTimingModel::findInputToOutputPaths()
{
  InstancePinIterator *input_iter = network_->pinIterator(network_->topInstance());
  while (input_iter->hasNext()) {
    Pin *input_pin = input_iter->next();    
    if (network_->direction(input_pin)->isInput()
        && !sta_->isClockSrc(input_pin)) {
      InstancePinIterator *output_iter = network_->pinIterator(network_->topInstance());
      while (output_iter->hasNext()) {
        Pin *output_pin = output_iter->next();    
        if (network_->direction(output_pin)->isOutput()) {
          PinSet *from_pins = new PinSet;
          from_pins->insert(input_pin);
          ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                        RiseFallBoth::riseFall());
          PinSet *to_pins = new PinSet;
          to_pins->insert(output_pin);
          ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                                  RiseFallBoth::riseFall(),
                                                  RiseFallBoth::riseFall());
          PathEndSeq *ends = sta_->findPathEnds(from, nullptr, to, true, corner_,
                                                min_max_->asMinMaxAll(),
                                                1, 1, false,
                                                -INF, INF, false, nullptr,
                                                false, false, false, false, false, false);
          if (!ends->empty()) {
            debugPrint(debug_, "make_timing_model", 1, "input %s -> output %s",
                       network_->pathName(input_pin),
                       network_->pathName(output_pin));
            PathEnd *end = (*ends)[0];
            sta_->reportPathEnd(end);
          }
        }
      }
    }
  }
}

// input -> register setup/hold
void
MakeTimingModel::findInputSetupHolds()
{
  InstancePinIterator *input_iter = network_->pinIterator(network_->topInstance());
  while (input_iter->hasNext()) {
    Pin *input_pin = input_iter->next();    
    if (network_->direction(input_pin)->isInput()
        && !sta_->isClockSrc(input_pin)) {
      LibertyPort *input_port = modelPort(input_pin);
      for (Clock *clk : *sdc_->clocks()) {
        for (const Pin *clk_pin : clk->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          for (RiseFall *clk_rf : RiseFall::range()) {
            for (MinMax *min_max : MinMax::range()) {
              MinMaxAll *min_max1 = min_max->asMinMaxAll();
              bool setup = min_max == MinMax::max();
              bool hold = !setup;
              TimingRole *role = setup ? TimingRole::setup() : TimingRole::hold();
              TimingArcAttrs *attrs = nullptr;
              for (RiseFall *input_rf : RiseFall::range()) {
                RiseFallBoth *input_rf1 = input_rf->asRiseFallBoth();
                sta_->setInputDelay(input_pin, input_rf1, clk, clk_rf,
                                    nullptr, false, false, min_max1, false, 0.0);
        
                PinSet *from_pins = new PinSet;
                from_pins->insert(input_pin);
                ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                              input_rf1);

                ClockSet *to_clks = new ClockSet;
                to_clks->insert(clk);
                ExceptionTo *to = sta_->makeExceptionTo(nullptr, to_clks, nullptr,
                                                        clk_rf->asRiseFallBoth(),
                                                        RiseFallBoth::riseFall());
                PathEndSeq *ends = sta_->findPathEnds(from, nullptr, to, false, corner_,
                                                      min_max1,
                                                      1, 1, false,
                                                      -INF, INF, false, nullptr,
                                                      setup, hold, setup, hold, setup, hold);
                if (!ends->empty()) {
                  PathEnd *end = (*ends)[0];
                  debugPrint(debug_, "make_timing_model", 1, "%s %s %s -> clock %s %s",
                             setup ? "setup" : "hold",
                             network_->pathName(input_pin),
                             input_rf->asString(),
                             clk->name(),
                             clk_rf->asString());
                  if (debug_->check("make_timing_model", 2))
                    sta_->reportPathEnd(end);
                  Arrival data_arrival = end->path()->arrival(sta_);
                  Delay clk_latency = end->targetClkDelay(sta_);
                  ArcDelay check_setup = end->margin(sta_);
                  float margin = data_arrival - clk_latency + check_setup;

                  ScaleFactorType scale_type = setup
                    ? ScaleFactorType::setup
                    : ScaleFactorType::hold;
                  TimingModel *check_model = makeScalarCheckModel(margin, scale_type, input_rf);
                  if (attrs == nullptr)
                    attrs = new TimingArcAttrs();
                  attrs->setModel(input_rf, check_model);
                }
                sta_->removeInputDelay(input_pin, input_rf1, clk, clk_rf, min_max1);
              }
              if (attrs)
                lib_builder_->makeFromTransitionArcs(cell_, clk_port,
                                                     input_port, nullptr,
                                                     clk_rf, role, attrs);
            }
          }
        }
      }
    }
  }
}

void
MakeTimingModel::findClkedOutputPaths()
{
  InstancePinIterator *output_iter = network_->pinIterator(network_->topInstance());
  while (output_iter->hasNext()) {
    Pin *output_pin = output_iter->next();    
    if (network_->direction(output_pin)->isOutput()) {
      LibertyPort *output_port = modelPort(output_pin);
      for (Clock *clk : *sdc_->clocks()) {
        for (const Pin *clk_pin : clk->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          for (RiseFall *clk_rf : RiseFall::range()) {
            TimingArcAttrs *attrs = nullptr;
            for (RiseFall *output_rf : RiseFall::range()) {
              RiseFallBoth *output_rf1 = output_rf->asRiseFallBoth();
              MinMax *min_max = MinMax::max();
              MinMaxAll *min_max1 = min_max->asMinMaxAll();
              sta_->setOutputDelay(output_pin, output_rf1, clk, clk_rf,
                                   nullptr, false, false, min_max1, false, 0.0);
        
              ClockSet *from_clks = new ClockSet;
              from_clks->insert(clk);
              ExceptionFrom *from = sta_->makeExceptionFrom(nullptr, from_clks, nullptr,
                                                            clk_rf->asRiseFallBoth());
              PinSet *to_pins = new PinSet;
              to_pins->insert(output_pin);
              ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                                      output_rf1, output_rf1);

              PathEndSeq *ends = sta_->findPathEnds(from, nullptr, to, false, corner_, min_max1,
                                                    1, 1, false, -INF, INF, false, nullptr,
                                                    true, false, false, false, false, false);
              if (!ends->empty()) {
                debugPrint(debug_, "make_timing_model", 1, "clock %s -> output %s",
                           clk->name(),
                           network_->pathName(output_pin));
                PathEnd *end = (*ends)[0];
                if (debug_->check("make_timing_model", 2))
                  sta_->reportPathEnd(end);
                Arrival delay = end->path()->arrival(sta_);
                Slew slew = end->path()->slew(sta_);
                TimingModel *check_model = makeScalarGateModel(delay, slew, output_rf);
                if (attrs == nullptr)
                  attrs = new TimingArcAttrs();
                attrs->setModel(output_rf, check_model);
              }
              sta_->removeOutputDelay(output_pin, output_rf1, clk, clk_rf, MinMaxAll::max());
            }
            if (attrs)
              lib_builder_->makeFromTransitionArcs(cell_, clk_port,
                                                   output_port, nullptr,
                                                   clk_rf, TimingRole::regClkToQ(),
                                                   attrs);
          }
        }
      }
    }
  }
}

LibertyPort *
MakeTimingModel::modelPort(const Pin *pin)
{
  return cell_->findLibertyPort(network_->name(network_->port(pin)));
}

TimingModel *
MakeTimingModel::makeScalarCheckModel(float value,
                                      ScaleFactorType scale_factor_type,
                                      RiseFall *rf)
{
  Table *table = new Table0(value);
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *table_model = new TableModel(table, tbl_template,
                                           scale_factor_type, rf);
  CheckTableModel *check_model = new CheckTableModel(table_model, nullptr);
  return check_model;
}

TimingModel *
MakeTimingModel::makeScalarGateModel(Delay delay,
                                     Slew slew,
                                     RiseFall *rf)
{
  Table *delay_table = new Table0(delay);
  Table *slew_table = new Table0(slew);
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *delay_model = new TableModel(delay_table, tbl_template,
                                           ScaleFactorType::cell, rf);
  TableModel *slew_model = new TableModel(slew_table, tbl_template,
                                          ScaleFactorType::cell, rf);
  GateTableModel *gate_model = new GateTableModel(delay_model, nullptr,
                                                  slew_model, nullptr);
  return gate_model;
}

} // namespace
