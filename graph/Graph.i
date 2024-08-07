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

%module graph

%{
#include "Graph.hh"
#include "FuncExpr.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Clock.hh"
#include "Corner.hh"
#include "Search.hh"
#include "Sta.hh"

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// Empty class definitions to make swig happy.
// Private constructor/destructor so swig doesn't emit them.
//
////////////////////////////////////////////////////////////////

class Vertex
{
private:
  Vertex();
  ~Vertex();
};

class Edge
{
private:
  Edge();
  ~Edge();
};

class VertexIterator
{
private:
  VertexIterator();
  ~VertexIterator();
};

class VertexInEdgeIterator
{
private:
  VertexInEdgeIterator();
  ~VertexInEdgeIterator();
};

class VertexOutEdgeIterator
{
private:
  VertexOutEdgeIterator();
  ~VertexOutEdgeIterator();
};

%inline %{

int
graph_vertex_count()
{
  return cmdGraph()->vertexCount();
}

int
graph_edge_count()
{
  return cmdGraph()->edgeCount();
}

int
graph_arc_count()
{
  return cmdGraph()->arcCount();
}

VertexIterator *
vertex_iterator()
{
  return new VertexIterator(cmdGraph());
}

void
set_arc_delay(Edge *edge,
	      TimingArc *arc,
	      const Corner *corner,
	      const MinMaxAll *min_max,
	      float delay)
{
  cmdGraph();
  Sta::sta()->setArcDelay(edge, arc, corner, min_max, delay);
}

void
set_annotated_slew(Vertex *vertex,
		   const Corner *corner,
		   const MinMaxAll *min_max,
		   const RiseFallBoth *rf,
		   float slew)
{
  cmdGraph();
  Sta::sta()->setAnnotatedSlew(vertex, corner, min_max, rf, slew);
}

// Remove all delay and slew annotations.
void
remove_delay_slew_annotations()
{
  cmdGraph();
  Sta::sta()->removeDelaySlewAnnotations();
}

%} // inline

////////////////////////////////////////////////////////////////
//
// Object Methods
//
////////////////////////////////////////////////////////////////

%extend Vertex {
Pin *pin() { return self->pin(); }
bool is_bidirect_driver() { return self->isBidirectDriver(); }
int level() { return Sta::sta()->vertexLevel(self); }
int tag_group_index() { return self->tagGroupIndex(); }

Slew
slew(const RiseFall *rf,
     const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  return sta->vertexSlew(self, rf, min_max);
}

Slew
slew_corner(const RiseFall *rf,
            const Corner *corner,
            const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  return sta->vertexSlew(self, rf, corner, min_max);
}

VertexOutEdgeIterator *
out_edge_iterator()
{
  return new VertexOutEdgeIterator(self, Sta::sta()->graph());
}

VertexInEdgeIterator *
in_edge_iterator()
{
  return new VertexInEdgeIterator(self, Sta::sta()->graph());
}

FloatSeq
arrivals_clk(const RiseFall *rf,
	     Clock *clk,
	     const RiseFall *clk_rf)
{
  Sta *sta = Sta::sta();
  FloatSeq arrivals;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    arrivals.push_back(delayAsFloat(sta->vertexArrival(self, rf, clk_edge,
                                                       path_ap, nullptr)));
  }
  return arrivals;
}

StringSeq
arrivals_clk_delays(const RiseFall *rf,
		    Clock *clk,
		    const RiseFall *clk_rf,
		    int digits)
{
  Sta *sta = Sta::sta();
  StringSeq arrivals;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    arrivals.push_back(delayAsString(sta->vertexArrival(self, rf, clk_edge,
                                                        path_ap, nullptr),
                                     sta, digits));
  }
  return arrivals;
}

FloatSeq
requireds_clk(const RiseFall *rf,
	      Clock *clk,
	      const RiseFall *clk_rf)
{
  Sta *sta = Sta::sta();
  FloatSeq reqs;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    reqs.push_back(delayAsFloat(sta->vertexRequired(self, rf, clk_edge,
                                                    path_ap)));
  }
  return reqs;
}

StringSeq
requireds_clk_delays(const RiseFall *rf,
		     Clock *clk,
		     const RiseFall *clk_rf,
		     int digits)
{
  Sta *sta = Sta::sta();
  StringSeq reqs;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    reqs.push_back(delayAsString(sta->vertexRequired(self, rf, clk_edge, path_ap),
                                 sta, digits));
  }
  return reqs;
}

Slack
slack(MinMax *min_max)
{
  Sta *sta = Sta::sta();
  return sta->vertexSlack(self, min_max);
}

FloatSeq
slacks(RiseFall *rf)
{
  Sta *sta = Sta::sta();
  FloatSeq slacks;
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    slacks.push_back(delayAsFloat(sta->vertexSlack(self, rf, path_ap)));
  }
  return slacks;
}

// Slack with respect to a clock rise/fall edge.
FloatSeq
slacks_clk(const RiseFall *rf,
	   Clock *clk,
	   const RiseFall *clk_rf)
{
  Sta *sta = Sta::sta();
  FloatSeq slacks;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    slacks.push_back(delayAsFloat(sta->vertexSlack(self, rf, clk_edge,
                                                   path_ap)));
  }
  return slacks;
}

StringSeq
slacks_clk_delays(const RiseFall *rf,
		  Clock *clk,
		  const RiseFall *clk_rf,
		  int digits)
{
  Sta *sta = Sta::sta();
  StringSeq slacks;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    slacks.push_back(delayAsString(sta->vertexSlack(self, rf, clk_edge,
                                                    path_ap),
                                   sta, digits));
  }
  return slacks;
}

VertexPathIterator *
path_iterator(const RiseFall *rf,
	      const MinMax *min_max)
{
  return Sta::sta()->vertexPathIterator(self, rf, min_max);
}

bool
has_downstream_clk_pin()
{
  return self->hasDownstreamClkPin();
}

bool
is_clock()
{
  Sta *sta = Sta::sta();
  Search *search = sta->search();
  return search->isClock(self);
}

bool is_disabled_constraint() { return self->isDisabledConstraint(); }

} // Vertex methods

%extend Edge {
Vertex *from() { return self->from(Sta::sta()->graph()); }
Vertex *to() { return self->to(Sta::sta()->graph()); }
Pin *from_pin() { return self->from(Sta::sta()->graph())->pin(); }
Pin *to_pin() { return self->to(Sta::sta()->graph())->pin(); }
TimingRole *role() { return self->role(); }
const char *sense() { return timingSenseString(self->sense()); }
TimingArcSeq &
timing_arcs() { return self->timingArcSet()->arcs(); }
bool is_disabled_loop() { return Sta::sta()->isDisabledLoop(self); }
bool is_disabled_constraint() { return Sta::sta()->isDisabledConstraint(self);}
bool is_disabled_constant() { return Sta::sta()->isDisabledConstant(self); }
bool is_disabled_cond_default()
{ return Sta::sta()->isDisabledCondDefault(self); }
PinSet
disabled_constant_pins() { return Sta::sta()->disabledConstantPins(self); }
bool is_disabled_bidirect_inst_path()
{ return Sta::sta()->isDisabledBidirectInstPath(self); }
bool is_disabled_bidirect_net_path()
{ return Sta::sta()->isDisabledBidirectNetPath(self); }
bool is_disabled_preset_clear()
{ return Sta::sta()->isDisabledPresetClr(self); }
const char *
sim_timing_sense(){return timingSenseString(Sta::sta()->simTimingSense(self));}

FloatSeq
arc_delays(TimingArc *arc)
{
  Sta *sta = Sta::sta();
  FloatSeq delays;
  for (auto dcalc_ap : sta->corners()->dcalcAnalysisPts())
    delays.push_back(delayAsFloat(sta->arcDelay(self, arc, dcalc_ap)));
  return delays;
}

StringSeq
arc_delay_strings(TimingArc *arc,
		  int digits)
{
  Sta *sta = Sta::sta();
  StringSeq delays;
  for (auto dcalc_ap : sta->corners()->dcalcAnalysisPts())
    delays.push_back(delayAsString(sta->arcDelay(self, arc, dcalc_ap),
                                   sta, digits));
  return delays;
}

bool
delay_annotated(TimingArc *arc,
		const Corner *corner,
		const MinMax *min_max)
{
  DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  return Sta::sta()->arcDelayAnnotated(self, arc, dcalc_ap);
}

float
arc_delay(TimingArc *arc,
	  const Corner *corner,
	  const MinMax *min_max)
{
  DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  return delayAsFloat(Sta::sta()->arcDelay(self, arc, dcalc_ap));
}

const char *
cond()
{
  FuncExpr *cond = self->timingArcSet()->cond();
  if (cond)
    return cond->asString();
  else
    return nullptr;
}

const char *
mode_name()
{
  return self->timingArcSet()->modeName();
}

const char *
mode_value()
{
  return self->timingArcSet()->modeValue();
}

const char *
latch_d_to_q_en()
{
  if (self->role() == TimingRole::latchDtoQ()) {
    Sta *sta = Sta::sta();
    const Network *network = sta->cmdNetwork();
    const Graph *graph = sta->graph();
    Pin *from_pin = self->from(graph)->pin();
    Instance *inst = network->instance(from_pin);
    LibertyCell *lib_cell = network->libertyCell(inst);
    TimingArcSet *d_q_set = self->timingArcSet();
    const LibertyPort *enable_port;
    const FuncExpr *enable_func;
    const RiseFall *enable_rf;
    lib_cell->latchEnable(d_q_set, enable_port, enable_func, enable_rf);
    if (enable_port)
      return stringPrintTmp("%s %s", enable_port->name(), enable_rf->asString());
  }
  return "";
}

} // Edge methods

%extend VertexIterator {
bool has_next() { return self->hasNext(); }
Vertex *next() { return self->next(); }
void finish() { delete self; }
}

%extend VertexInEdgeIterator {
bool has_next() { return self->hasNext(); }
Edge *next() { return self->next(); }
void finish() { delete self; }
}

%extend VertexOutEdgeIterator {
bool has_next() { return self->hasNext(); }
Edge *next() { return self->next(); }
void finish() { delete self; }
}
