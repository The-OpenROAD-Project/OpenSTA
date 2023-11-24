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

#include "ParallelDelayCalc.hh"

#include "TimingArc.hh"
#include "Corner.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Liberty.hh"
#include "GraphDelayCalc.hh"

namespace sta {

ParallelDelayCalc::ParallelDelayCalc(StaState *sta):
  DelayCalcBase(sta)
{
}

void
ParallelDelayCalc::inputPortDelay(const Pin *drvr_pin,
                                  float in_slew,
                                  const RiseFall *rf,
                                  const Parasitic *parasitic,
                                  const DcalcAnalysisPt *dcalc_ap)
{
  DelayCalcBase::inputPortDelay(drvr_pin, in_slew, rf, parasitic, dcalc_ap);
  multi_drvr_slew_factor_ = 1.0;
}

void
ParallelDelayCalc::gateDelayInit(const TimingArc *arc,
                                const Slew &in_slew,
                                const Parasitic *drvr_parasitic)
{
  DelayCalcBase::gateDelayInit(arc, in_slew, drvr_parasitic);
  multi_drvr_slew_factor_ = 1.0F;
}

void
ParallelDelayCalc::findParallelGateDelays(const MultiDrvrNet *multi_drvr,
                                          GraphDelayCalc *dcalc)
{
  int count = RiseFall::index_count * corners_->dcalcAnalysisPtCount();
  parallel_delays_.resize(count);
  parallel_slews_.resize(count);
  for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
    for (auto drvr_rf : RiseFall::range()) {
      DcalcAPIndex ap_index = dcalc_ap->index();
      int drvr_rf_index = drvr_rf->index();
      int index = ap_index * RiseFall::index_count + drvr_rf_index;
      findMultiDrvrGateDelay(multi_drvr, drvr_rf, dcalc_ap, dcalc,
                             parallel_delays_[index],
                             parallel_slews_[index]);
    }
  }
}

void
ParallelDelayCalc::findMultiDrvrGateDelay(const MultiDrvrNet *multi_drvr,
                                          const RiseFall *drvr_rf,
                                          const DcalcAnalysisPt *dcalc_ap,
                                          GraphDelayCalc *dcalc,
                                          // Return values.
                                          ArcDelay &parallel_delay,
                                          Slew &parallel_slew)
{
  ArcDelay delay_sum = 0.0;
  Slew slew_sum = 0.0;
  for (Vertex *drvr_vertex : *multi_drvr->drvrs()) {
    Pin *drvr_pin = drvr_vertex->pin();
    Instance *drvr_inst = network_->instance(drvr_pin);
    const Pvt *pvt = sdc_->pvt(drvr_inst, dcalc_ap->constraintMinMax());
    if (pvt == nullptr)
      pvt = dcalc_ap->operatingConditions();
    VertexInEdgeIterator edge_iter(drvr_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      TimingArcSet *arc_set = edge->timingArcSet();
      const LibertyPort *related_out_port = arc_set->relatedOut();
      for (TimingArc *arc : arc_set->arcs()) {
        RiseFall *arc_rf = arc->toEdge()->asRiseFall();
        if (arc_rf == drvr_rf) {
          Vertex *from_vertex = edge->from(graph_);
          RiseFall *from_rf = arc->fromEdge()->asRiseFall();
          Slew from_slew = dcalc->edgeFromSlew(from_vertex, from_rf,
                                               edge, dcalc_ap);
          ArcDelay intrinsic_delay;
          Slew intrinsic_slew;
          gateDelay(arc, from_slew, 0.0, 0, 0.0, pvt, dcalc_ap,
                    intrinsic_delay, intrinsic_slew);
          Parasitic *parasitic = findParasitic(drvr_pin, drvr_rf, dcalc_ap);
          const Pin *related_out_pin = 0;
          float related_out_cap = 0.0;
          if (related_out_port) {
            Instance *inst = network_->instance(drvr_pin);
            related_out_pin = network_->findPin(inst, related_out_port);
            if (related_out_pin) {
              Parasitic *related_out_parasitic = findParasitic(related_out_pin,
                                                               drvr_rf,
                                                               dcalc_ap);
              related_out_cap = dcalc->loadCap(related_out_pin,
                                               related_out_parasitic,
                                               drvr_rf, dcalc_ap);
            }
          }
          float load_cap = dcalc->loadCap(drvr_pin, parasitic,
                                          drvr_rf, dcalc_ap);
          ArcDelay gate_delay;
          Slew gate_slew;
          gateDelay(arc, from_slew, load_cap, parasitic,
                    related_out_cap, pvt, dcalc_ap,
                    gate_delay, gate_slew);
          delay_sum += 1.0F / (gate_delay - intrinsic_delay);
          slew_sum += 1.0F / gate_slew;
        }
      }
    }
  }
  parallel_delay = 1.0F / delay_sum;
  parallel_slew = 1.0F / slew_sum;
}

void
ParallelDelayCalc::parallelGateDelay(const Pin *,
                                     const TimingArc *arc,
                                     const Slew &from_slew,
                                     float load_cap,
                                     const Parasitic *drvr_parasitic,
                                     float related_out_cap,
                                     const Pvt *pvt,
                                     const DcalcAnalysisPt *dcalc_ap,
                                     // Return values.
                                     ArcDelay &gate_delay,
                                     Slew &gate_slew)
{
  ArcDelay intrinsic_delay;
  Slew intrinsic_slew;
  gateDelay(arc, from_slew, 0.0, 0, 0.0, pvt, dcalc_ap,
            intrinsic_delay, intrinsic_slew);
  const RiseFall *drvr_rf = arc->toEdge()->asRiseFall();
  int index = dcalc_ap->index() * RiseFall::index_count + drvr_rf->index();
  ArcDelay parallel_delay = parallel_delays_[index];
  Slew parallel_slew = parallel_slews_[index];
  gate_delay = parallel_delay + intrinsic_delay;
  gate_slew = parallel_slew;

  Delay gate_delay1;
  Slew gate_slew1;
  gateDelay(arc, from_slew, load_cap, drvr_parasitic,
            related_out_cap, pvt, dcalc_ap,
            gate_delay1, gate_slew1);
  float factor = delayRatio(gate_slew, gate_slew1);
  multi_drvr_slew_factor_ = factor;
}

} // namespace
