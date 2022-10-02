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

#include "ReportParasiticAnnotation.hh"

#include "Report.hh"
#include "Network.hh"
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
  const DcalcAnalysisPt *dcalc_ap_;
  const ParasiticAnalysisPt *parasitics_ap_;
  NetSet unannotated_nets_;
  VertexSet partially_annotated_drvrs_;
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
  dcalc_ap_(corner_->findDcalcAnalysisPt(min_max_)),
  parasitics_ap_(corner_->findParasiticAnalysisPt(min_max_)),
  partially_annotated_drvrs_(graph_)
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
  report_->reportLine("Found %lu unannotated nets.", unannotated_nets_.size());
  if (report_unannotated_) {
    for (const Net *net : unannotated_nets_)
      report_->reportLine(" %s", network_->pathName(net));
  }

  report_->reportLine("Found %lu partially unannotated nets.",
                      partially_annotated_drvrs_.size());
  if (report_unannotated_) {
    const RiseFall *rf = RiseFall::rise();
    for (Vertex *vertex : partially_annotated_drvrs_) {
      Pin *drvr_pin = vertex->pin();
      Net *net = network_->isTopLevelPort(drvr_pin)
        ? network_->net(network_->term(drvr_pin))
        : network_->net(drvr_pin);
      report_->reportLine(" %s", network_->pathName(net));

      Parasitic *parasitic = arc_delay_calc_->findParasitic(drvr_pin, rf, dcalc_ap_);
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        Vertex *load_vertex = edge->to(graph_);
        Pin *load_pin = load_vertex->pin();
        bool elmore_exists = false;
        float elmore = 0.0;
        parasitics_->findElmore(parasitic, load_pin, elmore, elmore_exists);
        if (!elmore_exists)
          report_->reportLine("  %s", network_->pathName(load_pin));
      }
    }
  }
}

void
ReportParasiticAnnotation::findCounts()
{
  const RiseFall *rf = RiseFall::rise();
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (vertex->isDriver(network_)) {
      Pin *drvr_pin = vertex->pin();
      Parasitic *parasitic = arc_delay_calc_->findParasitic(drvr_pin, rf, dcalc_ap_);
      if (parasitic) {
        VertexOutEdgeIterator edge_iter(vertex, graph_);
        while (edge_iter.hasNext()) {
          Edge *edge = edge_iter.next();
          Vertex *load_vertex = edge->to(graph_);
          Pin *load_pin = load_vertex->pin();
          bool elmore_exists = false;
          float elmore = 0.0;
          parasitics_->findElmore(parasitic, load_pin, elmore, elmore_exists);
          if (!elmore_exists)
            partially_annotated_drvrs_.insert(vertex);
        }
      }
      else {
        Net *net = network_->isTopLevelPort(drvr_pin)
          ? network_->net(network_->term(drvr_pin))
          : network_->net(drvr_pin);
        if (net)
          unannotated_nets_.insert(net);
      }
    }
  }
}

} // namespace
