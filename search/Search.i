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
#include "PathGroup.hh"
#include "Search.hh"
#include "search/Levelize.hh"
#include "search/ReportPath.hh"

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

class Corner
{
private:
  Corner();
  ~Corner();
};

%inline %{

int group_count_max = PathGroup::group_count_max;

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

void
report_loops()
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  Graph *graph = cmdGraph();
  Report *report = sta->report();
  for (GraphLoop *loop : *sta->graphLoops()) {
    loop->report(report, network, graph);
    report->reportLineString("");
  }
}

char
pin_sim_logic_value(const Pin *pin)
{
  return logicValueString(Sta::sta()->simLogicValue(pin));
}

InstanceSeq
slow_drivers(int count)
{
  return Sta::sta()->slowDrivers(count);
}

////////////////////////////////////////////////////////////////

PathEndSeq
find_path_ends(ExceptionFrom *from,
	       ExceptionThruSeq *thrus,
	       ExceptionTo *to,
	       bool unconstrained,
	       Corner *corner,
	       const MinMaxAll *delay_min_max,
	       int group_count,
	       int endpoint_count,
	       bool unique_pins,
	       float slack_min,
	       float slack_max,
	       bool sort_by_slack,
	       PathGroupNameSet *groups,
	       bool setup,
	       bool hold,
	       bool recovery,
	       bool removal,
	       bool clk_gating_setup,
	       bool clk_gating_hold)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  PathEndSeq ends = sta->findPathEnds(from, thrus, to, unconstrained,
                                      corner, delay_min_max,
                                      group_count, endpoint_count, unique_pins,
                                      slack_min, slack_max,
                                      sort_by_slack,
                                      groups->size() ? groups : nullptr,
                                      setup, hold,
                                      recovery, removal,
                                      clk_gating_setup, clk_gating_hold);
  delete groups;
  return ends;
}

////////////////////////////////////////////////////////////////

void
report_path_end_header()
{
  Sta::sta()->reportPathEndHeader();
}

void
report_path_end_footer()
{
  Sta::sta()->reportPathEndFooter();
}

void
report_path_end(PathEnd *end)
{
  Sta::sta()->reportPathEnd(end);
}

void
report_path_end2(PathEnd *end,
		 PathEnd *prev_end)
{
  Sta::sta()->reportPathEnd(end, prev_end);
}

void
set_report_path_format(ReportPathFormat format)
{
  Sta::sta()->setReportPathFormat(format);
}
    
void
set_report_path_field_order(StringSeq *field_names)
{
  Sta::sta()->setReportPathFieldOrder(field_names);
  delete field_names;
}

void
set_report_path_fields(bool report_input_pin,
		       bool report_net,
		       bool report_cap,
		       bool report_slew,
                       bool report_fanout)
{
  Sta::sta()->setReportPathFields(report_input_pin,
				  report_net,
				  report_cap,
				  report_slew,
                                  report_fanout);
}

void
set_report_path_field_properties(const char *field_name,
				 const char *title,
				 int width,
				 bool left_justify)
{
  Sta *sta = Sta::sta();
  ReportField *field = sta->findReportPathField(field_name);
  if (field)
    field->setProperties(title, width, left_justify);
  else
    sta->report()->error(1575, "unknown report path field %s", field_name);
}

void
set_report_path_field_width(const char *field_name,
			    int width)
{
  Sta *sta = Sta::sta();
  ReportField *field = sta->findReportPathField(field_name);
  if (field)
    field->setWidth(width);
  else
    sta->report()->error(1576, "unknown report path field %s", field_name);
}

void
set_report_path_digits(int digits)
{
  Sta::sta()->setReportPathDigits(digits);
}

void
set_report_path_no_split(bool no_split)
{
  Sta::sta()->setReportPathNoSplit(no_split);
}

void
set_report_path_sigmas(bool report_sigmas)
{
  Sta::sta()->setReportPathSigmas(report_sigmas);
}

void
delete_path_ref(PathRef *path)
{
  delete path;
}

void
report_path_cmd(PathRef *path)
{
  Sta::sta()->reportPath(path);
}

////////////////////////////////////////////////////////////////

void
report_clk_skew(ConstClockSeq clks,
		const Corner *corner,
		const SetupHold *setup_hold,
                bool include_internal_latency,
		int digits)
{
  cmdLinkedNetwork();
  Sta::sta()->reportClkSkew(clks, corner, setup_hold,
                            include_internal_latency, digits);
}

void
report_clk_latency(ConstClockSeq clks,
                   const Corner *corner,
                   bool include_internal_latency,
                   int digits)
{
  cmdLinkedNetwork();
  Sta::sta()->reportClkLatency(clks, corner, include_internal_latency, digits);
}

float
worst_clk_skew_cmd(const SetupHold *setup_hold,
                   bool include_internal_latency)
{
  cmdLinkedNetwork();
  return Sta::sta()->findWorstClkSkew(setup_hold, include_internal_latency);
}

////////////////////////////////////////////////////////////////

MinPulseWidthCheckSeq &
min_pulse_width_violations(const Corner *corner)
{
  cmdLinkedNetwork();
  return Sta::sta()->minPulseWidthViolations(corner);
}

MinPulseWidthCheckSeq &
min_pulse_width_check_pins(PinSeq *pins,
			   const Corner *corner)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  MinPulseWidthCheckSeq &checks = sta->minPulseWidthChecks(pins, corner);
  delete pins;
  return checks;
}

MinPulseWidthCheckSeq &
min_pulse_width_checks(const Corner *corner)
{
  cmdLinkedNetwork();
  return Sta::sta()->minPulseWidthChecks(corner);
}

MinPulseWidthCheck *
min_pulse_width_check_slack(const Corner *corner)
{
  cmdLinkedNetwork();
  return Sta::sta()->minPulseWidthSlack(corner);
}

void
report_mpw_checks(MinPulseWidthCheckSeq *checks,
		  bool verbose)
{
  Sta::sta()->reportMpwChecks(checks, verbose);
}

void
report_mpw_check(MinPulseWidthCheck *check,
		 bool verbose)
{
  Sta::sta()->reportMpwCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

MinPeriodCheckSeq &
min_period_violations()
{
  cmdLinkedNetwork();
  return Sta::sta()->minPeriodViolations();
}

MinPeriodCheck *
min_period_check_slack()
{
  cmdLinkedNetwork();
  return Sta::sta()->minPeriodSlack();
}

void
report_min_period_checks(MinPeriodCheckSeq *checks,
			 bool verbose)
{
  Sta::sta()->reportChecks(checks, verbose);
}

void
report_min_period_check(MinPeriodCheck *check,
			bool verbose)
{
  Sta::sta()->reportCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

MaxSkewCheckSeq &
max_skew_violations()
{
  cmdLinkedNetwork();
  return Sta::sta()->maxSkewViolations();
}

MaxSkewCheck *
max_skew_check_slack()
{
  cmdLinkedNetwork();
  return Sta::sta()->maxSkewSlack();
}

void
report_max_skew_checks(MaxSkewCheckSeq *checks,
		       bool verbose)
{
  Sta::sta()->reportChecks(checks, verbose);
}

void
report_max_skew_check(MaxSkewCheck *check,
		      bool verbose)
{
  Sta::sta()->reportCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

Slack
find_clk_min_period(const Clock *clk,
                    bool ignore_port_paths)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  return sta->findClkMinPeriod(clk, ignore_port_paths);
}

////////////////////////////////////////////////////////////////

PinSeq
check_slew_limits(Net *net,
                  bool violators,
                  const Corner *corner,
                  const MinMax *min_max)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkSlewLimits(net, violators, corner, min_max);
}

size_t
max_slew_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkSlewLimits(nullptr, true, nullptr, MinMax::max()).size();
}

float
max_slew_check_slack()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  Slew slew;
  float slack;
  float limit;
  sta->maxSlewCheck(pin, slew, slack, limit);
  return sta->units()->timeUnit()->staToUser(slack);
}

float
max_slew_check_limit()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  Slew slew;
  float slack;
  float limit;
  sta->maxSlewCheck(pin, slew, slack, limit);
  return sta->units()->timeUnit()->staToUser(limit);
}

void
report_slew_limit_short_header()
{
  Sta::sta()->reportSlewLimitShortHeader();
}

void
report_slew_limit_short(Pin *pin,
			const Corner *corner,
			const MinMax *min_max)
{
  Sta::sta()->reportSlewLimitShort(pin, corner, min_max);
}

void
report_slew_limit_verbose(Pin *pin,
			  const Corner *corner,
			  const MinMax *min_max)
{
  Sta::sta()->reportSlewLimitVerbose(pin, corner, min_max);
}

////////////////////////////////////////////////////////////////

PinSeq
check_fanout_limits(Net *net,
                    bool violators,
                    const MinMax *min_max)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkFanoutLimits(net, violators, min_max);
}

size_t
max_fanout_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkFanoutLimits(nullptr, true, MinMax::max()).size();
}

float
max_fanout_check_slack()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  float fanout;
  float slack;
  float limit;
  sta->maxFanoutCheck(pin, fanout, slack, limit);
  return slack;;
}

float
max_fanout_check_limit()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  float fanout;
  float slack;
  float limit;
  sta->maxFanoutCheck(pin, fanout, slack, limit);
  return limit;;
}

void
report_fanout_limit_short_header()
{
  Sta::sta()->reportFanoutLimitShortHeader();
}

void
report_fanout_limit_short(Pin *pin,
			  const MinMax *min_max)
{
  Sta::sta()->reportFanoutLimitShort(pin, min_max);
}

void
report_fanout_limit_verbose(Pin *pin,
			    const MinMax *min_max)
{
  Sta::sta()->reportFanoutLimitVerbose(pin, min_max);
}

////////////////////////////////////////////////////////////////

PinSeq
check_capacitance_limits(Net *net,
                         bool violators,
                         const Corner *corner,
                         const MinMax *min_max)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkCapacitanceLimits(net, violators, corner, min_max);
}

size_t
max_capacitance_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkCapacitanceLimits(nullptr, true,nullptr,MinMax::max()).size();
}

float
max_capacitance_check_slack()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  float capacitance;
  float slack;
  float limit;
  sta->maxCapacitanceCheck(pin, capacitance, slack, limit);
  return sta->units()->capacitanceUnit()->staToUser(slack);
}

float
max_capacitance_check_limit()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  float capacitance;
  float slack;
  float limit;
  sta->maxCapacitanceCheck(pin, capacitance, slack, limit);
  return sta->units()->capacitanceUnit()->staToUser(limit);
}

void
report_capacitance_limit_short_header()
{
  Sta::sta()->reportCapacitanceLimitShortHeader();
}

void
report_capacitance_limit_short(Pin *pin,
			       const Corner *corner,
			       const MinMax *min_max)
{
  Sta::sta()->reportCapacitanceLimitShort(pin, corner, min_max);
}

void
report_capacitance_limit_verbose(Pin *pin,
				 const Corner *corner,
				 const MinMax *min_max)
{
  Sta::sta()->reportCapacitanceLimitVerbose(pin, corner, min_max);
}

////////////////////////////////////////////////////////////////

void
write_timing_model_cmd(const char *lib_name,
                       const char *cell_name,
                       const char *filename,
                       const Corner *corner)
{
  Sta::sta()->writeTimingModel(lib_name, cell_name, filename, corner);
}

////////////////////////////////////////////////////////////////

void
define_corners_cmd(StringSet *corner_names)
{
  Sta *sta = Sta::sta();
  sta->makeCorners(corner_names);
  delete corner_names;
}

Corner *
cmd_corner()
{
  return Sta::sta()->cmdCorner();
}

void
set_cmd_corner(Corner *corner)
{
  Sta::sta()->setCmdCorner(corner);
}

Corner *
find_corner(const char *corner_name)
{
  return Sta::sta()->findCorner(corner_name);
}

Corners *
corners()
{
  return Sta::sta()->corners();
}

bool
multi_corner()
{
  return Sta::sta()->multiCorner();
}

////////////////////////////////////////////////////////////////

CheckErrorSeq &
check_timing_cmd(bool no_input_delay,
		 bool no_output_delay,
		 bool reg_multiple_clks,
		 bool reg_no_clks,
		 bool unconstrained_endpoints,
		 bool loops,
		 bool generated_clks)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkTiming(no_input_delay, no_output_delay,
				 reg_multiple_clks, reg_no_clks,
				 unconstrained_endpoints,
				 loops, generated_clks);
}

////////////////////////////////////////////////////////////////

PinSet
find_fanin_pins(PinSeq *to,
		bool flat,
		bool startpoints_only,
		int inst_levels,
		int pin_levels,
		bool thru_disabled,
		bool thru_constants)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  PinSet fanin = sta->findFaninPins(to, flat, startpoints_only,
                                    inst_levels, pin_levels,
                                    thru_disabled, thru_constants);
  delete to;
  return fanin;
}

InstanceSet
find_fanin_insts(PinSeq *to,
		 bool flat,
		 bool startpoints_only,
		 int inst_levels,
		 int pin_levels,
		 bool thru_disabled,
		 bool thru_constants)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  InstanceSet fanin = sta->findFaninInstances(to, flat, startpoints_only,
                                              inst_levels, pin_levels,
                                              thru_disabled, thru_constants);
  delete to;
  return fanin;
}

PinSet
find_fanout_pins(PinSeq *from,
		 bool flat,
		 bool endpoints_only,
		 int inst_levels,
		 int pin_levels,
		 bool thru_disabled,
		 bool thru_constants)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  PinSet fanout = sta->findFanoutPins(from, flat, endpoints_only,
                                      inst_levels, pin_levels,
                                      thru_disabled, thru_constants);
  delete from;
  return fanout;
}

InstanceSet
find_fanout_insts(PinSeq *from,
		  bool flat,
		  bool endpoints_only,
		  int inst_levels,
		  int pin_levels,
		  bool thru_disabled,
		  bool thru_constants)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  InstanceSet fanout = sta->findFanoutInstances(from, flat, endpoints_only,
                                                inst_levels, pin_levels,
                                                thru_disabled, thru_constants);
  delete from;
  return fanout;
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

%extend Corner {
const char *name() { return self->name(); }
}
