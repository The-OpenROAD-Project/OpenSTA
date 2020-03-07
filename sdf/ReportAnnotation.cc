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
#include "DisallowCopyAssign.hh"
#include "StringUtil.hh"
#include "Report.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "GraphCmp.hh"
#include "Sdc.hh"
#include "DcalcAnalysisPt.hh"
#include "ReportAnnotation.hh"

namespace sta {

class ReportAnnotated : public StaState
{
public:
  ReportAnnotated(bool report_cells,
		  bool report_nets,
		  bool report_in_ports,
		  bool report_out_ports,
		  int max_lines,
		  bool list_annotated,
		  bool list_unannotated,
		  bool constant_arcs,
		  StaState *sta);
  ReportAnnotated(bool report_setup,
		  bool report_hold,
		  bool report_recovery,
		  bool report_removal,
		  bool report_nochange,
		  bool report_width,
		  bool report_period,
		  bool report_max_skew,
		  int max_lines,
		  bool list_annotated,
		  bool list_unannotated,
		  bool constant_arcs,
		  StaState *sta);
  void reportDelayAnnotation();
  void reportCheckAnnotation();

protected:
  enum CountIndex {
    count_internal_net = TimingRole::index_max,
    count_input_net,
    count_output_net,
    count_index_max
  };
  static int count_delay;

  void init();
  void findCounts();
  void findWidthPeriodCount(Pin *pin);
  void reportDelayCounts();
  void reportCheckCounts();
  void reportArcs();
  void reportArcs(const char *header,
		  bool report_annotated,
		  PinSet &pins);
  void reportArcs(Vertex *vertex,
		  bool report_annotated,
		  int &i);
  void reportWidthPeriodArcs(Pin *pin,
			     bool report_annotated,
			     int &i);
  void reportCount(const char *title,
		   int index,
		   int &total,
		   int &annotated_total);
  void reportCheckCount(TimingRole *role,
			int &total,
			int &annotated_total);
  int roleIndex(const TimingRole *role,
		const Pin *from_pin,
		const Pin *to_pin);

  int max_lines_;
  bool list_annotated_;
  bool list_unannotated_;
  bool report_constant_arcs_;

  int edge_count_[count_index_max];
  int edge_annotated_count_[count_index_max];
  int edge_constant_count_[count_index_max];
  int edge_constant_annotated_count_[count_index_max];
  bool report_role_[count_index_max];
  PinSet unannotated_pins_;
  PinSet annotated_pins_;

private:
  DISALLOW_COPY_AND_ASSIGN(ReportAnnotated);
};


int ReportAnnotated::count_delay;

void
reportAnnotatedDelay(bool report_cells,
		     bool report_nets,
		     bool from_in_ports,
		     bool to_out_ports,
		     int max_lines,
		     bool list_annotated,
		     bool list_unannotated,
		     bool report_constant_arcs,
		     StaState *sta)
{
  ReportAnnotated report_annotated(report_cells, report_nets,
				   from_in_ports, to_out_ports,
				   max_lines, list_annotated, list_unannotated,
				   report_constant_arcs, sta);
  report_annotated.reportDelayAnnotation();
}

ReportAnnotated::ReportAnnotated(bool report_cells,
				 bool report_nets,
				 bool report_in_ports,
				 bool report_out_ports,
				 int max_lines,
				 bool list_annotated,
				 bool list_unannotated,
				 bool report_constant_arcs,
				 StaState *sta) :
  StaState(sta),
  max_lines_(max_lines),
  list_annotated_(list_annotated),
  list_unannotated_(list_unannotated),
  report_constant_arcs_(report_constant_arcs)
{
  init();
  report_role_[TimingRole::sdfIopath()->index()] = report_cells;
  report_role_[count_internal_net] = report_nets;
  report_role_[count_input_net] = report_in_ports;
  report_role_[count_output_net] = report_out_ports;
}

void
ReportAnnotated::reportDelayAnnotation()
{
  findCounts();
  reportDelayCounts();
  reportArcs();
}

void
ReportAnnotated::reportDelayCounts()
{
  report_->print("                                                          Not   \n");
  report_->print("Delay type                        Total    Annotated   Annotated\n");
  report_->print("----------------------------------------------------------------\n");

  int total = 0;
  int annotated_total = 0;
  reportCount("cell arcs", count_delay, total, annotated_total);
  reportCount("internal net arcs", count_internal_net, total, annotated_total);
  reportCount("net arcs from primary inputs", count_input_net,
		total, annotated_total);
  reportCount("net arcs to primary outputs", count_output_net,
	      total, annotated_total);
  report_->print("----------------------------------------------------------------\n");
  report_->print("%-28s %10u  %10u  %10u\n",
		 " ",
		 total,
		 annotated_total,
		 total - annotated_total);
}

////////////////////////////////////////////////////////////////

void
reportAnnotatedCheck(bool report_setup,
		     bool report_hold,
		     bool report_recovery,
		     bool report_removal,
		     bool report_nochange,
		     bool report_width,
		     bool report_period,
		     bool report_max_skew,
		     int max_lines,
		     bool list_annotated,
		     bool list_unannotated,
		     bool report_constant_arcs,
		     StaState *sta)

{
  ReportAnnotated report_annotated(report_setup, report_hold,
				   report_recovery, report_removal,
				   report_nochange, report_width,
				   report_period,  report_max_skew,
				   max_lines, list_annotated, list_unannotated,
				   report_constant_arcs, sta);
  report_annotated.reportCheckAnnotation();
}

ReportAnnotated::ReportAnnotated(bool report_setup,
				 bool report_hold,
				 bool report_recovery,
				 bool report_removal,
				 bool report_nochange,
				 bool report_width,
				 bool report_period,
				 bool report_max_skew,
				 int max_lines,
				 bool list_annotated,
				 bool list_unannotated,
				 bool report_constant_arcs,
				 StaState *sta) :
  StaState(sta),
  max_lines_(max_lines),
  list_annotated_(list_annotated),
  list_unannotated_(list_unannotated),
  report_constant_arcs_(report_constant_arcs)
{
  init();
  report_role_[TimingRole::setup()->index()] = report_setup;
  report_role_[TimingRole::hold()->index()] = report_hold;
  report_role_[TimingRole::recovery()->index()] = report_recovery;
  report_role_[TimingRole::removal()->index()] = report_removal;
  report_role_[TimingRole::nochange()->index()] = report_nochange;
  report_role_[TimingRole::width()->index()] = report_width;
  report_role_[TimingRole::period()->index()] = report_period;
  report_role_[TimingRole::skew()->index()] = report_max_skew;
}

void
ReportAnnotated::reportCheckAnnotation()
{
  findCounts();
  reportCheckCounts();
  reportArcs();
}

void
ReportAnnotated::reportCheckCounts()
{
  report_->print("                                                          Not   \n");
  report_->print("Check type                        Total    Annotated   Annotated\n");
  report_->print("----------------------------------------------------------------\n");

  int total = 0;
  int annotated_total = 0;
  reportCheckCount(TimingRole::setup(), total, annotated_total);
  reportCheckCount(TimingRole::hold(), total, annotated_total);
  reportCheckCount(TimingRole::recovery(), total, annotated_total);
  reportCheckCount(TimingRole::removal(), total, annotated_total);
  reportCheckCount(TimingRole::nochange(), total, annotated_total);
  reportCheckCount(TimingRole::width(), total, annotated_total);
  reportCheckCount(TimingRole::period(), total, annotated_total);
  reportCheckCount(TimingRole::skew(), total, annotated_total);

  report_->print("----------------------------------------------------------------\n");
  report_->print("%-28s %10u  %10u  %10u\n",
		 " ",
		 total,
		 annotated_total,
		 total - annotated_total);
}

void
ReportAnnotated::reportCheckCount(TimingRole *role,
				  int &total,
				  int &annotated_total)
{
  int index = role->index();
  if (edge_count_[index] > 0) {
    const char *role_name = role->asString();
    string title;
    stringPrint(title, "cell %s arcs", role_name);
    reportCount(title.c_str(), index, total, annotated_total);
  }
}

////////////////////////////////////////////////////////////////

void
ReportAnnotated::init()
{
  count_delay = TimingRole::sdfIopath()->index();
  for (int i = 0; i < count_index_max; i++) {
    edge_count_[i] = 0;
    edge_annotated_count_[i] = 0;
    edge_constant_count_[i] = 0;
    edge_constant_annotated_count_[i] = 0;
    report_role_[i] = false;
  }
}

void
ReportAnnotated::findCounts()
{
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *from_vertex = vertex_iter.next();
    Pin *from_pin = from_vertex->pin();
    LogicValue from_logic_value;
    bool from_logic_value_exists;
    sdc_->logicValue(from_pin, from_logic_value,
		     from_logic_value_exists);
    VertexOutEdgeIterator edge_iter(from_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      const TimingRole *role = edge->role();
      Vertex *to_vertex = edge->to(graph_);
      Pin *to_pin = to_vertex->pin();
      int index = roleIndex(role, from_pin, to_pin);
      LogicValue to_logic_value;
      bool to_logic_value_exists;
      sdc_->logicValue(to_pin, to_logic_value,
		       to_logic_value_exists);

      edge_count_[index]++;

      if (from_logic_value_exists || to_logic_value_exists)
	edge_constant_count_[index]++;
      if (report_role_[index]) {
	if (graph_->delayAnnotated(edge)) {
	  edge_annotated_count_[index]++;
	  if (from_logic_value_exists || to_logic_value_exists)
	    edge_constant_annotated_count_[index]++;
	  if (list_annotated_)
	    annotated_pins_.insert(from_pin);
	}
	else {
	  if (list_unannotated_)
	    unannotated_pins_.insert(from_pin);
	}
      }
    }
    findWidthPeriodCount(from_pin);
  }
}

int
ReportAnnotated::roleIndex(const TimingRole *role,
			   const Pin *from_pin,
			   const Pin *to_pin)
{
  if (role == TimingRole::wire()) {
    if (network_->isTopLevelPort(from_pin))
      return count_input_net;
    else if (network_->isTopLevelPort(to_pin))
      return count_output_net;
    else
      return count_internal_net;
  }
  else if (role->sdfRole() == TimingRole::sdfIopath())
    return count_delay;
  else {
    if (role->isTimingCheck()
	&& (role == TimingRole::latchSetup()
	    || role == TimingRole::latchHold()))
      role = role->genericRole();
    return role->index();
  }
}

// Width and period checks are not edges in the graph so
// they require special handling.
void
ReportAnnotated::findWidthPeriodCount(Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  if (port) {
    DcalcAPIndex ap_index = 0;
    float value;
    bool exists, annotated;
    int period_index = TimingRole::period()->index();
    if (report_role_[period_index]) {
      port->minPeriod(value, exists);
      if (exists) {
	edge_count_[period_index]++;
	graph_->periodCheckAnnotation(pin, ap_index, value, annotated);
	if (annotated) {
	  edge_annotated_count_[period_index]++;
	  if (list_annotated_)
	    annotated_pins_.insert(pin);
	}
	else {
	  if (list_unannotated_)
	    unannotated_pins_.insert(pin);
	}
      }
    }

    int width_index = TimingRole::width()->index();
    if (report_role_[width_index]) {
      for (auto hi_low : RiseFall::range()) {
	port->minPulseWidth(hi_low, value, exists);
	if (exists) {
	  edge_count_[width_index]++;
	  graph_->widthCheckAnnotation(pin, hi_low, ap_index,
				       value, annotated);
	  if (annotated) {
	    edge_annotated_count_[width_index]++;
	    if (list_annotated_)
	      annotated_pins_.insert(pin);
	  }
	  else {
	    if (list_unannotated_)
	      unannotated_pins_.insert(pin);
	  }
	}
      }
    }
  }
}

void
ReportAnnotated::reportCount(const char *title,
			     int index,
			     int &total,
			     int &annotated_total)
{
  if (report_role_[index]) {
    int count = edge_count_[index];
    int annotated_count = edge_annotated_count_[index];
    report_->print("%-28s %10u  %10u  %10u\n",
		   title,
		   count,
		   annotated_count,
		   count - annotated_count);
    if (report_constant_arcs_) {
      int const_count = edge_constant_count_[index];
      int const_annotated_count = edge_constant_annotated_count_[index];
      report_->print("%-28s %10s  %10u  %10u\n",
		     "constant arcs",
		     "",
		     const_annotated_count,
		     const_count - const_annotated_count);
    }
    total += count;
    annotated_total += annotated_count;
  }
}

void
ReportAnnotated::reportArcs()
{
  if (list_annotated_)
    reportArcs("Annotated Arcs", true, annotated_pins_);
  if (list_unannotated_)
    reportArcs("Unannotated Arcs", false, unannotated_pins_);
}

void
ReportAnnotated::reportArcs(const char *header,
			    bool report_annotated,
			    PinSet &pins)
{
  report_->print("\n");
  report_->print(header);
  report_->print("\n");
  PinSeq sorted_pins;
  sortPinSet(&pins, network_, sorted_pins);
  int i = 0;
  PinSeq::Iterator pin_iter(sorted_pins);
  while (pin_iter.hasNext()
	 && (max_lines_ == 0 || i < max_lines_)) {
    Pin *pin = pin_iter.next();
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    reportArcs(vertex, report_annotated, i);
    if (bidirect_drvr_vertex)
      reportArcs(bidirect_drvr_vertex, report_annotated, i);
    reportWidthPeriodArcs(pin, report_annotated, i);
  }
}

void
ReportAnnotated::reportArcs(Vertex *vertex,
			    bool report_annotated,
			    int &i)
{
  const Pin *from_pin = vertex->pin();
  VertexOutEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()
	 && (max_lines_ == 0 || i < max_lines_)) {
    Edge *edge = edge_iter.next();
    TimingRole *role = edge->role();
    const Pin *to_pin = edge->to(graph_)->pin();
    if (graph_->delayAnnotated(edge) == report_annotated
	&& report_role_[roleIndex(role, from_pin, to_pin)]) {
      const char *role_name;
      if (role->isTimingCheck())
	role_name = role->asString();
      else if (role->isWire()) {
	if (network_->isTopLevelPort(from_pin))
	  role_name = "primary input net";
	else if (network_->isTopLevelPort(to_pin))
	  role_name = "primary output net";
	else
	  role_name = "internal net";
      }
      else
	role_name = "delay";
      report_->print(" %-18s %s -> %s",
		     role_name,
		     network_->pathName(from_pin),
		     network_->pathName(to_pin));
      const char *cond = edge->timingArcSet()->sdfCond();
      if (cond)
	report_->print(" %s", cond);
      report_->print("\n");
      i++;
    }
  }
}

void
ReportAnnotated::reportWidthPeriodArcs(Pin *pin,
				       bool report_annotated,
				       int &i)
{
  LibertyPort *port = network_->libertyPort(pin);
  if (port) {
    DcalcAPIndex ap_index = 0;
    float value;
    bool exists, annotated;
    int period_index = TimingRole::period()->index();
    if (report_role_[period_index]
	&& (max_lines_ == 0 || i < max_lines_)) {
      float value;
      bool exists, annotated;
      port->minPeriod(value, exists);
      if (exists) {
	edge_count_[period_index]++;
	graph_->periodCheckAnnotation(pin, ap_index, value, annotated);
	if (annotated == report_annotated) {
	  report_->print(" %-18s %s\n",
			 "period",
			 network_->pathName(pin));
	  i++;
	}
      }
    }

    int width_index = TimingRole::width()->index();
    if (report_role_[width_index]
	&& (max_lines_ == 0 || i < max_lines_)) {
      bool report = false;
      for (auto hi_low : RiseFall::range()) {
	port->minPulseWidth(hi_low, value, exists);
	if (exists) {
	  edge_count_[width_index]++;
	  graph_->widthCheckAnnotation(pin, hi_low, ap_index,
				       value, annotated);
	  report |= (annotated == report_annotated);
	}
      }
      if (report) {
	report_->print(" %-18s %s\n",
		       "min width",
		       network_->pathName(pin));
	i++;
      }
    }
  }
}

} // namespace
