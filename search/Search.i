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

%module search

%{
#include "Sta.hh"
#include "Search.hh"

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// Empty class definitions to make swig happy.
// Private constructor/destructor so swig doesn't emit them.
//
////////////////////////////////////////////////////////////////

class VertexPathIterator
{
private:
  VertexPathIterator();
  ~VertexPathIterator();
};

class PathRef
{
private:
  PathRef();
  ~PathRef();
};

class PathEnd
{
private:
  PathEnd();
  ~PathEnd();
};

class MinPulseWidthCheck
{
private:
  MinPulseWidthCheck();
  ~MinPulseWidthCheck();
};

class MinPulseWidthCheckSeq
{
private:
  MinPulseWidthCheckSeq();
  ~MinPulseWidthCheckSeq();
};

class MinPulseWidthCheckSeqIterator
{
private:
  MinPulseWidthCheckSeqIterator();
  ~MinPulseWidthCheckSeqIterator();
};

%inline %{

void
find_timing_cmd(bool full)
{
  cmdLinkedNetwork();
  Sta::sta()->updateTiming(full);
}

void
arrivals_invalid()
{
  Sta *sta = Sta::sta();
  sta->arrivalsInvalid();
}

PinSet
startpoints()
{
  return Sta::sta()->startpointPins();
}

PinSet
endpoints()
{
  return Sta::sta()->endpointPins();
}

size_t
endpoint_count()
{
  return Sta::sta()->endpointPins().size();
}

void
find_requireds()
{
  cmdLinkedNetwork();
  Sta::sta()->findRequireds();
}

Slack
total_negative_slack_cmd(const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  return sta->totalNegativeSlack(min_max);
}

Slack
total_negative_slack_corner_cmd(const Corner *corner,
				const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  return sta->totalNegativeSlack(corner, min_max);
}

Slack
worst_slack_cmd(const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  Slack worst_slack;
  Vertex *worst_vertex;
  sta->worstSlack(min_max, worst_slack, worst_vertex);
  return worst_slack;
}

Vertex *
worst_slack_vertex(const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  Slack worst_slack;
  Vertex *worst_vertex;
  sta->worstSlack(min_max, worst_slack, worst_vertex);
  return worst_vertex;;
}

Slack
worst_slack_corner(const Corner *corner,
		   const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  Slack worst_slack;
  Vertex *worst_vertex;
  sta->worstSlack(corner, min_max, worst_slack, worst_vertex);
  return worst_slack;
}

PathRef *
vertex_worst_arrival_path(Vertex *vertex,
			  const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  PathRef path = sta->vertexWorstArrivalPath(vertex, min_max);
  if (!path.isNull())
    return new PathRef(path);
  else
    return nullptr;
}

PathRef *
vertex_worst_arrival_path_rf(Vertex *vertex,
			     const RiseFall *rf,
			     MinMax *min_max)
{
  Sta *sta = Sta::sta();
  PathRef path = sta->vertexWorstArrivalPath(vertex, rf, min_max);
  if (!path.isNull())
    return new PathRef(path);
  else
    return nullptr;
}

PathRef *
vertex_worst_slack_path(Vertex *vertex,
			const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  PathRef path = sta->vertexWorstSlackPath(vertex, min_max);
  if (!path.isNull())
    return new PathRef(path);
  else
    return nullptr;
}

int
tag_group_count()
{
  return Sta::sta()->tagGroupCount();
}

void
report_tag_groups()
{
  Sta::sta()->search()->reportTagGroups();
}

void
report_tag_arrivals_cmd(Vertex *vertex)
{
  Sta::sta()->search()->reportArrivals(vertex);
}

void
report_arrival_count_histogram()
{
  Sta::sta()->search()->reportArrivalCountHistogram();
}

int
tag_count()
{
  return Sta::sta()->tagCount();
}

void
report_tags()
{
  Sta::sta()->search()->reportTags();
}

void
report_clk_infos()
{
  Sta::sta()->search()->reportClkInfos();
}

int
clk_info_count()
{
  return Sta::sta()->clkInfoCount();
}

int
arrival_count()
{
  return Sta::sta()->arrivalCount();
}

int
required_count()
{
  return Sta::sta()->requiredCount();
}

int
graph_arrival_count()
{
  return Sta::sta()->graph()->arrivalCount();
}

int
graph_required_count()
{
  return Sta::sta()->graph()->requiredCount();
}

int
endpoint_violation_count(const MinMax *min_max)
{
  return  Sta::sta()->endpointViolationCount(min_max);
}

%} // inline

////////////////////////////////////////////////////////////////
//
// Object Methods
//
////////////////////////////////////////////////////////////////

%extend PathEnd {
bool is_unconstrained() { return self->isUnconstrained(); }
bool is_check() { return self->isCheck(); }
bool is_latch_check() { return self->isLatchCheck(); }
bool is_data_check() { return self->isDataCheck(); }
bool is_output_delay() { return self->isOutputDelay(); }
bool is_path_delay() { return self->isPathDelay(); }
bool is_gated_clock() { return self->isGatedClock(); }
Vertex *vertex() { return self->vertex(Sta::sta()); }
PathRef *path() { return &self->pathRef(); }
RiseFall *end_transition()
{ return const_cast<RiseFall*>(self->path()->transition(Sta::sta())); }
Slack slack() { return self->slack(Sta::sta()); }
ArcDelay margin() { return self->margin(Sta::sta()); }
Required data_required_time() { return self->requiredTimeOffset(Sta::sta()); }
Arrival data_arrival_time() { return self->dataArrivalTimeOffset(Sta::sta()); }
TimingRole *check_role() { return self->checkRole(Sta::sta()); }
MinMax *min_max() { return const_cast<MinMax*>(self->minMax(Sta::sta())); }
float source_clk_offset() { return self->sourceClkOffset(Sta::sta()); }
Arrival source_clk_latency() { return self->sourceClkLatency(Sta::sta()); }
Arrival source_clk_insertion_delay()
{ return self->sourceClkInsertionDelay(Sta::sta()); }
const Clock *target_clk() { return self->targetClk(Sta::sta()); }
const ClockEdge *target_clk_edge() { return self->targetClkEdge(Sta::sta()); }
Path *target_clk_path() { return self->targetClkPath(); }
float target_clk_time() { return self->targetClkTime(Sta::sta()); }
float target_clk_offset() { return self->targetClkOffset(Sta::sta()); }
float target_clk_mcp_adjustment()
{ return self->targetClkMcpAdjustment(Sta::sta()); }
Arrival target_clk_delay() { return self->targetClkDelay(Sta::sta()); }
Arrival target_clk_insertion_delay()
{ return self->targetClkInsertionDelay(Sta::sta()); }
float target_clk_uncertainty()
{ return self->targetNonInterClkUncertainty(Sta::sta()); }
float inter_clk_uncertainty()
{ return self->interClkUncertainty(Sta::sta()); }
Arrival target_clk_arrival() { return self->targetClkArrival(Sta::sta()); }
bool path_delay_margin_is_external()
{ return self->pathDelayMarginIsExternal();}
Crpr check_crpr() { return self->checkCrpr(Sta::sta()); }
RiseFall *target_clk_end_trans()
{ return const_cast<RiseFall*>(self->targetClkEndTrans(Sta::sta())); }
Delay clk_skew() { return self->clkSkew(Sta::sta()); }

}

%extend PathRef {
float
arrival()
{
  Sta *sta = Sta::sta();
  return delayAsFloat(self->arrival(sta));
}

float
required()
{
  Sta *sta = Sta::sta();
  return delayAsFloat(self->required(sta));
}

float
slack()
{
  Sta *sta = Sta::sta();
  return delayAsFloat(self->slack(sta));
}

const Pin *
pin()
{
  Sta *sta = Sta::sta();
  return self->pin(sta);
}

const char *
tag()
{
  Sta *sta = Sta::sta();
  return self->tag(sta)->asString(sta);
}

// mea_opt3
PinSeq
pins()
{
  Sta *sta = Sta::sta();
  PinSeq pins;
  PathRef path1(self);
  while (!path1.isNull()) {
    pins.push_back(path1.vertex(sta)->pin());
    PathRef prev_path;
    path1.prevPath(sta, prev_path);
    path1.init(prev_path);
  }
  return pins;
}

}

%extend VertexPathIterator {
bool has_next() { return self->hasNext(); }
PathRef *
next()
{
  Path *path = self->next();
  return new PathRef(path);
}

void finish() { delete self; }
}

%extend MinPulseWidthCheckSeqIterator {
bool has_next() { return self->hasNext(); }
MinPulseWidthCheck *next() { return self->next(); }
void finish() { delete self; }
} // MinPulseWidthCheckSeqIterator methods
