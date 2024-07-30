// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#include "CcsCeffDelayCalc.hh"

#include "Debug.hh"
#include "Units.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "Parasitics.hh"
#include "GraphDelayCalc.hh"
#include "DmpDelayCalc.hh"
#include "FindRoot.hh"

namespace sta {

// Implementaion based on:
// "Gate Delay Estimation with Library Compatible Current Source Models
// and Effective Capacitance", D. Garyfallou et al,
// IEEE Transactions on Very Large Scale Integration (VLSI) Systems, March 2021

using std::abs;
using std::exp;
using std::make_shared;

ArcDelayCalc *
makeCcsCeffDelayCalc(StaState *sta)
{
  return new CcsCeffDelayCalc(sta);
}

CcsCeffDelayCalc::CcsCeffDelayCalc(StaState *sta) :
  LumpedCapDelayCalc(sta),
  output_waveforms_(nullptr),
  // Includes the Vh:Vdd region.
  region_count_(0),
  vl_fail_(false),
  watch_pin_values_(network_),
  capacitance_unit_(units_->capacitanceUnit()),
  table_dcalc_(makeDmpCeffElmoreDelayCalc(sta))
{
}

CcsCeffDelayCalc::~CcsCeffDelayCalc()
{
  delete table_dcalc_;
}

ArcDelayCalc *
CcsCeffDelayCalc::copy()
{
  return new CcsCeffDelayCalc(this);
}

ArcDcalcResult
CcsCeffDelayCalc::gateDelay(const Pin *drvr_pin,
                            const TimingArc *arc,
                            const Slew &in_slew,
                            float load_cap,
                            const Parasitic *parasitic,
                            const LoadPinIndexMap &load_pin_index_map,
                            const DcalcAnalysisPt *dcalc_ap)
{
  in_slew_ = delayAsFloat(in_slew);
  load_cap_ = load_cap;
  parasitic_ = parasitic;
  output_waveforms_ = nullptr;

  GateTableModel *table_model = arc->gateTableModel(dcalc_ap);
  if (table_model && parasitic) {
    OutputWaveforms *output_waveforms = table_model->outputWaveforms();
    parasitics_->piModel(parasitic, c2_, rpi_, c1_);
    if (output_waveforms
        && rpi_ > 0.0 && c1_ > 0.0
        // Bounds check because extrapolating waveforms does not work for shit.
        && output_waveforms->slewAxis()->inBounds(in_slew_)
        && output_waveforms->capAxis()->inBounds(c2_)
        && output_waveforms->capAxis()->inBounds(load_cap_)) {
      bool vdd_exists;
      LibertyCell *drvr_cell = arc->to()->libertyCell();
      const LibertyLibrary *drvr_library = drvr_cell->libertyLibrary();
      drvr_rf_ = arc->toEdge()->asRiseFall();
      drvr_library->supplyVoltage("VDD", vdd_, vdd_exists);
      if (!vdd_exists)
        report_->error(1700, "VDD not defined in library %s", drvr_library->name());
      vth_ = drvr_library->outputThreshold(drvr_rf_) * vdd_;
      vl_ = drvr_library->slewLowerThreshold(drvr_rf_) * vdd_;
      vh_ = drvr_library->slewUpperThreshold(drvr_rf_) * vdd_;

      const DcalcAnalysisPtSeq &dcalc_aps = corners_->dcalcAnalysisPts();
      drvr_cell->ensureVoltageWaveforms(dcalc_aps);
      in_slew_ = delayAsFloat(in_slew);
      output_waveforms_ = output_waveforms;
      ref_time_ = output_waveforms_->referenceTime(in_slew_);
      debugPrint(debug_, "ccs_dcalc", 1, "%s %s",
                 drvr_cell->name(),
                 drvr_rf_->asString());
      ArcDelay gate_delay;
      Slew drvr_slew;
      gateDelaySlew(drvr_library, drvr_rf_, gate_delay, drvr_slew);
      return makeResult(drvr_library,drvr_rf_,gate_delay,drvr_slew,load_pin_index_map);
    }
  }
  return table_dcalc_->gateDelay(drvr_pin, arc, in_slew, load_cap, parasitic,
                                 load_pin_index_map, dcalc_ap);
}

void
CcsCeffDelayCalc::gateDelaySlew(const LibertyLibrary *drvr_library,
                                const RiseFall *rf,
                                // Return values.
                                ArcDelay &gate_delay,
                                Slew &drvr_slew)
{    
  initRegions(drvr_library, rf);
  findCsmWaveform();
  ref_time_ = output_waveforms_->referenceTime(in_slew_);
  gate_delay = region_times_[region_vth_idx_] - ref_time_;
  drvr_slew = abs(region_times_[region_vh_idx_] - region_times_[region_vl_idx_]);
  debugPrint(debug_, "ccs_dcalc", 2,
             "gate_delay %s drvr_slew %s (initial)",
             delayAsString(gate_delay, this),
             delayAsString(drvr_slew, this));
  float prev_drvr_slew = delayAsFloat(drvr_slew);
  constexpr int max_iterations = 5;
  for (int iter = 0; iter < max_iterations; iter++) {
    debugPrint(debug_, "ccs_dcalc", 2, "iteration %d", iter);
    // Init drvr ramp model for vl.
    for (size_t i = 0; i <= region_count_; i++) {
      region_ramp_times_[i] = region_times_[i];
      if (i < region_count_)
        region_ramp_slopes_[i] = (region_volts_[i + 1] - region_volts_[i])
          / (region_times_[i + 1] - region_times_[i]);
    }

    for (size_t i = 0; i < region_count_; i++) {
      double v1 = region_volts_[i];
      double v2 = region_volts_[i + 1];
      double t1 = region_times_[i];
      double t2 = region_times_[i + 1];

      // Receiver cap Cp(l) assumed constant and included in c1.
      // Note that eqn 8 in the ref'd paper does not properly account
      // for the charge on c1 from previous segments so it does not
      // work well.
      double c1_v1, c1_v2, ignore;
      vl(t1, rpi_ * c1_, c1_v1, ignore);
      vl(t2, rpi_ * c1_, c1_v2, ignore);
      double q1 = v1 * c2_ + c1_v1 * c1_;
      double q2 = v2 * c2_ + c1_v2 * c1_;
      double ceff = (q2 - q1) / (v2 - v1);

      debugPrint(debug_, "ccs_dcalc", 2, "ceff %s",
                 capacitance_unit_->asString(ceff));
      region_ceff_[i] = ceff;
    }
    findCsmWaveform();
    gate_delay = region_times_[region_vth_idx_] - ref_time_;
    drvr_slew = abs(region_times_[region_vh_idx_] - region_times_[region_vl_idx_]);
    debugPrint(debug_, "ccs_dcalc", 2,
               "gate_delay %s drvr_slew %s",
               delayAsString(gate_delay, this),
               delayAsString(drvr_slew, this));
    if (abs(delayAsFloat(drvr_slew) - prev_drvr_slew) < .01 * prev_drvr_slew)
      break;
    prev_drvr_slew = delayAsFloat(drvr_slew);
  }
}

void
CcsCeffDelayCalc::initRegions(const LibertyLibrary *drvr_library,
                              const RiseFall *rf)
{
  // Falling waveforms are treated as rising to simplify the conditionals.
  if (rf == RiseFall::fall()) {
    vl_ = (1.0 - drvr_library->slewUpperThreshold(rf)) * vdd_;
    vh_ = (1.0 - drvr_library->slewLowerThreshold(rf)) * vdd_;
  }

  // Includes the Vh:Vdd region.
  region_count_ = 7;
  region_volts_.resize(region_count_ + 1);
  region_ceff_.resize(region_count_ + 1);
  region_times_.resize(region_count_ + 1);
  region_begin_times_.resize(region_count_ + 1);
  region_end_times_.resize(region_count_ + 1);
  region_time_offsets_.resize(region_count_ + 1);
  region_ramp_times_.resize(region_count_ + 1);
  region_ramp_slopes_.resize(region_count_ + 1);

  region_vl_idx_ = 1;
  region_vh_idx_ = region_count_ - 1;

  double vth_vh = (vh_ - vth_);
  switch (region_count_) {
  case 4:
    region_vth_idx_ = 2;
    region_volts_ = {0.0, vl_, vth_, vh_, vdd_};
    break;
  case 5: {
    region_vth_idx_ = 2;
    double v1 =  vth_ + .7 * vth_vh;
    region_volts_ = {0.0, vl_, vth_, v1, vh_, vdd_};
    break;
  }
  case 6: {
    region_vth_idx_ = 2;
    double v1 = vth_ + .3 * vth_vh;
    double v2 = vth_ + .6 * vth_vh;
    region_volts_ = {0.0, vl_, vth_, v1, v2, vh_, vdd_};
    break;
  }
  case 7: {
    region_vth_idx_ = 2;
    region_vh_idx_ = 5;
    double v1 = vth_ + .3 * vth_vh;
    double v2 = vth_ + .6 * vth_vh;
    double v3 = vh_ + .5 * (vdd_ - vh_);
    region_volts_ = {0.0, vl_, vth_, v1, v2, vh_, v3, vdd_};
    break;
  }
  case 8: {
    region_vth_idx_ = 2;
    region_vh_idx_ = 6;
    double v1 = vth_ + .25 * vth_vh;
    double v2 = vth_ + .50 * vth_vh;
    double v3 = vth_ + .75 * vth_vh;
    double v4 = vh_ + .5 * (vdd_ - vh_);
    region_volts_ = {0.0, vl_, vth_, v1, v2, v3, vh_, v4, vdd_};
    break;
  }
  case 9: {
    region_vth_idx_ = 2;
    region_vh_idx_ = 7;
    double v1 = vth_ + .2 * vth_vh;
    double v2 = vth_ + .4 * vth_vh;
    double v3 = vth_ + .6 * vth_vh;
    double v4 = vth_ + .8 * vth_vh;
    double v5 = vh_ + .5 * (vdd_ - vh_);
    region_volts_ = {0.0, vl_, vth_, v1, v2, v3, v4, vh_, v5, vdd_};
    break;
  }
  case 10: {
    region_vth_idx_ = 2;
    region_vh_idx_ = 7;
    double v1 = vth_ + .2 * vth_vh;
    double v2 = vth_ + .4 * vth_vh;
    double v3 = vth_ + .6 * vth_vh;
    double v4 = vth_ + .8 * vth_vh;
    double v5 = vh_ + .3 * (vdd_ - vh_);
    double v6 = vh_ + .6 * (vdd_ - vh_);
    region_volts_ = {0.0, vl_, vth_, v1, v2, v3, v4, vh_, v5, v6, vdd_};
    break;
  }
  default:
    report_->error(1701, "unsupported ccs region count.");
    break;
  }
  fill(region_ceff_.begin(), region_ceff_.end(), c2_ + c1_);
}

void
CcsCeffDelayCalc::findCsmWaveform()
{
  for (size_t i = 0; i < region_count_; i++) {
    double t1 = output_waveforms_->voltageTime(in_slew_, region_ceff_[i],
                                               region_volts_[i]);
    double t2 = output_waveforms_->voltageTime(in_slew_, region_ceff_[i],
                                               region_volts_[i + 1]);
    region_begin_times_[i] = t1;
    region_end_times_[i] = t2;
    double time_offset = (i == 0)
      ? 0.0
      : t1 - (region_end_times_[i - 1] - region_time_offsets_[i - 1]);
    region_time_offsets_[i] = time_offset;

    if (i == 0)
      region_times_[i] = t1 - time_offset;
    region_times_[i + 1] = t2 - time_offset;
  }
}

////////////////////////////////////////////////////////////////

ArcDcalcResult
CcsCeffDelayCalc::makeResult(const LibertyLibrary *drvr_library,
                             const RiseFall *rf,
                             ArcDelay &gate_delay,
                             Slew &drvr_slew,
                             const LoadPinIndexMap &load_pin_index_map)
{
  ArcDcalcResult dcalc_result(load_pin_index_map.size());
  debugPrint(debug_, "ccs_dcalc", 2,
             "gate_delay %s drvr_slew %s",
             delayAsString(gate_delay, this),
             delayAsString(drvr_slew, this));
  dcalc_result.setGateDelay(gate_delay);
  dcalc_result.setDrvrSlew(drvr_slew);

  for (const auto [load_pin, load_idx] : load_pin_index_map) {
    ArcDelay wire_delay;
    Slew load_slew;
    loadDelaySlew(load_pin, drvr_library, rf, drvr_slew, wire_delay, load_slew);
    dcalc_result.setWireDelay(load_idx, wire_delay);
    dcalc_result.setLoadSlew(load_idx, load_slew);
  }
  return dcalc_result;
}

void
CcsCeffDelayCalc::loadDelaySlew(const Pin *load_pin,
                                const LibertyLibrary *drvr_library,
                                const RiseFall *rf,
                                Slew &drvr_slew,
                                // Return values.
                                ArcDelay &wire_delay,
                                Slew &load_slew)
{
  ArcDelay wire_delay1 = 0.0;
  Slew load_slew1 = drvr_slew;
  bool elmore_exists = false;
  float elmore = 0.0;
  if (parasitic_
      && parasitics_->isPiElmore(parasitic_))
    parasitics_->findElmore(parasitic_, load_pin, elmore, elmore_exists);

  if (elmore_exists &&
      (elmore == 0.0
       // Elmore delay is small compared to driver slew.
       || elmore < delayAsFloat(drvr_slew) * 1e-3)) {
    wire_delay1 = elmore;
    load_slew1 = drvr_slew;
  }
  else
    loadDelaySlew(load_pin, drvr_slew, elmore, wire_delay1, load_slew1);

  thresholdAdjust(load_pin, drvr_library, rf, wire_delay, load_slew);
  wire_delay = wire_delay1;
  load_slew = load_slew1;
}

void
CcsCeffDelayCalc::loadDelaySlew(const Pin *load_pin,
                                Slew &drvr_slew,
                                float elmore,
                                // Return values.
                                ArcDelay &delay,
                                Slew &slew)
{
  for (size_t i = 0; i <= region_count_; i++) {
    region_ramp_times_[i] = region_times_[i];
    if (i < region_count_)
      region_ramp_slopes_[i] = (region_volts_[i + 1] - region_volts_[i])
        / (region_times_[i + 1] - region_times_[i]);
  }

  vl_fail_ = false;
  double t_vl = findVlTime(vl_, elmore);
  double t_vth = findVlTime(vth_, elmore);
  double t_vh = findVlTime(vh_, elmore);
  if (!vl_fail_) {
    delay = t_vth - region_times_[region_vth_idx_];
    slew = t_vh - t_vl;
  }
  else {
    delay = elmore;
    slew = drvr_slew;
    fail("load delay threshold crossing");
  }
  debugPrint(debug_, "ccs_dcalc", 2,
             "load %s delay %s slew %s",
             network_->pathName(load_pin),
             delayAsString(delay, this),
             delayAsString(slew, this));
}

// Elmore (one pole) response to ramp with slope slew.
static void
rampElmoreV(double t,
            double slew,
            double elmore,
            // Return values.
            double &v,
            double &dv_dt)
{
  double exp_t = 1.0 - exp2(-t / elmore);
  v = slew * (t - elmore * exp_t);
  // First derivative of loadVl dv/dt.
  dv_dt = slew * exp_t;
}

// Elmore (one pole) response to 2 segment ramps [0, vth] slew1, [vth, vdd] slew2.
void
CcsCeffDelayCalc::vl(double t,
                     double elmore,
                     // Return values.
                     double &vl,
                     double &dvl_dt)
{
  vl = 0.0;
  dvl_dt = 0.0;
  for (size_t i = 0; i < region_count_; i++) {
    double t_begin = region_ramp_times_[i];
    double t_end = region_ramp_times_[i + 1];
    double ramp_slope = region_ramp_slopes_[i];
    if (t >= t_begin) {
      double v, dv_dt;
      rampElmoreV(t - t_begin, ramp_slope, elmore, v, dv_dt);
      vl += v;
      dvl_dt += dv_dt;
    }
    if (t > t_end) {
      double v, dv_dt;
      rampElmoreV(t - t_end, ramp_slope, elmore, v, dv_dt);
      vl -= v;
      dvl_dt -= dv_dt;
    }
  }
}

// for debugging
double
CcsCeffDelayCalc::vl(double t,
                     double elmore)
{
  double vl1, dvl_dt;
  vl(t, elmore, vl1, dvl_dt);
  return vl1;
}

double
CcsCeffDelayCalc::findVlTime(double v,
                             double elmore)
{
  double t_init = region_ramp_times_[0];
  double t_final = region_ramp_times_[region_count_];
  bool root_fail = false;
  double time = findRoot([=] (double t,
                             double &y,
                             double &dy) {
    vl(t, elmore, y, dy);
    y -= v;
  }, t_init, t_final + elmore * 3.0, .001, 20, root_fail);
  vl_fail_ |= root_fail;
  return time;
}

////////////////////////////////////////////////////////////////

// Waveform accessors for swig/tcl.

void
CcsCeffDelayCalc::watchPin(const Pin *pin)
{
  watch_pin_values_[pin] = FloatSeq();
}

void
CcsCeffDelayCalc::clearWatchPins()
{
  watch_pin_values_.clear();
}

PinSeq
CcsCeffDelayCalc::watchPins() const
{
  PinSeq pins;
  for (const auto& [pin, values] : watch_pin_values_)
    pins.push_back(pin);
  return pins;
}

Waveform
CcsCeffDelayCalc::watchWaveform(const Pin *pin)
{
  if (pin == drvr_pin_)
    return drvrWaveform();
  else
    return loadWaveform(pin);
}

Waveform
CcsCeffDelayCalc::drvrWaveform()
{
  if (output_waveforms_) {
    // Stitch together the ccs waveforms for each region.
    FloatSeq *drvr_times = new FloatSeq;
    FloatSeq *drvr_volts = new FloatSeq;
    for (size_t i = 0; i < region_count_; i++) {
      double t1 = region_begin_times_[i];
      double t2 = region_end_times_[i];
      size_t time_steps = 10;
      double time_step = (t2 - t1) / time_steps;
      double time_offset = region_time_offsets_[i];
      for (size_t s = 0; s <= time_steps; s++) {
        double t = t1 + s * time_step;
        drvr_times->push_back(t - time_offset);
        double v = output_waveforms_->timeVoltage(in_slew_, region_ceff_[i], t);
        if (drvr_rf_ == RiseFall::fall())
          v = vdd_ - v;
        drvr_volts->push_back(v);
      }
    }
    TableAxisPtr drvr_time_axis = make_shared<TableAxis>(TableAxisVariable::time,
                                                         drvr_times);
    Table1 drvr_table(drvr_volts, drvr_time_axis);
    return drvr_table;
  }
  else
    return Table1();
}

Waveform
CcsCeffDelayCalc::loadWaveform(const Pin *load_pin)
{
  if (output_waveforms_) {
    bool elmore_exists = false;
    float elmore = 0.0;
    parasitics_->findElmore(parasitic_, load_pin, elmore, elmore_exists);
    if (elmore_exists) {
      FloatSeq *load_times = new FloatSeq;
      FloatSeq *load_volts = new FloatSeq;
      double t_vh = findVlTime(vh_, elmore);
      double dt = t_vh / 20.0;
      double v_final = vh_ + (vdd_ - vh_) * .8;
      double v = 0.0;
      for (double t = 0; v < v_final; t += dt) {
        load_times->push_back(t);

        double ignore;
        vl(t, elmore, v, ignore);
        double v1 = (drvr_rf_ == RiseFall::rise()) ? v : vdd_ - v;
        load_volts->push_back(v1);
      }
      TableAxisPtr load_time_axis = make_shared<TableAxis>(TableAxisVariable::time,
                                                           load_times);
      Table1 load_table(load_volts, load_time_axis);
      return load_table;
    }
  }
  return Table1();
}

Waveform
CcsCeffDelayCalc::drvrRampWaveform(const Pin *in_pin,
                                   const RiseFall *in_rf,
                                   const Pin *drvr_pin,
                                   const RiseFall *drvr_rf,
                                   const Pin *load_pin,
                                   const Corner *corner,
                                   const MinMax *min_max)
{
  bool elmore_exists = false;
  float elmore = 0.0;
  if (parasitic_) {
    parasitics_->findElmore(parasitic_, load_pin, elmore, elmore_exists);
    bool dcalc_success = makeWaveformPreamble(in_pin, in_rf, drvr_pin,
                                              drvr_rf, corner, min_max);
    if (dcalc_success
        && elmore_exists) {
      FloatSeq *load_times = new FloatSeq;
      FloatSeq *load_volts = new FloatSeq;
      for (size_t j = 0; j <= region_count_; j++) {
        double t = region_ramp_times_[j];
        load_times->push_back(t);
        double v = 0.0;
        for (size_t i = 0; i < region_count_; i++) {
          double t_begin = region_ramp_times_[i];
          double t_end = region_ramp_times_[i + 1];
          double ramp_slope = region_ramp_slopes_[i];
          if (t >= t_begin)
            v += (t - t_begin) * ramp_slope;
          if (t > t_end)
            v -= (t - t_end) * ramp_slope;
        }
        double v1 = (drvr_rf == RiseFall::rise()) ? v : vdd_ - v;
        load_volts->push_back(v1);
      }
      TableAxisPtr load_time_axis = make_shared<TableAxis>(TableAxisVariable::time,
                                                           load_times);
      Table1 load_table(load_volts, load_time_axis);
      return load_table;
    }
  }
  return Table1();
}

bool
CcsCeffDelayCalc::makeWaveformPreamble(const Pin *in_pin,
                                       const RiseFall *in_rf,
                                       const Pin *drvr_pin,
                                       const RiseFall *drvr_rf,
                                       const Corner *corner,
                                       const MinMax *min_max)
{
  Vertex *in_vertex = graph_->pinLoadVertex(in_pin);
  Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
  Edge *edge = nullptr;
  VertexInEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    edge = edge_iter.next();
    Vertex *from_vertex = edge->from(graph_);
    const Pin *from_pin = from_vertex->pin();
    if (from_pin == in_pin)
      break;
  }
  if (edge) {
    TimingArc *arc = nullptr;  
    for (TimingArc *arc1 : edge->timingArcSet()->arcs()) {
      if (arc1->fromEdge()->asRiseFall() == in_rf
          && arc1->toEdge()->asRiseFall() == drvr_rf) {
        arc = arc1;
        break;
      }
    }
    if (arc) {
      DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
      const Slew &in_slew = graph_->slew(in_vertex, in_rf, dcalc_ap->index());
      parasitic_ = arc_delay_calc_->findParasitic(drvr_pin, drvr_rf, dcalc_ap);
      if (parasitic_) {
        parasitics_->piModel(parasitic_, c2_, rpi_, c1_);
        LoadPinIndexMap load_pin_index_map =
          graph_delay_calc_->makeLoadPinIndexMap(drvr_vertex);
        gateDelay(drvr_pin, arc, in_slew, load_cap_, parasitic_,
                  load_pin_index_map, dcalc_ap);
        return true;
      }
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////

string
CcsCeffDelayCalc::reportGateDelay(const Pin *drvr_pin,
                                  const TimingArc *arc,
                                  const Slew &in_slew,
                                  float load_cap,
                                  const Parasitic *parasitic,
                                  const LoadPinIndexMap &load_pin_index_map,
                                  const DcalcAnalysisPt *dcalc_ap,
                                  int digits)
{
  Parasitic *pi_elmore = nullptr;
  const RiseFall *rf = arc->toEdge()->asRiseFall();
  if (parasitic && !parasitics_->isPiElmore(parasitic)) {
    const ParasiticAnalysisPt *ap = dcalc_ap->parasiticAnalysisPt();
    pi_elmore = parasitics_->reduceToPiElmore(parasitic, drvr_pin_, rf,
                                               dcalc_ap->corner(),
                                               dcalc_ap->constraintMinMax(), ap);
  }
  string report = table_dcalc_->reportGateDelay(drvr_pin, arc, in_slew, load_cap,
                                                pi_elmore, load_pin_index_map,
                                                dcalc_ap, digits);
  parasitics_->deleteDrvrReducedParasitics(drvr_pin);
  return report;
}

void
CcsCeffDelayCalc::fail(const char *reason)
{
  // Report failures with a unique debug flag.
  if (debug_->check("ccs_dcalc", 1) || debug_->check("dcalc_error", 1))
    report_->reportLine("delay_calc: CCS failed - %s", reason);
}

} // namespace
