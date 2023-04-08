// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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
#include "MakeTimingModelPvt.hh"

#include <algorithm>
#include <map>

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
#include "Graph.hh"
#include "PathEnd.hh"
#include "Search.hh"
#include "Sta.hh"
#include "VisitPathEnds.hh"
#include "ArcDelayCalc.hh"

namespace sta {

using std::max;
using std::make_shared;

LibertyLibrary *
makeTimingModel(const char *lib_name,
                const char *cell_name,
                const char *filename,
                const Corner *corner,
                Sta *sta)
{
  MakeTimingModel maker(lib_name, cell_name, filename, corner, sta);
  return maker.makeTimingModel();
}

MakeTimingModel::MakeTimingModel(const char *lib_name,
                                 const char *cell_name,
                                 const char *filename,
                                 const Corner *corner,
                                 Sta *sta) :
  StaState(sta),
  lib_name_(lib_name),
  cell_name_(cell_name),
  filename_(filename),
  corner_(corner),
  cell_(nullptr),
  min_max_(MinMax::max()),
  lib_builder_(new LibertyBuilder),
  tbl_template_index_(1),
  sdc_backup_(nullptr),
  sta_(sta)
{
}

MakeTimingModel::~MakeTimingModel()
{
  delete lib_builder_;
}

LibertyLibrary *
MakeTimingModel::makeTimingModel()
{
  saveSdc();

  tbl_template_index_ = 1;
  makeLibrary();
  makeCell();
  makePorts();

  sta_->searchPreamble();
  graph_ = sta_->graph();

  findTimingFromInputs();
  findClkedOutputPaths();

  cell_->finish(false, report_, debug_);
  restoreSdc();
  
  return library_;
}

// Move sdc commands used by makeTimingModel to the side.
void
MakeTimingModel::saveSdc()
{
  sdc_backup_ = new Sdc(this);
  Sdc::movePortDelays(sdc_, sdc_backup_);
  Sdc::movePortExtCaps(sdc_, sdc_backup_);
  Sdc::moveDeratingFactors(sdc_, sdc_backup_);
  sta_->delaysInvalid();
}

void
MakeTimingModel::restoreSdc()
{
  Sdc::movePortDelays(sdc_backup_, sdc_);
  Sdc::movePortExtCaps(sdc_backup_, sdc_);
  Sdc::moveDeratingFactors(sdc_backup_, sdc_);
  delete sdc_backup_;
  sta_->delaysInvalid();
}

void
MakeTimingModel::makeLibrary()
{
  library_ = network_->makeLibertyLibrary(lib_name_, filename_);
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
MakeTimingModel::makeCell()
{
  cell_ = lib_builder_->makeCell(library_, cell_name_, filename_);
}

void
MakeTimingModel::makePorts()
{
  const DcalcAnalysisPt *dcalc_ap = corner_->findDcalcAnalysisPt(min_max_);
  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  CellPortIterator *port_iter = network_->portIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    const char *port_name = network_->name(port);
    if (network_->isBus(port)) {
      int from_index = network_->fromIndex(port);
      int to_index = network_->toIndex(port);
      BusDcl *bus_dcl = new BusDcl(port_name, from_index, to_index);
      library_->addBusDcl(bus_dcl);
      LibertyPort *lib_port = lib_builder_->makeBusPort(cell_, port_name,
                                                        from_index, to_index,
                                                        bus_dcl);
      lib_port->setDirection(network_->direction(port));
      PortMemberIterator *member_iter = network_->memberIterator(port);
      while (member_iter->hasNext()) {
        Port *bit_port = member_iter->next();
        Pin *pin = network_->findPin(top_inst, bit_port);
        LibertyPort *lib_bit_port = modelPort(pin);
        float load_cap = graph_delay_calc_->loadCap(pin, dcalc_ap);
        lib_bit_port->setCapacitance(load_cap);
      }
    }
    else {
      LibertyPort *lib_port = lib_builder_->makePort(cell_, port_name);
      lib_port->setDirection(network_->direction(port));
      Pin *pin = network_->findPin(top_inst, port);
      float load_cap = graph_delay_calc_->loadCap(pin, dcalc_ap);
      lib_port->setCapacitance(load_cap);
    }
  }
  delete port_iter;
}

void
MakeTimingModel::checkClock(Clock *clk)
{
  for (const Pin *pin : clk->leafPins()) {
    if (!network_->isTopLevelPort(pin))
      report_->warn(810, "clock %s pin %s is inside model block.",
                    clk->name(),
                    network_->pathName(pin));
  }
}

////////////////////////////////////////////////////////////////

class MakeEndTimingArcs : public PathEndVisitor
{
public:
  MakeEndTimingArcs(Sta *sta);
  MakeEndTimingArcs(const MakeEndTimingArcs&) = default;
  virtual ~MakeEndTimingArcs() {}
  virtual PathEndVisitor *copy() const;
  virtual void visit(PathEnd *path_end);
  void setInputRf(const RiseFall *input_rf);
  const ClockEdgeDelays &margins() const { return margins_; }

private:
  const RiseFall *input_rf_;
  ClockEdgeDelays margins_;
  Sta *sta_;
};

MakeEndTimingArcs::MakeEndTimingArcs(Sta *sta) :
  input_rf_(nullptr),
  sta_(sta)
{
}

PathEndVisitor *
MakeEndTimingArcs::copy() const
{
  return new MakeEndTimingArcs(*this);
}

void
MakeEndTimingArcs::setInputRf(const RiseFall *input_rf)
{
  input_rf_ = input_rf;
}

void
MakeEndTimingArcs::visit(PathEnd *path_end)
{
  Path *src_path = path_end->path();
  const Clock *src_clk = src_path->clock(sta_);
  const ClockEdge *tgt_clk_edge = path_end->targetClkEdge(sta_);
  if (src_clk == sta_->sdc()->defaultArrivalClock()
      && tgt_clk_edge) {
    Network *network = sta_->network();
    Debug *debug = sta_->debug();
    const MinMax *min_max = path_end->minMax(sta_);
    Arrival data_delay = src_path->arrival(sta_);
    Delay clk_latency = path_end->targetClkDelay(sta_);
    ArcDelay check_margin = path_end->margin(sta_);
    Delay margin = min_max == MinMax::max()
      ? data_delay - clk_latency + check_margin
      : clk_latency - data_delay + check_margin;
    float delay1 = delayAsFloat(margin, MinMax::max(), sta_);
    debugPrint(debug, "make_timing_model", 2, "%s -> %s clock %s %s %s %s",
               input_rf_->shortName(),
               network->pathName(src_path->pin(sta_)),
               tgt_clk_edge->name(),
               path_end->typeName(),
               min_max->asString(),
               delayAsString(margin, sta_));
    if (debug->check("make_timing_model", 3))
      sta_->reportPathEnd(path_end);

    RiseFallMinMax &margins = margins_[tgt_clk_edge];
    float max_margin;
    bool max_exists;
    margins.value(input_rf_, min_max, max_margin, max_exists);
    // Always max margin, even for min/hold checks.
    margins.setValue(input_rf_, min_max,
                     max_exists ? max(max_margin, delay1) : delay1);
  }
}

// input -> register setup/hold
// input -> output combinational paths
// Use default input arrival (set_input_delay with no clock) from inputs
// to find downstream register checks and output ports.
void
MakeTimingModel::findTimingFromInputs()
{
  search_->deleteFilteredArrivals();

  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  CellPortBitIterator *port_iter = network_->portBitIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *input_port = port_iter->next();
    if (network_->direction(input_port)->isInput())
      findTimingFromInput(input_port);
  }
  delete port_iter;
}

void
MakeTimingModel::findTimingFromInput(Port *input_port)
{
  Instance *top_inst = network_->topInstance();
  Pin *input_pin = network_->findPin(top_inst, input_port);
  if (!sta_->isClockSrc(input_pin)) {
    MakeEndTimingArcs end_visitor(sta_);
    OutputPinDelays output_delays;
    for (RiseFall *input_rf : RiseFall::range()) {
      RiseFallBoth *input_rf1 = input_rf->asRiseFallBoth();
      sta_->setInputDelay(input_pin, input_rf1,
                          sdc_->defaultArrivalClock(),
                          sdc_->defaultArrivalClockEdge()->transition(),
                          nullptr, false, false, MinMaxAll::all(), true, 0.0);

      PinSet *from_pins = new PinSet(network_);
      from_pins->insert(input_pin);
      ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                    input_rf1);
      search_->findFilteredArrivals(from, nullptr, nullptr, false, false);

      end_visitor.setInputRf(input_rf);
      VertexSeq endpoints = search_->filteredEndpoints();
      VisitPathEnds visit_ends(sta_);
      for (Vertex *end : endpoints)
        visit_ends.visitPathEnds(end, corner_, MinMaxAll::all(), true, &end_visitor);
      findOutputDelays(input_rf, output_delays);
      search_->deleteFilteredArrivals();

      sta_->removeInputDelay(input_pin, input_rf1,
                             sdc_->defaultArrivalClock(),
                             sdc_->defaultArrivalClockEdge()->transition(),
                             MinMaxAll::all());
    }
    makeSetupHoldTimingArcs(input_pin, end_visitor.margins());
    makeInputOutputTimingArcs(input_pin, output_delays);
  }
}

void
MakeTimingModel::findOutputDelays(const RiseFall *input_rf,
                                  OutputPinDelays &output_pin_delays)
{
  InstancePinIterator *output_iter = network_->pinIterator(network_->topInstance());
  while (output_iter->hasNext()) {
    Pin *output_pin = output_iter->next();
    if (network_->direction(output_pin)->isOutput()) {
      Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
      VertexPathIterator path_iter(output_vertex, this);
      while (path_iter.hasNext()) {
        PathVertex *path = path_iter.next();
        if (search_->matchesFilter(path, nullptr)) {
          const RiseFall *output_rf = path->transition(sta_);
          const MinMax *min_max = path->minMax(sta_);
          Arrival delay = path->arrival(sta_);
          OutputDelays &delays = output_pin_delays[output_pin];
          delays.delays.mergeValue(output_rf, min_max,
                                   delayAsFloat(delay, min_max, sta_));
          delays.rf_path_exists[input_rf->index()][output_rf->index()] = true;
        }
      }
    }
  }
  delete output_iter;
}

void
MakeTimingModel::makeSetupHoldTimingArcs(const Pin *input_pin,
                                         const ClockEdgeDelays &clk_margins)
{
  for (auto clk_edge_margins : clk_margins) {
    const ClockEdge *clk_edge = clk_edge_margins.first;
    RiseFallMinMax &margins = clk_edge_margins.second;
    for (MinMax *min_max : MinMax::range()) {
      bool setup = (min_max == MinMax::max());
      TimingArcAttrsPtr attrs = nullptr;
      for (RiseFall *input_rf : RiseFall::range()) {
        float margin;
        bool exists;
        margins.value(input_rf, min_max, margin, exists);
        if (exists) {
          debugPrint(debug_, "make_timing_model", 2, "%s %s %s -> clock %s %s",
                     sta_->network()->pathName(input_pin),
                     input_rf->shortName(),
                     min_max == MinMax::max() ? "setup" : "hold",
                     clk_edge->name(),
                     delayAsString(margin, sta_));
          ScaleFactorType scale_type = setup
            ? ScaleFactorType::setup
            : ScaleFactorType::hold;
          TimingModel *check_model = makeScalarCheckModel(margin, scale_type, input_rf);
          if (attrs == nullptr)
            attrs = std::make_shared<TimingArcAttrs>();
          attrs->setModel(input_rf, check_model);
        }
      }
      if (attrs) {
        LibertyPort *input_port = modelPort(input_pin);
        for (const Pin *clk_pin : clk_edge->clock()->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          if (clk_port) {
            RiseFall *clk_rf = clk_edge->transition();
            TimingRole *role = setup ? TimingRole::setup() : TimingRole::hold();
            lib_builder_->makeFromTransitionArcs(cell_, clk_port,
                                                 input_port, nullptr,
                                                 clk_rf, role, attrs);
          }
        }
      }
    }
  }
}

void
MakeTimingModel::makeInputOutputTimingArcs(const Pin *input_pin,
                                           OutputPinDelays &output_pin_delays)
{
  for (auto out_pin_delay : output_pin_delays) {
    const Pin *output_pin = out_pin_delay.first;
    OutputDelays &output_delays = out_pin_delay.second;
    TimingArcAttrsPtr attrs = nullptr;
    for (RiseFall *output_rf : RiseFall::range()) {
      MinMax *min_max = MinMax::max();
      float delay;
      bool exists;
      output_delays.delays.value(output_rf, min_max, delay, exists);
      if (exists) {
        debugPrint(debug_, "make_timing_model", 2, "%s -> %s %s delay %s",
                   network_->pathName(input_pin),
                   network_->pathName(output_pin),
                   output_rf->shortName(),
                   delayAsString(delay, sta_));
        TimingModel *gate_model = makeGateModelTable(output_pin, delay, output_rf);
        if (attrs == nullptr)
          attrs = std::make_shared<TimingArcAttrs>();
        attrs->setModel(output_rf, gate_model);
      }
    }
    if (attrs) {
      LibertyPort *output_port = modelPort(output_pin);
      LibertyPort *input_port = modelPort(input_pin);
      attrs->setTimingSense(output_delays.timingSense());
      lib_builder_->makeCombinationalArcs(cell_, input_port,
                                          output_port, nullptr,
                                          true, true, attrs);
    }
  }
}

////////////////////////////////////////////////////////////////

// clocked register -> output paths
void
MakeTimingModel::findClkedOutputPaths()
{
  InstancePinIterator *output_iter = network_->pinIterator(network_->topInstance());
  while (output_iter->hasNext()) {
    Pin *output_pin = output_iter->next();    
    if (network_->direction(output_pin)->isOutput()) {
      ClockEdgeDelays clk_delays;
      LibertyPort *output_port = modelPort(output_pin);
      Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
      VertexPathIterator path_iter(output_vertex, this);
      while (path_iter.hasNext()) {
        PathVertex *path = path_iter.next();
        const ClockEdge *clk_edge = path->clkEdge(sta_);
        if (clk_edge) {
          const RiseFall *output_rf = path->transition(sta_);
          const MinMax *min_max = path->minMax(sta_);
          Arrival delay = path->arrival(sta_);
          RiseFallMinMax &delays = clk_delays[clk_edge];
          delays.mergeValue(output_rf, min_max,
                            delayAsFloat(delay, min_max, sta_));
        }
      }
      for (auto clk_edge_delay : clk_delays) {
        const ClockEdge *clk_edge = clk_edge_delay.first;
        RiseFallMinMax &delays = clk_edge_delay.second;
        for (const Pin *clk_pin : clk_edge->clock()->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          if (clk_port) {
            RiseFall *clk_rf = clk_edge->transition();
            TimingArcAttrsPtr attrs = nullptr;
            for (RiseFall *output_rf : RiseFall::range()) {
              float delay = delays.value(output_rf, min_max_) - clk_edge->time();
              TimingModel *gate_model = makeGateModelTable(output_pin, delay, output_rf);
              if (attrs == nullptr)
                attrs = std::make_shared<TimingArcAttrs>();
              attrs->setModel(output_rf, gate_model);
            }
            if (attrs) {
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
  delete output_iter;
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
  TablePtr table = make_shared<Table0>(value);
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *table_model = new TableModel(table, tbl_template,
                                           scale_factor_type, rf);
  CheckTableModel *check_model = new CheckTableModel(table_model, nullptr);
  return check_model;
}

TimingModel *
MakeTimingModel::makeGateModelScalar(Delay delay,
                                     Slew slew,
                                     RiseFall *rf)
{
  TablePtr delay_table = make_shared<Table0>(delayAsFloat(delay));
  TablePtr slew_table = make_shared<Table0>(delayAsFloat(slew));
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *delay_model = new TableModel(delay_table, tbl_template,
                                           ScaleFactorType::cell, rf);
  TableModel *slew_model = new TableModel(slew_table, tbl_template,
                                          ScaleFactorType::cell, rf);
  GateTableModel *gate_model = new GateTableModel(delay_model, nullptr,
                                                  slew_model, nullptr,
                                                  nullptr, nullptr);
  return gate_model;
}

// Eval the driver pin model along its load capacitance
// axis and add the input to output 'delay' to the table values.
TimingModel *
MakeTimingModel::makeGateModelTable(const Pin *output_pin,
                                    Delay delay,
                                    RiseFall *rf)
{
  const DcalcAnalysisPt *dcalc_ap = corner_->findDcalcAnalysisPt(min_max_);
  const Pvt *pvt = dcalc_ap->operatingConditions();
  const OperatingConditions *op_cond = dcalc_ap->operatingConditions();
  int lib_index = dcalc_ap->libertyIndex();

  PinSet *drvrs = network_->drivers(network_->net(network_->term(output_pin)));
  const Pin *drvr_pin = *drvrs->begin();
  const LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  if (drvr_port) {
    const LibertyCell *drvr_cell = drvr_port->libertyCell();
    for (TimingArcSet *arc_set : drvr_cell->timingArcSets(nullptr, drvr_port)) {
      for (TimingArc *drvr_arc : arc_set->arcs()) {
        // Use the first timing arc to simplify life.
        if (drvr_arc->toEdge()->asRiseFall() == rf) {
          const LibertyPort *gate_in_port = drvr_arc->from();
          const Instance *drvr_inst = network_->instance(drvr_pin);
          const Pin *gate_in_pin = network_->findPin(drvr_inst, gate_in_port);
          if (gate_in_pin) {
            Vertex *gate_in_vertex = graph_->pinLoadVertex(gate_in_pin);
            Slew in_slew = graph_->slew(gate_in_vertex,
                                        drvr_arc->fromEdge()->asRiseFall(),
                                        dcalc_ap->index());
            float in_slew1 = delayAsFloat(in_slew);
            TimingModel *drvr_model = drvr_arc->cornerArc(lib_index)->model(op_cond);
            GateTableModel *drvr_gate_model = dynamic_cast<GateTableModel*>(drvr_model);
            if (drvr_gate_model) {
              float output_load_cap = graph_delay_calc_->loadCap(output_pin, dcalc_ap);
              ArcDelay drvr_self_delay;
              Slew drvr_self_slew;
              drvr_gate_model->gateDelay(drvr_cell, pvt, in_slew1,
                                         output_load_cap, 0.0, false,
                                         drvr_self_delay, drvr_self_slew);

              const TableModel *drvr_table = drvr_gate_model->delayModel();
              const TableTemplate *drvr_template = drvr_table->tblTemplate();
              const TableAxisPtr drvr_load_axis = loadCapacitanceAxis(drvr_table);
              if (drvr_load_axis) {
                const FloatSeq *drvr_axis_values = drvr_load_axis->values();
                FloatSeq *load_values = new FloatSeq;
                FloatSeq *slew_values = new FloatSeq;
                for (size_t i = 0; i < drvr_axis_values->size(); i++) {
                  float load_cap = (*drvr_axis_values)[i];
                  // get slew from driver input pin
                  ArcDelay gate_delay;
                  Slew gate_slew;
                  drvr_gate_model->gateDelay(drvr_cell, pvt, in_slew1,
                                             load_cap, 0.0, false,
                                             gate_delay, gate_slew);
                  // Remove the self delay driving the output pin net load cap.
                  load_values->push_back(delayAsFloat(delay + gate_delay
                                                      - drvr_self_delay));
                  slew_values->push_back(delayAsFloat(gate_slew));
                }

                FloatSeq *axis_values = new FloatSeq(*drvr_axis_values);
                TableAxisPtr load_axis =
                  std::make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance,
                                              axis_values);

                TablePtr delay_table = make_shared<Table1>(load_values, load_axis);
                TablePtr slew_table = make_shared<Table1>(slew_values, load_axis);

                TableTemplate *model_template = ensureTableTemplate(drvr_template,
                                                                    load_axis);
                TableModel *delay_model = new TableModel(delay_table, model_template,
                                                         ScaleFactorType::cell, rf);
                TableModel *slew_model = new TableModel(slew_table, model_template,
                                                        ScaleFactorType::cell, rf);
                GateTableModel *gate_model = new GateTableModel(delay_model, nullptr,
                                                                slew_model, nullptr,
                                                                nullptr, nullptr);
                return gate_model;
              }
            }
          }
        }
      }
    }
  }
  Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
  Slew slew = graph_->slew(output_vertex, rf, dcalc_ap->index());
  return makeGateModelScalar(delay, slew, rf);
}

TableTemplate *
MakeTimingModel::ensureTableTemplate(const TableTemplate *drvr_template,
                                     TableAxisPtr load_axis)
{
  TableTemplate *model_template = template_map_.findKey(drvr_template);
  if (model_template == nullptr) {
    string template_name = "template_";
    template_name += std::to_string(tbl_template_index_++);

    model_template = new TableTemplate(template_name.c_str());
    model_template->setAxis1(load_axis);
    library_->addTableTemplate(model_template, TableTemplateType::delay);
    template_map_[drvr_template] = model_template;
  }
  return model_template;
}

TableAxisPtr
MakeTimingModel::loadCapacitanceAxis(const TableModel *table)
{
  if (table->axis1()
      && table->axis1()->variable() == TableAxisVariable::total_output_net_capacitance)
    return table->axis1();
  else if (table->axis2()
           && table->axis2()->variable() == TableAxisVariable::total_output_net_capacitance)
    return table->axis2();
  else if (table->axis3()
           && table->axis3()->variable() == TableAxisVariable::total_output_net_capacitance)
    return table->axis3();
  else
    return nullptr;
}

OutputDelays::OutputDelays()
{
  rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()] = false;
  rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()] = false;
  rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()] = false;
  rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()] = false;
}

TimingSense
OutputDelays::timingSense() const
{
  if (rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
           && rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()]
           && !rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
           && !rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()])
    return TimingSense::positive_unate;
  else if (rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
           && rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()]
           && !rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
           && !rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()])
    return TimingSense::negative_unate;
  else if (rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
           || rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
           || rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()]
           || rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()])
    return TimingSense::non_unate;
  else
    return TimingSense::none;
}

} // namespace
