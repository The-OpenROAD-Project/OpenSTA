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

#include "ReportParasiticAnnotation.hh"

#include "Report.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "PortDirection.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "Parasitics.hh"
#include "DcalcAnalysisPt.hh"
#include "ArcDelayCalc.hh"

namespace sta {

class ReportParasiticAnnotation : public StaState
{
public:
  ReportParasiticAnnotation(bool report_unannotated,
                            const Corner *corner,
                            StaState *sta);
  void report();

private:
  void reportAnnotationCounts();
  void findCounts();
  void findCounts(Instance *inst);
  void findCounts(Net *net);

  bool report_unannotated_;
  const Corner *corner_;
  const MinMax *min_max_;
  const ParasiticAnalysisPt *parasitic_ap_;
  PinSeq unannotated_;
  PinSeq partially_annotated_;
};

void
reportParasiticAnnotation(bool report_unannotated,
                          const Corner *corner,
                          StaState *sta)
{
  ReportParasiticAnnotation report_annotation(report_unannotated, corner, sta);
  report_annotation.report();
}

ReportParasiticAnnotation::ReportParasiticAnnotation(bool report_unannotated,
                                                     const Corner *corner,
                                                     StaState *sta) :
  StaState(sta),
  report_unannotated_(report_unannotated),
  corner_(corner),
  min_max_(MinMax::max()),
  parasitic_ap_(corner_->findParasiticAnalysisPt(min_max_))
{
}

void
ReportParasiticAnnotation::report()
{
  findCounts();
  reportAnnotationCounts();
}

void
ReportParasiticAnnotation::reportAnnotationCounts()
{
  report_->reportLine("Found %zu unannotated drivers.", unannotated_.size());
  if (report_unannotated_) {
    sort(unannotated_, PinPathNameLess(network_));
    for (const Pin *drvr_pin : unannotated_)
      report_->reportLine(" %s", network_->pathName(drvr_pin));
  }

  report_->reportLine("Found %zu partially unannotated drivers.",
                      partially_annotated_.size());
  if (report_unannotated_) {
    sort(partially_annotated_, PinPathNameLess(network_));
    for (const Pin *drvr_pin : partially_annotated_) {
      report_->reportLine(" %s", network_->pathName(drvr_pin));

      Parasitic *parasitic = parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap_);
      if (parasitic) {
        PinSet unannotated_loads = parasitics_->unannotatedLoads(parasitic, drvr_pin);
        for (const Pin *load_pin : unannotated_loads)
          report_->reportLine("  %s", network_->pathName(load_pin));
      }
    }
  }
}

void
ReportParasiticAnnotation::findCounts()
{
  DcalcAnalysisPt *dcalc_ap = corner_->findDcalcAnalysisPt(min_max_);
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    Pin *pin = vertex->pin();
    PortDirection *dir = network_->direction(pin);
    if (vertex->isDriver(network_)
        && !dir->isInternal()) {
      Parasitic *parasitic = parasitics_->findParasiticNetwork(pin, parasitic_ap_);
      if (parasitic == nullptr)
        parasitic = arc_delay_calc_->findParasitic(pin, RiseFall::rise(), dcalc_ap);
      if (parasitic) {
        PinSet unannotated_loads = parasitics_->unannotatedLoads(parasitic, pin);
        if (unannotated_loads.size() > 0)
          partially_annotated_.push_back(pin);
      }
      else 
        unannotated_.push_back(pin);
    }
  }
}

} // namespace
