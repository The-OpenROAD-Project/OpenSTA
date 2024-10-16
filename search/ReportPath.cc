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

#include "ReportPath.hh"

#include "Report.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "TimingRole.hh"
#include "Transition.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Graph.hh"
#include "PortDelay.hh"
#include "ExceptionPath.hh"
#include "InputDrive.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "DcalcAnalysisPt.hh"
#include "ArcDelayCalc.hh"
#include "GraphDelayCalc.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "PathVertex.hh"
#include "PathAnalysisPt.hh"
#include "PathGroup.hh"
#include "CheckMinPulseWidths.hh"
#include "CheckMinPeriods.hh"
#include "CheckMaxSkews.hh"
#include "PathRef.hh"
#include "Search.hh"
#include "PathExpanded.hh"
#include "Latches.hh"
#include "Corner.hh"
#include "Genclks.hh"

namespace sta {

ReportField::ReportField(const char *name,
			 const char *title,
			 int width,
			 bool left_justify,
			 Unit *unit,
			 bool enabled) :
  name_(name),
  title_(stringCopy(title)),
  left_justify_(left_justify),
  unit_(unit),
  enabled_(enabled),
  blank_(nullptr)
{
  setWidth(width);
}

ReportField::~ReportField()
{
  stringDelete(title_);
  stringDelete(blank_);
}

void
ReportField::setProperties(const char *title,
			   int width,
			   bool left_justify)
{
  if (title_)
    stringDelete(title_);
  title_ = stringCopy(title);
  left_justify_ = left_justify;
  setWidth(width);
}

void
ReportField::setWidth(int width)
{
  width_ = width;

  if (blank_)
    stringDelete(blank_);
  blank_ = new char[width_ + 1];
  int i;
  for (i = 0; i < width_; i++)
    blank_[i] = ' ';
  blank_[i] = '\0';
}

void
ReportField::setEnabled(bool enabled)
{
  enabled_ = enabled;
}

////////////////////////////////////////////////////////////////

const float ReportPath::field_blank_ = -1.0;

ReportPath::ReportPath(StaState *sta) :
  StaState(sta),
  format_(ReportPathFormat::full),
  no_split_(false),
  report_sigmas_(false),
  start_end_pt_width_(80),
  plus_zero_(nullptr),
  minus_zero_(nullptr)
{
  setDigits(2);
  makeFields();
  setReportFields(false, false, false, false, false, false);
}

ReportPath::~ReportPath()
{
  delete field_description_;
  delete field_total_;
  delete field_incr_;
  delete field_capacitance_;
  delete field_slew_;
  delete field_fanout_;
  delete field_src_attr_;
  delete field_edge_;
  delete field_case_;

  stringDelete(plus_zero_);
  stringDelete(minus_zero_);
}

void
ReportPath::makeFields()
{
  field_fanout_ = makeField("fanout", "Fanout", 6, false, nullptr, true);
  field_capacitance_ = makeField("capacitance", "Cap", 6, false,
				 units_->capacitanceUnit(), true);
  field_slew_ = makeField("slew", "Slew", 6, false, units_->timeUnit(),
			  true);
  field_incr_ = makeField("incr", "Delay", 6, false, units_->timeUnit(),
			  true);
  field_total_ = makeField("total", "Time", 6, false, units_->timeUnit(),
			   true);
  field_edge_ = makeField("edge", "", 1, false, nullptr, true);
  field_case_ = makeField("case", "case", 11, false, nullptr, false);
  field_description_ = makeField("description", "Description", 36, 
				 true, nullptr, true);
  field_src_attr_ = makeField("src_attr", "Src Attr", 40,
			      true, nullptr, true);
}

ReportField *
ReportPath::makeField(const char *name,
		      const char *title,
		      int width,
		      bool left_justify,
		      Unit *unit,
		      bool enabled)
{
  ReportField *field = new ReportField(name, title, width, left_justify,
				       unit, enabled);
  fields_.push_back(field);
  return field;
}

ReportField *
ReportPath::findField(const char *name)
{
  ReportFieldSeq::Iterator field_iter(fields_);
  while (field_iter.hasNext()) {
    ReportField *field = field_iter.next();
    if (stringEq(name, field->name()))
      return field;
  }
  return nullptr;
}

void
ReportPath::setReportFieldOrder(StringSeq *field_names)
{
  // Disable all fields.
  ReportFieldSeq::Iterator field_iter1(fields_);
  while (field_iter1.hasNext()) {
    ReportField *field = field_iter1.next();
    field->setEnabled(false);
  }

  ReportFieldSeq next_fields;
  StringSeq::Iterator name_iter(field_names);
  while (name_iter.hasNext()) {
    const char *field_name = name_iter.next();
    ReportField *field = findField(field_name);
    if (field) {
      next_fields.push_back(field);
      field->setEnabled(true);
    }
  }
  // Push remaining disabled fields on the end.
  ReportFieldSeq::Iterator field_iter2(fields_);
  while (field_iter2.hasNext()) {
    ReportField *field = field_iter2.next();
    if (!field->enabled())
      next_fields.push_back(field);
  }

  fields_.clear();
  ReportFieldSeq::Iterator field_iter3(next_fields);
  while (field_iter3.hasNext()) {
    ReportField *field = field_iter3.next();
    fields_.push_back(field);
  }
}

void
ReportPath::setReportFields(bool report_input_pin,
			    bool report_net,
			    bool report_cap,
			    bool report_slew,
			    bool report_fanout,
			    bool report_src_attr)
{
  report_input_pin_ = report_input_pin;
  report_net_ = report_net;

  field_capacitance_->setEnabled(report_cap);
  field_slew_->setEnabled(report_slew);
  field_fanout_->setEnabled(report_fanout);
  field_src_attr_->setEnabled(report_src_attr);
  // for debug
  field_case_->setEnabled(false);
}

void
ReportPath::setPathFormat(ReportPathFormat format)
{
  format_ = format;  
}

void
ReportPath::setNoSplit(bool no_split)
{
  no_split_ = no_split;
}

void
ReportPath::setDigits(int digits)
{
  digits_ = digits;

  stringDelete(plus_zero_);
  stringDelete(minus_zero_);
  minus_zero_ = stringPrint("-%.*f", digits_, 0.0);
  plus_zero_  = stringPrint("%.*f", digits_, 0.0);
}

void
ReportPath::setReportSigmas(bool report)
{
  report_sigmas_ = report;
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportPathEnd(PathEnd *end)
{
  reportPathEnd(end, nullptr, true);
}

void
ReportPath::reportPathEnd(PathEnd *end,
			  PathEnd *prev_end,
                          bool last)
{
  switch (format_) {
  case ReportPathFormat::full:
  case ReportPathFormat::full_clock:
  case ReportPathFormat::full_clock_expanded:
    end->reportFull(this);
    reportBlankLine();
    reportBlankLine();
    break;
  case ReportPathFormat::shorter:
    end->reportShort(this);
    reportBlankLine();
    reportBlankLine();
    break;
  case ReportPathFormat::endpoint:
    reportEndpointHeader(end, prev_end);
    reportEndLine(end);
    break;
  case ReportPathFormat::summary:
    reportSummaryLine(end);
    break;
  case ReportPathFormat::slack_only:
    reportSlackOnly(end);
    break;
  case ReportPathFormat::json:
    reportJson(end, last);
    break;
  }
}

void
ReportPath::reportPathEnds(PathEndSeq *ends)
{
  reportPathEndHeader();
  PathEndSeq::Iterator end_iter(ends);
  PathEnd *prev_end = nullptr;
  while (end_iter.hasNext()) {
    PathEnd *end = end_iter.next();
    reportEndpointHeader(end, prev_end);
    end->reportFull(this);
    reportBlankLine();
    prev_end = end;
  }
  reportPathEndFooter();
}

void
ReportPath::reportPathEndHeader()
{
  switch (format_) {
  case ReportPathFormat::full:
  case ReportPathFormat::full_clock:
  case ReportPathFormat::full_clock_expanded:
  case ReportPathFormat::shorter:
  case ReportPathFormat::endpoint:
    break;
  case ReportPathFormat::summary:
    reportSummaryHeader();
    break;
  case ReportPathFormat::slack_only:
    reportSlackOnlyHeader();
    break;
  case ReportPathFormat::json:
    reportJsonHeader();
    break;
  }
}

void
ReportPath::reportPathEndFooter()
{
  string header;
  switch (format_) {
  case ReportPathFormat::full:
  case ReportPathFormat::full_clock:
  case ReportPathFormat::full_clock_expanded:
  case ReportPathFormat::shorter:
    break;
  case ReportPathFormat::endpoint:
  case ReportPathFormat::summary:
  case ReportPathFormat::slack_only:
    reportBlankLine();
    break;
  case ReportPathFormat::json:
    reportJsonFooter();
    break;
  }
}

void
ReportPath::reportEndpointHeader(PathEnd *end,
				 PathEnd *prev_end)
{
  PathGroup *prev_group = nullptr;
  if (prev_end)
    prev_group = search_->pathGroup(prev_end);
  PathGroup *group = search_->pathGroup(end);
  if (group && group != prev_group) {
    if (prev_group)
      reportBlankLine();
    const char *setup_hold = (end->minMax(this) == MinMax::min())
      ? "min_delay/hold"
      : "max_delay/setup";
    report_->reportLine("%s group %s",
                        setup_hold,
                        group->name());
    reportBlankLine();
    reportEndHeader();
  }
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportShort(const PathEndUnconstrained *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
}

void
ReportPath::reportShort(const PathEndUnconstrained *end,
			PathExpanded &expanded)
{
  reportStartpoint(end, expanded);
  reportUnclockedEndpoint(end, "internal pin");
  reportGroup(end);
}

void
ReportPath::reportFull(const PathEndUnconstrained *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
  reportBlankLine();

  reportPath(end, expanded);
  reportLine("data arrival time", end->dataArrivalTimeOffset(this),
	     end->pathEarlyLate(this));
  reportDashLine();
  report_->reportLine("(Path is unconstrained)");
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportShort(const PathEndCheck *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
}

void
ReportPath::reportShort(const PathEndCheck *end,
			PathExpanded &expanded)
{
  reportStartpoint(end, expanded);
  reportEndpoint(end);
  reportGroup(end);
}

void
ReportPath::reportFull(const PathEndCheck *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
  reportSrcPathArrival(end, expanded);
  reportTgtClk(end);
  reportRequired(end, checkRoleString(end));
  reportSlack(end);
}

string
ReportPath::checkRoleString(const PathEnd *end)
{
  const char *check_role = end->checkRole(this)->asString();
  return stdstrPrint("library %s time", check_role);
}

void
ReportPath::reportEndpoint(const PathEndCheck *end)
{
  Instance *inst = network_->instance(end->vertex(this)->pin());
  const char *inst_name = cmd_network_->pathName(inst);
  string clk_name = tgtClkName(end);
  const char *rise_fall = asRisingFalling(end->targetClkEndTrans(this));
  const TimingRole *check_role = end->checkRole(this);
  const TimingRole *check_generic_role = check_role->genericRole();
  if (check_role == TimingRole::recovery()
      || check_role == TimingRole::removal()) {
    const char *check_role_name = check_role->asString();
    auto reason = stdstrPrint("%s check against %s-edge clock %s",
			      check_role_name,
			      rise_fall,
			      clk_name.c_str());
    reportEndpoint(inst_name, reason);
  }
  else if (check_generic_role == TimingRole::setup()
	   || check_generic_role == TimingRole::hold()) {
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell->isClockGate()) {
      auto reason = stdstrPrint("%s clock gating-check end-point clocked by %s",
				rise_fall, clk_name.c_str());
      reportEndpoint(inst_name, reason);
    }
    else {
      const char *reg_desc = clkRegLatchDesc(end);
      auto reason = stdstrPrint("%s clocked by %s", reg_desc, clk_name.c_str());
      reportEndpoint(inst_name, reason);
    }
  }
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportShort(const PathEndLatchCheck *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
}

void
ReportPath::reportShort(const PathEndLatchCheck *end,
			PathExpanded &expanded)
{
  reportStartpoint(end, expanded);
  reportEndpoint(end);
  reportGroup(end);
}

void
ReportPath::reportFull(const PathEndLatchCheck *end)
{
  PathExpanded expanded(end->path(), this);
  const EarlyLate *early_late = end->pathEarlyLate(this);
  reportShort(end, expanded);
  reportBlankLine();

  PathDelay *path_delay = end->pathDelay();
  bool ignore_clk_latency = path_delay && path_delay->ignoreClkLatency();
  if (ignore_clk_latency) {
    // Based on reportSrcPath.
    reportPathHeader();
    reportPath3(end->path(), expanded, false, false, 0.0,
		end->sourceClkOffset(this));
  }
  else
    reportSrcPath(end, expanded);
  reportLine("data arrival time", end->dataArrivalTimeOffset(this), early_late);
  reportBlankLine();

  Required req_time;
  Arrival borrow, adjusted_data_arrival, time_given_to_startpoint;
  end->latchRequired(this, req_time, borrow, adjusted_data_arrival,
		     time_given_to_startpoint);
  // Adjust required to requiredTimeOffset.
  req_time += end->sourceClkOffset(this);
  if (path_delay) {
    float delay = path_delay->delay();
    reportLine("max_delay", delay, delay, early_late);
    if (!ignore_clk_latency) {
      if (reportClkPath()
	  && isPropagated(end->targetClkPath()))
	reportTgtClk(end, delay);
      else {
	Delay delay1(delay);
	reportCommonClkPessimism(end, delay1);
      }
    }
  }
  else
    reportTgtClk(end);

  if (delayGreaterEqual(borrow, 0.0, this))
    reportLine("time borrowed from endpoint", borrow, req_time, early_late);
  else
    reportLine("time given to endpoint", borrow, req_time, early_late);
  reportLine("data required time", req_time, early_late);
  reportDashLine();
  reportSlack(end);
  if (end->checkGenericRole(this) == TimingRole::setup()
      && !ignore_clk_latency) {
    reportBlankLine();
    reportBorrowing(end, borrow, time_given_to_startpoint);
  }
}

void
ReportPath::reportEndpoint(const PathEndLatchCheck *end)
{
  Instance *inst = network_->instance(end->vertex(this)->pin());
  const char *inst_name = cmd_network_->pathName(inst);
  string clk_name = tgtClkName(end);
  const char *reg_desc = latchDesc(end);
  auto reason = stdstrPrint("%s clocked by %s", reg_desc, clk_name.c_str());
  reportEndpoint(inst_name, reason);
}

const char *
ReportPath::latchDesc(const PathEndLatchCheck *end)
{
  TimingArc *check_arc = end->checkArc();
  const RiseFall *en_rf = check_arc->fromEdge()->asRiseFall()->opposite();
  return latchDesc(en_rf);
}

void
ReportPath::reportBorrowing(const PathEndLatchCheck *end,
			    Arrival &borrow,
			    Arrival &time_given_to_startpoint)
{
  Delay open_latency, latency_diff, max_borrow;
  float nom_pulse_width, open_uncertainty;
  Crpr open_crpr, crpr_diff;
  bool borrow_limit_exists;
  const EarlyLate *early_late = EarlyLate::late();
  end->latchBorrowInfo(this, nom_pulse_width, open_latency, latency_diff,
		       open_uncertainty, open_crpr, crpr_diff,
		       max_borrow, borrow_limit_exists);
  report_->reportLine("Time Borrowing Information");
  reportDashLineTotal();
  if (borrow_limit_exists)
    reportLineTotal("user max time borrow", max_borrow, early_late);
  else {
    string tgt_clk_name = tgtClkName(end);
    Arrival tgt_clk_width = end->targetClkWidth(this);
    const Path *tgt_clk_path = end->targetClkPath();
    if (tgt_clk_path->clkInfo(search_)->isPropagated()) {
      auto width_msg = stdstrPrint("%s nominal pulse width", tgt_clk_name.c_str());
      reportLineTotal(width_msg.c_str(), nom_pulse_width, early_late);
      if (!delayZero(latency_diff))
	reportLineTotalMinus("clock latency difference", latency_diff, early_late);
    }
    else {
      auto width_msg = stdstrPrint("%s pulse width", tgt_clk_name.c_str());
      reportLineTotal(width_msg.c_str(), tgt_clk_width, early_late);
    }
    ArcDelay margin = end->margin(this);
    reportLineTotalMinus("library setup time", margin, early_late);
    reportDashLineTotal();
    if (!delayZero(crpr_diff))
      reportLineTotalMinus("CRPR difference", crpr_diff, early_late);
    reportLineTotal("max time borrow", max_borrow, early_late);
  }
  if (delayGreater(borrow, delay_zero, this)
      && (!fuzzyZero(open_uncertainty)
	  || !delayZero(open_crpr))) {
    reportDashLineTotal();
    reportLineTotal("actual time borrow", borrow, early_late);
    if (!fuzzyZero(open_uncertainty))
      reportLineTotal("open edge uncertainty", open_uncertainty, early_late);
    if (!delayZero(open_crpr))
      reportLineTotal("open edge CRPR", open_crpr, early_late);
    reportDashLineTotal();
    reportLineTotal("time given to startpoint", time_given_to_startpoint, early_late);
  }
  else
    reportLineTotal("actual time borrow", borrow, early_late);
  reportDashLineTotal();
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportShort(const PathEndPathDelay *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
}

void
ReportPath::reportShort(const PathEndPathDelay *end,
			PathExpanded &expanded)
{
  reportStartpoint(end, expanded);
  if (end->targetClk(this))
    reportEndpoint(end);
  else
    reportUnclockedEndpoint(end, "internal path endpoint");
  reportGroup(end);
}

void
ReportPath::reportEndpoint(const PathEndPathDelay *end)
{
  if (end->hasOutputDelay())
    reportEndpointOutputDelay(end);
  else {
    Instance *inst = network_->instance(end->vertex(this)->pin());
    const char *inst_name = cmd_network_->pathName(inst);
    string clk_name = tgtClkName(end);
    const char *reg_desc = clkRegLatchDesc(end);
    auto reason = stdstrPrint("%s clocked by %s", reg_desc, clk_name.c_str());
    reportEndpoint(inst_name, reason);
  }
}

void
ReportPath::reportFull(const PathEndPathDelay *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
  const EarlyLate *early_late = end->pathEarlyLate(this);

  // Based on reportSrcPathArrival.
  reportBlankLine();
  PathDelay *path_delay = end->pathDelay();
  if (end->ignoreClkLatency(this)) {
    // Based on reportSrcPath.
    reportPathHeader();
    reportPath3(end->path(), expanded, false, false, 0.0,
                end->sourceClkOffset(this));
  }
  else
    reportSrcPath(end, expanded);
  reportLine("data arrival time", end->dataArrivalTimeOffset(this), early_late);
  reportBlankLine();

  ArcDelay margin = end->margin(this);
  MinMax *min_max = path_delay->minMax()->asMinMax();
  if (min_max == MinMax::max())
    margin = -margin;

  const char *min_max_str = min_max->asString();
  auto delay_msg = stdstrPrint("%s_delay", min_max_str);
  float delay = path_delay->delay();
  reportLine(delay_msg.c_str(), delay, delay, early_late);
  if (!path_delay->ignoreClkLatency()) {
    const Clock *tgt_clk = end->targetClk(this);
    if (tgt_clk) {
      const Path *tgt_clk_path = end->targetClkPath();
      if (reportClkPath()
	  && isPropagated(tgt_clk_path, tgt_clk))
	reportTgtClk(end, delay, 0.0, true);
      else {
	Arrival tgt_clk_delay = end->targetClkDelay(this);
	Arrival tgt_clk_arrival = delay + tgt_clk_delay;
	if (!delayZero(tgt_clk_delay))
	  reportLine(clkNetworkDelayIdealProp(isPropagated(tgt_clk_path)),
		     tgt_clk_delay, tgt_clk_arrival, early_late);
	reportClkUncertainty(end, tgt_clk_arrival);
	reportCommonClkPessimism(end, tgt_clk_arrival);
      }
    }
  }
  if (end->pathDelayMarginIsExternal())
    reportRequired(end, "output external delay");
  else
    reportRequired(end, checkRoleString(end));
  reportSlack(end);
}

bool
ReportPath::isPropagated(const Path *clk_path)
{
  return clk_path->clkInfo(search_)->isPropagated();
}

bool
ReportPath::isPropagated(const Path *clk_path,
			 const Clock *clk)
{
  if (clk_path)
    return clk_path->clkInfo(search_)->isPropagated();
  else
    return clk->isPropagated();
}

const char *
ReportPath::clkNetworkDelayIdealProp(bool is_prop)
{
  if (is_prop)
    return "clock network delay (propagated)";
  else
    return "clock network delay (ideal)";
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportShort(const PathEndOutputDelay *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
}

void
ReportPath::reportShort(const PathEndOutputDelay *end,
			PathExpanded &expanded)
{
  reportStartpoint(end, expanded);
  reportEndpoint(end);
  reportGroup(end);
}

void
ReportPath::reportFull(const PathEndOutputDelay *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
  reportSrcPathArrival(end, expanded);
  reportTgtClk(end);
  reportRequired(end, "output external delay");
  reportSlack(end);
}

void
ReportPath::reportEndpoint(const PathEndOutputDelay *end)
{
  reportEndpointOutputDelay(end);
}

void
ReportPath::reportEndpointOutputDelay(const PathEndClkConstrained *end)
{
  Vertex *vertex = end->vertex(this);
  Pin *pin = vertex->pin();
  const char *pin_name = cmd_network_->pathName(pin);
  const Clock *tgt_clk = end->targetClk(this);
  if (network_->isTopLevelPort(pin)) {
    // Pin direction is "output" even for bidirects.
    if (tgt_clk) {
      string clk_name = tgtClkName(end);
      auto reason = stdstrPrint("output port clocked by %s", clk_name.c_str());
      reportEndpoint(pin_name, reason);
    }
    else
      reportEndpoint(pin_name, "output port");
  }
  else {
    if (tgt_clk) {
      string clk_name = tgtClkName(end);
      auto reason = stdstrPrint("internal path endpoint clocked by %s",
                                clk_name.c_str());

      reportEndpoint(pin_name, reason);
    }
    else
      reportEndpoint(pin_name, "internal path endpoint");
  }
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportShort(const PathEndGatedClock *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
}

void
ReportPath::reportShort(const PathEndGatedClock *end,
			PathExpanded &expanded)
{
  reportStartpoint(end, expanded);
  reportEndpoint(end);
  reportGroup(end);
}

void
ReportPath::reportFull(const PathEndGatedClock *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
  reportSrcPathArrival(end, expanded);
  reportTgtClk(end);
  reportRequired(end, checkRoleReason(end));
  reportSlack(end);
}

void
ReportPath::reportEndpoint(const PathEndGatedClock *end)
{
  Instance *inst = network_->instance(end->vertex(this)->pin());
  const char *inst_name = cmd_network_->pathName(inst);
  string clk_name = tgtClkName(end);
  const RiseFall *clk_end_rf = end->targetClkEndTrans(this);
  const RiseFall *clk_rf =
    (end->minMax(this) == MinMax::max()) ? clk_end_rf : clk_end_rf->opposite();
  const char *rise_fall = asRisingFalling(clk_rf);
  // Note that target clock transition is ignored.
  auto reason = stdstrPrint("%s clock gating-check end-point clocked by %s",
			    rise_fall,
			    clk_name.c_str());
  reportEndpoint(inst_name, reason);
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportShort(const PathEndDataCheck *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
}

void
ReportPath::reportShort(const PathEndDataCheck *end,
			PathExpanded &expanded)
{
  reportStartpoint(end, expanded);
  reportEndpoint(end);
  reportGroup(end);

}

void
ReportPath::reportFull(const PathEndDataCheck *end)
{
  PathExpanded expanded(end->path(), this);
  reportShort(end, expanded);
  reportSrcPathArrival(end, expanded);

  // Data check target clock path reporting resembles 
  // both source (reportSrcPath) and target (reportTgtClk) clocks. 
  // It is like a source because it can be a non-clock path.
  // It is like a target because crpr and uncertainty are reported.
  // It is always propagated, even if the clock is ideal.
  reportTgtClk(end, 0.0, true);
  const PathVertex *data_clk_path = end->dataClkPath();
  if (!data_clk_path->isClock(this)) {
    // Report the path from the clk network to the data check.
    PathExpanded clk_expanded(data_clk_path, this);
    float src_offset = end->sourceClkOffset(this);
    Delay clk_delay = end->targetClkDelay(this);
    Arrival clk_arrival = end->targetClkArrival(this);
    const ClockEdge *tgt_clk_edge = end->targetClkEdge(this);
    float prev = delayAsFloat(clk_arrival) + src_offset;
    float offset = prev - delayAsFloat(clk_delay) - tgt_clk_edge->time();
    reportPath5(data_clk_path, clk_expanded, clk_expanded.startIndex(),
		clk_expanded.size() - 1,
		data_clk_path->clkInfo(search_)->isPropagated(), false,
		// Delay to startpoint is already included.
		prev, offset);
  }
  reportRequired(end, checkRoleReason(end));
  reportSlack(end);
}

void
ReportPath::reportEndpoint(const PathEndDataCheck *end)
{
  Instance *inst = network_->instance(end->vertex(this)->pin());
  const char *inst_name = cmd_network_->pathName(inst);
  const char *tgt_clk_rf = asRisingFalling(end->dataClkPath()->transition(this));
  const char *tgt_clk_name = end->targetClk(this)->name();
  auto reason = stdstrPrint("%s edge-triggered data to data check clocked by %s",
			    tgt_clk_rf,
			    tgt_clk_name);
  reportEndpoint(inst_name, reason);
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportEndHeader()
{
  string line;
  // Line one.
  reportDescription("", line);
  line += ' ';
  reportField("Required", field_total_, line);
  line += ' ';
  reportField("Actual", field_total_, line);
  report_->reportLineString(line);

  // Line two.
  line.clear();
  reportDescription("Endpoint", line);
  line += ' ';
  reportField("Delay", field_total_, line);
  line += ' ';
  reportField("Delay", field_total_, line);
  line += ' ';
  reportField("Slack", field_total_, line);
  report_->reportLineString(line);

  reportDashLine(field_description_->width() + field_total_->width() * 3 + 3);
}

void
ReportPath::reportEndLine(PathEnd *end)
{
  string line;
  string endpoint = pathEndpoint(end);
  reportDescription(endpoint.c_str(), line);
  const EarlyLate *early_late = end->pathEarlyLate(this);
  reportSpaceFieldDelay(end->requiredTimeOffset(this), early_late, line);
  reportSpaceFieldDelay(end->dataArrivalTimeOffset(this), early_late, line);
  reportSpaceSlack(end, line);
  report_->reportLineString(line);
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportSummaryHeader()
{
  string line;
  reportDescription("Startpoint", line);
  line += ' ';
  reportDescription("Endpoint", line);
  line += ' ';
  reportField("Slack", field_total_, line);
  report_->reportLineString(line);

  reportDashLine(field_description_->width() * 2 + field_total_->width() + 1);
}

void
ReportPath::reportSummaryLine(PathEnd *end)
{
  string line;
  PathExpanded expanded(end->path(), this);
  const EarlyLate *early_late = end->pathEarlyLate(this);
  auto startpoint = pathStartpoint(end, expanded);
  reportDescription(startpoint.c_str(), line);
  line += ' ';
  auto endpoint = pathEndpoint(end);
  reportDescription(endpoint.c_str(), line);
  if (end->isUnconstrained())
    reportSpaceFieldDelay(end->dataArrivalTimeOffset(this), early_late, line);
  else
    reportSpaceFieldDelay(end->slack(this), EarlyLate::early(), line);
  report_->reportLineString(line);
}

string
ReportPath::pathStartpoint(PathEnd *end,
			   PathExpanded &expanded)
{
  PathRef *start = expanded.startPath();
  Pin *pin = start->pin(graph_);
  const char *pin_name = cmd_network_->pathName(pin);
  if (network_->isTopLevelPort(pin)) {
    PortDirection *dir = network_->direction(pin);
    return stdstrPrint("%s (%s)", pin_name, dir->name());
  }
  else {
    Instance *inst = network_->instance(end->vertex(this)->pin());
    const char *cell_name = cmd_network_->name(network_->cell(inst));
    return stdstrPrint("%s (%s)", pin_name, cell_name);
  }
}

string
ReportPath::pathEndpoint(PathEnd *end)
{
  Pin *pin = end->vertex(this)->pin();
  const char *pin_name = cmd_network_->pathName(pin);
  if (network_->isTopLevelPort(pin)) {
    PortDirection *dir = network_->direction(pin);
    return stdstrPrint("%s (%s)", pin_name, dir->name());
  }
  else {
    Instance *inst = network_->instance(end->vertex(this)->pin());
    const char *cell_name = cmd_network_->name(network_->cell(inst));
    return stdstrPrint("%s (%s)", pin_name, cell_name);
  }
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportJsonHeader()
{
  report_->reportLine("{\"checks\": [");
}

void
ReportPath::reportJsonFooter()
{
  report_->reportLine("]");
  report_->reportLine("}");
}

void
ReportPath::reportJson(const PathEnd *end,
                       bool last)
{
  string result;
  result += "{\n";
  stringAppend(result, "  \"type\": \"%s\",\n", end->typeName());
  stringAppend(result, "  \"path_group\": \"%s\",\n",
               search_->pathGroup(end)->name());
  stringAppend(result, "  \"path_type\": \"%s\",\n",
               end->minMax(this)->asString());

  PathExpanded expanded(end->path(), this);
  const Pin *startpoint = expanded.startPath()->vertex(this)->pin();
  const Pin *endpoint = expanded.endPath()->vertex(this)->pin();
  stringAppend(result, "  \"startpoint\": \"%s\",\n",
               network_->pathName(startpoint));
  stringAppend(result, "  \"endpoint\": \"%s\",\n",
               network_->pathName(endpoint));

  const ClockEdge *src_clk_edge = end->sourceClkEdge(this);
  const PathVertex *tgt_clk_path = end->targetClkPath();
  if (src_clk_edge) {
    stringAppend(result, "  \"source_clock\": \"%s\",\n",
                 src_clk_edge->clock()->name());
    stringAppend(result, "  \"source_clock_edge\": \"%s\",\n",
                 src_clk_edge->transition()->name());
  }
  reportJson(expanded, "source_path", 2, !end->isUnconstrained(), result);

  const ClockEdge *tgt_clk_edge = end->targetClkEdge(this);
  if (tgt_clk_edge) {
    stringAppend(result, "  \"target_clock\": \"%s\",\n",
                 tgt_clk_edge->clock()->name());
    stringAppend(result, "  \"target_clock_edge\": \"%s\",\n",
                 tgt_clk_edge->transition()->name());
  }
  if (tgt_clk_path)
    reportJson(end->targetClkPath(), "target_clock_path", 2, true, result);

  if (end->checkRole(this)) {
    stringAppend(result, "  \"data_arrival_time\": %.3e,\n",
                 end->dataArrivalTimeOffset(this));

    const MultiCyclePath *mcp = end->multiCyclePath();
    if (mcp)
      stringAppend(result, "  \"multi_cycle_path\": %d,\n",
                   mcp->pathMultiplier());

    PathDelay *path_delay = end->pathDelay();
    if (path_delay)
      stringAppend(result, "  \"path_delay\": %.3e,\n",
                   path_delay->delay());

    stringAppend(result, "  \"crpr\": %.3e,\n", end->checkCrpr(this));
    stringAppend(result, "  \"margin\": %.3e,\n", end->margin(this));
    stringAppend(result, "  \"required_time\": %.3e,\n",
                 end->requiredTimeOffset(this));
    stringAppend(result, "  \"slack\": %.3e\n", end->slack(this));
  }
  result += "}";
  if (!last)
    result += ",";
  report_->reportLineString(result);
}

void
ReportPath::reportJson(const Path *path)
{
  string result;
  result += "{\n";
  reportJson(path, "path", 0, false, result);
  result += "}\n";
  report_->reportLineString(result);
}

void
ReportPath::reportJson(const Path *path,
                       const char *path_name,
                       int indent,
                       bool trailing_comma,
                       string &result)
{
  PathExpanded expanded(path, this);
  reportJson(expanded, path_name, indent, trailing_comma, result);
}

void
ReportPath::reportJson(const PathExpanded &expanded,
                       const char *path_name,
                       int indent,
                       bool trailing_comma,
                       string &result)
{
  stringAppend(result, "%*s\"%s\": [\n", indent, "", path_name);
  for (size_t i = 0; i < expanded.size(); i++) {
    const PathRef *path = expanded.path(i);
    const Pin *pin = path->vertex(this)->pin();
    stringAppend(result, "%*s  {\n", indent, "");
    stringAppend(result, "%*s    \"pin\": \"%s\",\n",
                 indent, "",
                 network_->pathName(pin));
    double x, y;
    bool exists;
    network_->location(pin, x, y, exists);
    if (exists) {
      stringAppend(result, "%*s    \"x\": %.9f,\n", indent, "", x);
      stringAppend(result, "%*s    \"y\": %.9f,\n", indent, "", y);
    }

    stringAppend(result, "%*s    \"arrival\": %.3e,\n",
                 indent, "",
                 delayAsFloat(path->arrival(this)));
    stringAppend(result, "%*s    \"slew\": %.3e\n",
                 indent, "",
                 delayAsFloat(path->slew(this)));
    stringAppend(result, "%*s  }%s\n",
                 indent, "",
                 (i < expanded.size() - 1) ? "," : "");
  }
  stringAppend(result, "%*s]%s\n",
               indent, "",
               trailing_comma ? "," : "");
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportSlackOnlyHeader()
{
  string line;
  reportDescription("Group", line);
  line += ' ';
  reportField("Slack", field_total_, line);
  report_->reportLineString(line);

  reportDashLine(field_description_->width() + field_total_->width() + 1);
}

void
ReportPath::reportSlackOnly(PathEnd *end)
{
  string line;
  const EarlyLate *early_late = end->pathEarlyLate(this);
  reportDescription(search_->pathGroup(end)->name(), line);
  if (end->isUnconstrained())
    reportSpaceFieldDelay(end->dataArrivalTimeOffset(this), early_late, line);
  else
    reportSpaceFieldDelay(end->slack(this), early_late, line);
  report_->reportLineString(line);
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportMpwCheck(MinPulseWidthCheck *check,
			   bool verbose)
{
  if (verbose) {
    reportVerbose(check);
    reportBlankLine();
  }
  else {
    reportMpwHeaderShort();
    reportShort(check);
  }
  reportBlankLine();
}

void
ReportPath::reportMpwChecks(MinPulseWidthCheckSeq *checks,
			    bool verbose)
{
  if (!checks->empty()) {
    if (verbose) {
      MinPulseWidthCheckSeq::Iterator check_iter(checks);
      while (check_iter.hasNext()) {
	MinPulseWidthCheck *check = check_iter.next();
	reportVerbose(check);
        reportBlankLine();
      }
    }
    else {
      reportMpwHeaderShort();
      MinPulseWidthCheckSeq::Iterator check_iter(checks);
      while (check_iter.hasNext()) {
	MinPulseWidthCheck *check = check_iter.next();
	reportShort(check);
      }
    }
    reportBlankLine();
  }
}

void
ReportPath::reportMpwHeaderShort()
{
  string line;
  reportDescription("", line);
  line += ' ';
  reportField("Required", field_total_, line);
  line += ' ';
  reportField("Actual", field_total_, line);
  report_->reportLineString(line);

  line.clear();
  reportDescription("Pin", line);
  line += ' ';
  reportField("Width", field_total_, line);
  line += ' ';
  reportField("Width", field_total_, line);
  line += ' ';
  reportField("Slack", field_total_, line);
  report_->reportLineString(line);

  reportDashLine(field_description_->width() + field_total_->width() * 3 + 3);
}

void
ReportPath::reportShort(MinPulseWidthCheck *check)
{
  string line;
  const char *pin_name = cmd_network_->pathName(check->pin(this));
  const char *hi_low = mpwCheckHiLow(check);
  auto what = stdstrPrint("%s (%s)", pin_name, hi_low);
  reportDescription(what.c_str(), line);
  reportSpaceFieldTime(check->minWidth(this), line);
  reportSpaceFieldDelay(check->width(this), EarlyLate::late(), line);
  reportSpaceSlack(check->slack(this), line);
  report_->reportLineString(line);
}

void
ReportPath::reportVerbose(MinPulseWidthCheck *check)
{
  string line;
  const char *pin_name = cmd_network_->pathName(check->pin(this));
  line += "Pin: ";
  line += pin_name;
  report_->reportLineString(line);

  report_->reportLine("Check: sequential_clock_pulse_width");
  reportBlankLine();
  reportPathHeader();

  const EarlyLate *open_el = EarlyLate::late();
  const ClockEdge *open_clk_edge = check->openClkEdge(this);
  const Clock *open_clk = open_clk_edge->clock();
  const char *open_clk_name = open_clk->name();
  const char *open_rise_fall = asRiseFall(open_clk_edge->transition());
  float open_clk_time = open_clk_edge->time();
  auto open_clk_msg = stdstrPrint("clock %s (%s edge)", open_clk_name, open_rise_fall);
  reportLine(open_clk_msg.c_str(), open_clk_time, open_clk_time, open_el);

  Arrival open_arrival = check->openArrival(this);
  bool is_prop = isPropagated(check->openPath());
  const char *clk_ideal_prop = clkNetworkDelayIdealProp(is_prop);
  reportLine(clk_ideal_prop, check->openDelay(this), open_arrival, open_el);
  reportLine(pin_name, delay_zero, open_arrival, open_el);
  reportLine("open edge arrival time", open_arrival, open_el);
  reportBlankLine();

  const EarlyLate *close_el = EarlyLate::late();
  const ClockEdge *close_clk_edge = check->closeClkEdge(this);
  const Clock *close_clk = close_clk_edge->clock();
  const char *close_clk_name = close_clk->name();
  const char *close_rise_fall = asRiseFall(close_clk_edge->transition());
  float close_offset = check->closeOffset(this);
  float close_clk_time = close_clk_edge->time() + close_offset;
  auto close_clk_msg = stdstrPrint("clock %s (%s edge)", close_clk_name, close_rise_fall);
  reportLine(close_clk_msg.c_str(), close_clk_time, close_clk_time, close_el);
  Arrival close_arrival = check->closeArrival(this) + close_offset;
  reportLine(clk_ideal_prop, check->closeDelay(this), close_arrival, close_el);
  reportLine(pin_name, delay_zero, close_arrival, close_el);

  if (sdc_->crprEnabled()) {
    Crpr pessimism = check->checkCrpr(this);
    close_arrival += pessimism;
    reportLine("clock reconvergence pessimism", pessimism, close_arrival, close_el);
  }
  reportLine("close edge arrival time", close_arrival, close_el);
  reportDashLine();

  float min_width = check->minWidth(this);
  const char *hi_low = mpwCheckHiLow(check);
  auto rpw_msg = stdstrPrint("required pulse width (%s)", hi_low);
  reportLine(rpw_msg.c_str(), min_width, EarlyLate::early());
  reportLine("actual pulse width", check->width(this), EarlyLate::early());
  reportDashLine();
  reportSlack(check->slack(this));
}

const char *
ReportPath::mpwCheckHiLow(MinPulseWidthCheck *check)
{
  if (check->openTransition(this) == RiseFall::rise())
    return "high";
  else
    return "low";
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportCheck(MinPeriodCheck *check,
			bool verbose)
{
  if (verbose) {
    reportVerbose(check);
    reportBlankLine();
  }
  else {
    reportPeriodHeaderShort();
    reportShort(check);
  }
  reportBlankLine();
}

void
ReportPath::reportChecks(MinPeriodCheckSeq *checks,
			 bool verbose)
{
  if (!checks->empty()) {
    if (verbose) {
      MinPeriodCheckSeq::Iterator check_iter(checks);
      while (check_iter.hasNext()) {
	MinPeriodCheck *check = check_iter.next();
	reportVerbose(check);
        reportBlankLine();
      }
    }
    else {
      reportPeriodHeaderShort();
      MinPeriodCheckSeq::Iterator check_iter(checks);
      while (check_iter.hasNext()) {
	MinPeriodCheck *check = check_iter.next();
	reportShort(check);
      }
    }
    reportBlankLine();
  }
}

void
ReportPath::reportPeriodHeaderShort()
{
  string line;
  reportDescription("", line);
  line += ' ';
  reportField("", field_total_, line);
  line += ' ';
  reportField("Min", field_total_, line);
  line += ' ';
  reportField("", field_total_, line);
  report_->reportLineString(line);

  line.clear();
  reportDescription("Pin", line);
  line += ' ';
  reportField("Period", field_total_, line);
  line += ' ';
  reportField("Period", field_total_, line);
  line += ' ';
  reportField("Slack", field_total_, line);
  report_->reportLineString(line);

  reportDashLine(field_description_->width() + field_total_->width() * 3 + 3);
}

void
ReportPath::reportShort(MinPeriodCheck *check)
{
  string line;
  const char *pin_name = cmd_network_->pathName(check->pin());
  reportDescription(pin_name, line);
  reportSpaceFieldDelay(check->period(), EarlyLate::early(), line);
  reportSpaceFieldDelay(check->minPeriod(this), EarlyLate::early(), line);
  reportSpaceSlack(check->slack(this), line);
  report_->reportLineString(line);
}

void
ReportPath::reportVerbose(MinPeriodCheck *check)
{
  string line;
  const char *pin_name = cmd_network_->pathName(check->pin());
  line += "Pin: ";
  line += pin_name;
  report_->reportLineString(line);

  reportLine("period", check->period(), EarlyLate::early());
  reportLine("min period", -check->minPeriod(this), EarlyLate::early());
  reportDashLine();

  reportSlack(check->slack(this));
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportCheck(MaxSkewCheck *check,
			bool verbose)
{
  if (verbose) {
    reportVerbose(check);
    reportBlankLine();
  }
  else {
    reportMaxSkewHeaderShort();
    reportShort(check);
  }
  reportBlankLine();
}

void
ReportPath::reportChecks(MaxSkewCheckSeq *checks,
			 bool verbose)
{
  if (!checks->empty()) {
    if (verbose) {
      MaxSkewCheckSeq::Iterator check_iter(checks);
      while (check_iter.hasNext()) {
	MaxSkewCheck *check = check_iter.next();
	reportVerbose(check);
      }
    }
    else {
      reportMaxSkewHeaderShort();
      MaxSkewCheckSeq::Iterator check_iter(checks);
      while (check_iter.hasNext()) {
	MaxSkewCheck *check = check_iter.next();
	reportShort(check);
      }
    }
    reportBlankLine();
  }
}

void
ReportPath::reportMaxSkewHeaderShort()
{
  string line;
  reportDescription("", line);
  line += ' ';
  reportField("Required", field_total_, line);
  line += ' ';
  reportField("Actual", field_total_, line);
  line += ' ';
  reportField("", field_total_, line);
  report_->reportLineString(line);

  line.clear();
  reportDescription("Pin", line);
  line += ' ';
  reportField("Skew", field_total_, line);
  line += ' ';
  reportField("Skew", field_total_, line);
  line += ' ';
  reportField("Slack", field_total_, line);
  report_->reportLineString(line);

  reportDashLine(field_description_->width() + field_total_->width() * 3 + 3);
}

void
ReportPath::reportShort(MaxSkewCheck *check)
{
  string line;
  Pin *clk_pin = check->clkPin(this);
  const char *clk_pin_name = network_->pathName(clk_pin);
  TimingArc *check_arc = check->checkArc();
  auto what = stdstrPrint("%s (%s->%s)",
			  clk_pin_name,
			  check_arc->fromEdge()->asString(),
			  check_arc->toEdge()->asString());
  reportDescription(what.c_str(), line);
  const EarlyLate *early_late = EarlyLate::early();
  reportSpaceFieldDelay(check->maxSkew(this), early_late, line);
  reportSpaceFieldDelay(check->skew(this), early_late, line);
  reportSpaceSlack(check->slack(this), line);
  report_->reportLineString(line);
}

void
ReportPath::reportVerbose(MaxSkewCheck *check)
{
  string line;
  const char *clk_pin_name = cmd_network_->pathName(check->clkPin(this));
  line += "Constrained Pin: ";
  line += clk_pin_name;
  report_->reportLineString(line);

  const char *ref_pin_name = cmd_network_->pathName(check->refPin(this));
  line = "Reference   Pin: ";
  line += ref_pin_name;
  report_->reportLineString(line);

  line = "Check: max_skew";
  report_->reportLineString(line);
  reportBlankLine();

  reportPathHeader();
  reportSkewClkPath("reference pin arrival time", check->refPath());
  reportSkewClkPath("constrained pin arrival time", check->clkPath());

  reportDashLine();
  reportLine("allowable skew", check->maxSkew(this), EarlyLate::early());
  reportLine("actual skew", check->skew(this), EarlyLate::late());
  reportDashLine();
  reportSlack(check->slack(this));
}

// Based on reportTgtClk.
void
ReportPath::reportSkewClkPath(const char *arrival_msg,
			      const PathVertex *clk_path)
{
  const ClockEdge *clk_edge = clk_path->clkEdge(this);
  const Clock *clk = clk_edge->clock();
  const EarlyLate *early_late = clk_path->minMax(this);
  const RiseFall *clk_rf = clk_edge->transition();
  const RiseFall *clk_end_rf = clk_path->transition(this);
  string clk_name = clkName(clk, clk_end_rf != clk_rf);
  float clk_time = clk_edge->time();
  const Arrival &clk_arrival = search_->clkPathArrival(clk_path);
  Arrival clk_delay = clk_arrival - clk_time;
  PathAnalysisPt *path_ap = clk_path->pathAnalysisPt(this);
  const MinMax *min_max = path_ap->pathMinMax();
  Vertex *clk_vertex = clk_path->vertex(this);
  reportClkLine(clk, clk_name.c_str(), clk_end_rf, clk_time, min_max);

  bool is_prop = isPropagated(clk_path);
  if (is_prop && reportClkPath()) {
    const EarlyLate *early_late = TimingRole::skew()->tgtClkEarlyLate();
    if (reportGenClkSrcPath(clk_path, clk, clk_rf, min_max, early_late))
      reportGenClkSrcAndPath(clk_path, clk, clk_rf, early_late, path_ap,
			     0.0, 0.0, false);
    else {
      Arrival insertion, latency;
      PathEnd::checkTgtClkDelay(clk_path, clk_edge, TimingRole::skew(), this,
				insertion, latency);
      reportClkSrcLatency(insertion, clk_time, early_late);
      PathExpanded clk_expanded(clk_path, this);
      reportPath2(clk_path, clk_expanded, false, 0.0);
    }
  }
  else {
    reportLine(clkNetworkDelayIdealProp(is_prop), clk_delay, clk_arrival, early_late);
    reportLine(descriptionField(clk_vertex).c_str(), clk_arrival,
	       early_late, clk_end_rf);
  }
  reportLine(arrival_msg, search_->clkPathArrival(clk_path), early_late);
  reportBlankLine();
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportLimitShortHeader(const ReportField *field)
{
  string line;
  reportDescription("Pin", line);
  line += ' ';
  reportField("Limit", field, line);
  line += ' ';
  reportField(field->title(), field, line);
  line += ' ';
  reportField("Slack", field, line);
  report_->reportLineString(line);

  reportDashLine(field_description_->width() + field->width() * 3 + 3);
}

void
ReportPath::reportLimitShort(const ReportField *field,
			     Pin *pin,
			     float value,
			     float limit,
			     float slack)
{
  string line;
  const char *pin_name = cmd_network_->pathName(pin);
  reportDescription(pin_name, line);
  line += ' ';
  reportField(limit, field, line);
  line += ' ';
  reportField(value, field, line);
  line += ' ';
  reportField(slack, field, line);
  line += (slack >= 0.0)
    ? " (MET)"
    : " (VIOLATED)";
  report_->reportLineString(line);
}

void
ReportPath::reportLimitVerbose(const ReportField *field,
			       Pin *pin,
			       const RiseFall *rf,
			       float value,
			       float limit,
			       float slack,
			       const Corner *corner,
                               const MinMax *min_max)
{
  string line;
  line += "Pin ";
  line += cmd_network_->pathName(pin);
  line += ' ';
  if (rf)
    line += rf->shortName();
  else
    line += ' ';
  // Don't report corner if the default corner is the only corner.
  if (corner && corners_->count() > 1) {
    line += " (corner ";
    line += corner->name();
    line += ")";
  }
  report_->reportLineString(line);

  line = min_max->asString();
  line += ' ';
  line += field->name();
  line += ' ';
  reportField(limit, field, line);
  report_->reportLineString(line);

  line = field->name();
  line += "     ";
  reportField(value, field, line);
  report_->reportLineString(line);

  int name_width = strlen(field->name()) + 5;
  reportDashLine(name_width + field->width());

  line = "Slack";
  for (int i = strlen("Slack"); i < name_width; i++)
    line += ' ';
  reportField(slack, field, line);
  line += (slack >= 0.0)
    ? " (MET)"
    : " (VIOLATED)";
  report_->reportLineString(line);
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportStartpoint(const PathEnd *end,
			     PathExpanded &expanded)
{
  const Path *path = end->path();
  PathRef *start = expanded.startPath();
  TimingArc *prev_arc = expanded.startPrevArc();
  Edge *prev_edge = start->prevEdge(prev_arc, this);
  Pin *pin = start->pin(graph_);
  const ClockEdge *clk_edge = path->clkEdge(this);
  const Clock *clk = path->clock(search_);
  const char *pin_name = cmd_network_->pathName(pin);
  if (pathFromClkPin(path, pin)) {
    const char *clk_name = clk->name();
    auto reason = stdstrPrint("clock source '%s'", clk_name);
    reportStartpoint(pin_name, reason);
  }
  else if (network_->isTopLevelPort(pin)) {
    if (clk
	&& clk != sdc_->defaultArrivalClock()) {
      const char *clk_name = clk->name();
      // Pin direction is "input" even for bidirects.
      auto reason = stdstrPrint("input port clocked by %s", clk_name);
      reportStartpoint(pin_name, reason);
    }
    else
      reportStartpoint(pin_name, "input port");
  }
  else if (network_->isLeaf(pin) && prev_arc) {
    Instance *inst = network_->instance(pin);
    const char *inst_name = cmd_network_->pathName(inst);
    if (clk_edge) {
      const RiseFall *clk_rf = clk_edge->transition();
      PathRef clk_path;
      expanded.clkPath(clk_path);
      bool clk_inverted = !clk_path.isNull()
	&& clk_rf != clk_path.transition(this);
      string clk_name = clkName(clk, clk_inverted);
      const char *reg_desc = edgeRegLatchDesc(prev_edge, prev_arc);
      auto reason = stdstrPrint("%s clocked by %s", reg_desc, clk_name.c_str());
      reportStartpoint(inst_name, reason);
    }
    else {
      const char *reg_desc = edgeRegLatchDesc(prev_edge, prev_arc);
      reportStartpoint(inst_name, reg_desc);
    }
  }
  else if (network_->isLeaf(pin)) {
    if (clk_edge) {
      Clock *clk = clk_edge->clock();
      if (clk != sdc_->defaultArrivalClock()) {
	const char *clk_name = clk->name();
	auto reason = stdstrPrint("internal path startpoint clocked by %s", clk_name);
	reportStartpoint(pin_name, reason);
      }
      else
	reportStartpoint(pin_name, "internal path startpoint");
    }
    else
      reportStartpoint(pin_name, "internal pin");
  }
  else
    reportStartpoint(pin_name, "");
}

bool
ReportPath::pathFromClkPin(PathExpanded &expanded)
{
  PathRef *start = expanded.startPath();
  PathRef *end = expanded.endPath();
  const Pin *start_pin = start->pin(graph_);
  return pathFromClkPin(end, start_pin);
}

bool
ReportPath::pathFromClkPin(const Path *path,
			   const Pin *start_pin)
{
  const Clock *clk = path->clock(search_);
  return clk
    && clk->leafPins().hasKey(const_cast<Pin*>(start_pin));
}

void
ReportPath::reportStartpoint(const char *start,
			     string reason)
{
  reportStartEndPoint(start, reason, "Startpoint");
}

void
ReportPath::reportUnclockedEndpoint(const PathEnd *end,
				    const char *default_reason)
{
  Vertex *vertex = end->vertex(this);
  Pin *pin = vertex->pin();
  if (network_->isTopLevelPort(pin)) {
    // Pin direction is "output" even for bidirects.
    reportEndpoint(cmd_network_->pathName(pin), "output port");
  }
  else if (network_->isLeaf(pin)) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role()->genericRole() == TimingRole::setup()) {
	Vertex *clk_vertex = edge->from(graph_);
	VertexOutEdgeIterator clk_edge_iter(clk_vertex, graph_);
	while (clk_edge_iter.hasNext()) {
	  Edge *clk_edge = clk_edge_iter.next();
	  if (clk_edge->role() == TimingRole::regClkToQ()) {
	    Instance *inst = network_->instance(pin);
	    const char *inst_name = cmd_network_->pathName(inst);
	    const char *reason = regDesc(clk_edge->timingArcSet()->isRisingFallingEdge());
	    reportEndpoint(inst_name, reason);
	    return;
	  }
	  if (clk_edge->role() == TimingRole::latchEnToQ()) {
	    Instance *inst = network_->instance(pin);
	    const char *inst_name = cmd_network_->pathName(inst);
	    const char *reason = latchDesc(clk_edge->timingArcSet()->isRisingFallingEdge());
	    reportEndpoint(inst_name, reason);
	    return;
	  }
	}
      }
    }
    reportEndpoint(cmd_network_->pathName(pin), default_reason);
  }
  else
    reportEndpoint(cmd_network_->pathName(pin), "");
}

void
ReportPath::reportEndpoint(const char *end,
			   string reason)
{
  reportStartEndPoint(end, reason, "Endpoint");
}

void
ReportPath::reportStartEndPoint(const char *pt,
				string reason,
				const char *key)
{
  string line;
  // Account for punctuation in the line.
  int line_len = strlen(key) + 2 + strlen(pt) + 2 + reason.size() + 1;
  if (!no_split_
      && line_len > start_end_pt_width_) {
    line = key;
    line += ": ";
    line += pt;
    report_->reportLineString(line);

    line.clear();
    for (unsigned i = 0; i < strlen(key); i++)
      line += ' ';

    line += "  (";
    line += reason;
    line += ")";
    report_->reportLineString(line);
  }
  else {
    line = key;
    line += ": ";
    line += pt;
    line += " (";
    line += reason;
    line += ")";
    report_->reportLineString(line);
  }
}

void
ReportPath::reportGroup(const PathEnd *end)
{
  string line;
  line = "Path Group: ";
  PathGroup *group = search_->pathGroup(end);
  line += group ? group->name() : "(none)";
  report_->reportLineString(line);

  line = "Path Type: ";
  line += end->minMax(this)->asString();
  report_->reportLineString(line);

  if (corners_->multiCorner()) {
    line = "Corner: ";
    line += end->pathAnalysisPt(this)->corner()->name();
    report_->reportLineString(line);
  }
}

////////////////////////////////////////////////////////////////

string
ReportPath::checkRoleReason(const PathEnd *end)
{
  const char *setup_hold = end->checkRole(this)->asString();
  return stdstrPrint("%s time", setup_hold);
}

string
ReportPath::tgtClkName(const PathEnd *end)
{
  const ClockEdge *tgt_clk_edge = end->targetClkEdge(this);
  const Clock *tgt_clk = tgt_clk_edge->clock();
  const RiseFall *clk_rf = tgt_clk_edge->transition();
  const RiseFall *clk_end_rf = end->targetClkEndTrans(this);
  return clkName(tgt_clk, clk_end_rf != clk_rf);
}

string
ReportPath::clkName(const Clock *clk,
		    bool inverted)
{
  string name = clk->name();
  if (inverted)
    name += '\'';
  return name;
}

const char *
ReportPath::clkRegLatchDesc(const PathEnd *end)
{
  // Goofy libraries can have registers with both rising and falling
  // clk->q timing arcs.  Try and match the timing check transition.
  const RiseFall *check_clk_rf=end->checkArc()->fromEdge()->asRiseFall();
  TimingArcSet *clk_set = nullptr;
  TimingArcSet *clk_rf_set = nullptr;
  Vertex *tgt_clk_vertex = end->targetClkPath()->vertex(this);
  VertexOutEdgeIterator iter(tgt_clk_vertex, graph_);
  while (iter.hasNext()) {
    Edge *edge = iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    TimingRole *role = arc_set->role();
    if (role == TimingRole::regClkToQ()
	|| role == TimingRole::latchEnToQ()) {
      const RiseFall *arc_rf = arc_set->isRisingFallingEdge();
      clk_set = arc_set;
      if (arc_rf == check_clk_rf)
	clk_rf_set = arc_set;
    }
  }
  if (clk_rf_set)
    return checkRegLatchDesc(clk_rf_set->role(),
			     clk_rf_set->isRisingFallingEdge());
  else if (clk_set)
    return checkRegLatchDesc(clk_set->role(),
			     clk_set->isRisingFallingEdge());
  else
    return checkRegLatchDesc(TimingRole::regClkToQ(), check_clk_rf);
}

void
ReportPath::reportSrcPathArrival(const PathEnd *end,
				 PathExpanded &expanded)
{
  reportBlankLine();
  reportSrcPath(end, expanded);
  reportLine("data arrival time", end->dataArrivalTimeOffset(this),
	     end->pathEarlyLate(this));
  reportBlankLine();
}

void
ReportPath::reportSrcPath(const PathEnd *end,
			  PathExpanded &expanded)
{
  reportPathHeader();
  float src_clk_offset = end->sourceClkOffset(this);
  Arrival src_clk_insertion = end->sourceClkInsertionDelay(this);
  Arrival src_clk_latency = end->sourceClkLatency(this);
  const Path *path = end->path();
  reportSrcClkAndPath(path, expanded, src_clk_offset, src_clk_insertion,
		      src_clk_latency, end->isPathDelay());
}

void
ReportPath::reportSrcClkAndPath(const Path *path,
				PathExpanded &expanded,
				float time_offset,
				Arrival clk_insertion,
				Arrival clk_latency,
				bool is_path_delay)
{
  const ClockEdge *clk_edge = path->clkEdge(this);
  const MinMax *min_max = path->minMax(this);
  if (clk_edge) {
    Clock *clk = clk_edge->clock();
    RiseFall *clk_rf = clk_edge->transition();
    float clk_time = clk_edge->time() + time_offset;
    if (clk == sdc_->defaultArrivalClock()) {
      if (!is_path_delay) {
	float clk_end_time = clk_time + time_offset;
	const EarlyLate *early_late = min_max;
	reportLine("clock (input port clock) (rise edge)",
		   clk_end_time, clk_end_time, early_late);
	reportLine(clkNetworkDelayIdealProp(false), 0.0, clk_end_time, early_late);
      }
      reportPath1(path, expanded, false, time_offset);
    }
    else {
      bool path_from_input = false;
      bool input_has_ref_path = false;
      Arrival clk_delay, clk_end_time;
      PathRef clk_path;
      expanded.clkPath(clk_path);
      const RiseFall *clk_end_rf;
      if (!clk_path.isNull()) {
	clk_end_time = search_->clkPathArrival(&clk_path) + time_offset;
	clk_delay = clk_end_time - clk_time;
	clk_end_rf = clk_path.transition(this);
      }
      else {
	// Path from input port or clk used as data.
	clk_end_rf = clk_rf;
	clk_delay = clk_insertion + clk_latency;
	clk_end_time = clk_time + clk_delay;

	PathRef *first_path = expanded.startPath();
	InputDelay *input_delay = pathInputDelay(first_path);
	if (input_delay) {
	  path_from_input = true;
	  const Pin *ref_pin = input_delay->refPin();
	  if (ref_pin && clk->isPropagated()) {
	    PathRef ref_path;
	    pathInputDelayRefPath(first_path, input_delay, ref_path);
	    if (!ref_path.isNull()) {
	      const Arrival &ref_end_time = ref_path.arrival(this);
	      clk_delay = ref_end_time - clk_time;
	      clk_end_time = ref_end_time + time_offset;
	      input_has_ref_path = true;
	    }
	  }
	}
      }
      string clk_name = clkName(clk, clk_rf != clk_end_rf);

      bool clk_used_as_data = pathFromClkPin(expanded);
      bool is_prop = isPropagated(path);
      const EarlyLate *early_late = min_max;
      if (reportGenClkSrcPath(clk_path.isNull() ? nullptr : &clk_path,
			      clk, clk_rf, min_max, early_late)
	  && !(path_from_input && !input_has_ref_path)) {
	reportClkLine(clk, clk_name.c_str(), clk_end_rf, clk_time,
		      min_max);
 	const PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
	reportGenClkSrcAndPath(path, clk, clk_rf, early_late, path_ap,
 			       time_offset, time_offset, clk_used_as_data);
      }
      else if (clk_used_as_data 
 	       && pathFromGenPropClk(path, path->minMax(this))) {
	reportClkLine(clk, clk_name.c_str(), clk_end_rf, clk_time, min_max);
 	ClkInfo *clk_info = path->tag(search_)->clkInfo();
 	if (clk_info->isPropagated())
 	  reportClkSrcLatency(clk_insertion, clk_time, early_late);
	reportPath1(path, expanded, true, time_offset);
      }
      else if (is_prop
	       && reportClkPath()
	       && !(path_from_input && !input_has_ref_path)) {
 	reportClkLine(clk, clk_name.c_str(), clk_end_rf, clk_time, early_late);
	reportClkSrcLatency(clk_insertion, clk_time, early_late);
 	reportPath1(path, expanded, false, time_offset);
      }
      else if (clk_used_as_data) {
	reportClkLine(clk, clk_name.c_str(), clk_end_rf, clk_time, early_late);
 	if (delayGreater(clk_insertion, 0.0, this))
	  reportClkSrcLatency(clk_insertion, clk_time, early_late);
	if (reportClkPath())
	  reportPath1(path, expanded, true, time_offset);
	else {
	  Arrival clk_arrival = clk_end_time;
	  Arrival end_arrival = path->arrival(this) + time_offset;
	  Delay clk_delay = end_arrival - clk_arrival;
	  reportLine("clock network delay", clk_delay,
		     end_arrival, early_late);
	  Vertex *end_vertex = path->vertex(this);
	  reportLine(descriptionField(end_vertex).c_str(),
		     end_arrival, early_late, clk_end_rf);
	}
      }
      else {
	if (is_path_delay) {
	  if (delayGreater(clk_delay, 0.0, this))
	    reportLine(clkNetworkDelayIdealProp(is_prop), clk_delay,
		       clk_end_time, early_late);
	}
	else {
	  reportClkLine(clk, clk_name.c_str(), clk_end_rf, clk_time, min_max);
	  Arrival clk_arrival = clk_end_time;
	  reportLine(clkNetworkDelayIdealProp(is_prop), clk_delay,
		     clk_arrival, early_late);
	}
	reportPath1(path, expanded, false, time_offset);
      }
    }
  }
  else
    reportPath1(path, expanded, false, time_offset);
}

void
ReportPath::reportTgtClk(const PathEnd *end)
{
  reportTgtClk(end, 0.0);
}

void
ReportPath::reportTgtClk(const PathEnd *end,
			 float prev_time)
{
  const Clock *clk = end->targetClk(this);
  const Path *clk_path = end->targetClkPath();
  reportTgtClk(end, prev_time, isPropagated(clk_path, clk));
}

void
ReportPath::reportTgtClk(const PathEnd *end,
			 float prev_time,
			 bool is_prop)
{
  float src_offset = end->sourceClkOffset(this);
  reportTgtClk(end, prev_time, src_offset, is_prop);
}

void
ReportPath::reportTgtClk(const PathEnd *end,
			 float prev_time,
			 float src_offset,
			 bool is_prop)
{
  const ClockEdge *clk_edge = end->targetClkEdge(this);
  Clock *clk = clk_edge->clock();
  const RiseFall *clk_rf = clk_edge->transition();
  const RiseFall *clk_end_rf = end->targetClkEndTrans(this);
  string clk_name = clkName(clk, clk_end_rf != clk_rf);
  float clk_time = prev_time
    + end->targetClkTime(this)
    + end->targetClkMcpAdjustment(this)
    + src_offset;
  Arrival clk_delay = end->targetClkDelay(this);
  Arrival clk_arrival = clk_time + clk_delay;
  PathAnalysisPt *path_ap = end->pathAnalysisPt(this)->tgtClkAnalysisPt();
  const MinMax *min_max = path_ap->pathMinMax();
  const Path *clk_path = end->targetClkPath();
  reportClkLine(clk, clk_name.c_str(), clk_end_rf, prev_time, clk_time, min_max);
  TimingRole *check_role = end->checkRole(this);
  if (is_prop && reportClkPath()) {
    float time_offset = prev_time
      + end->targetClkOffset(this)
      + end->targetClkMcpAdjustment(this);
    const EarlyLate *early_late = check_role->tgtClkEarlyLate();
    if (reportGenClkSrcPath(clk_path, clk, clk_rf, min_max, early_late)) {
      float insertion_offset = 
	clk_path ? tgtClkInsertionOffet(clk_path, early_late, path_ap) : 0.0;
      reportGenClkSrcAndPath(clk_path, clk, clk_rf, early_late, path_ap,
			     time_offset, time_offset + insertion_offset, false);
    }
    else {
      Arrival insertion = end->targetClkInsertionDelay(this);
      if (clk_path) {
	reportClkSrcLatency(insertion, clk_time, early_late);
	PathExpanded clk_expanded(clk_path, this);
	float insertion_offset = tgtClkInsertionOffet(clk_path, early_late,
						      path_ap);
	reportPath5(clk_path, clk_expanded, 0, clk_expanded.size() - 1, is_prop,
		    reportClkPath(), delay_zero, time_offset + insertion_offset);
      }
      else {
	// Output departure.
	Arrival clk_arrival = clk_time + clk_delay;
	reportLine(clkNetworkDelayIdealProp(clk->isPropagated()),
		   clk_delay, clk_arrival, min_max);
      }
    }
    reportClkUncertainty(end, clk_arrival);
    reportCommonClkPessimism(end, clk_arrival);
  }
  else {
    reportLine(clkNetworkDelayIdealProp(is_prop), clk_delay,
               clk_arrival, min_max);
    reportClkUncertainty(end, clk_arrival);
    reportCommonClkPessimism(end, clk_arrival);
    if (clk_path) {
      Vertex *clk_vertex = clk_path->vertex(this);
      reportLine(descriptionField(clk_vertex).c_str(),
		 prev_time
		 + end->targetClkArrival(this)
		 + end->sourceClkOffset(this),
		 min_max, clk_end_rf);
    }
  }
}

float
ReportPath::tgtClkInsertionOffet(const Path *clk_path,
				 const EarlyLate *early_late,
				 PathAnalysisPt *path_ap)
{
  ClkInfo *clk_info = clk_path->clkInfo(this);
  const Pin *src_pin = clk_info->clkSrc();
  const ClockEdge *clk_edge = clk_info->clkEdge();
  const Clock *clk = clk_edge->clock();
  const RiseFall *clk_rf = clk_edge->transition();
  const MinMax *min_max = path_ap->pathMinMax();
  Arrival path_insertion = search_->clockInsertion(clk, src_pin, clk_rf,
						   min_max, min_max,
						   path_ap);
  Arrival tgt_insertion = search_->clockInsertion(clk, src_pin, clk_rf,
						  min_max, early_late,
						  path_ap);
  return delayAsFloat(tgt_insertion - path_insertion);
}

bool
ReportPath::pathFromGenPropClk(const Path *clk_path,
			       const EarlyLate *early_late)
{
  ClkInfo *clk_info = clk_path->tag(search_)->clkInfo();
  const ClockEdge *clk_edge = clk_info->clkEdge();
  if (clk_edge) {
    const Clock *clk = clk_edge->clock();
    float insertion;
    bool exists;
    sdc_->clockInsertion(clk, clk_info->clkSrc(),
			 clk_edge->transition(),
			 clk_path->minMax(this),
			 early_late,
			 insertion, exists);
    return !exists
      && clk->isGeneratedWithPropagatedMaster();
  }
  else
    return false;
}

bool
ReportPath::isGenPropClk(const Clock *clk,
			 const RiseFall *clk_rf,
			 const MinMax *min_max,
			 const EarlyLate *early_late)
{
  float insertion;
  bool exists;
  sdc_->clockInsertion(clk, clk->srcPin(), clk_rf,
		       min_max, early_late,
		       insertion, exists);
  return !exists
    && clk->isGeneratedWithPropagatedMaster();
}

void
ReportPath::reportClkLine(const Clock *clk,
			  const char *clk_name,
			  const RiseFall *clk_rf,
			  Arrival clk_time,
			  const MinMax *min_max)
{
  reportClkLine(clk, clk_name, clk_rf, 0.0, clk_time, min_max);
}

void
ReportPath::reportClkLine(const Clock *clk,
			  const char *clk_name,
			  const RiseFall *clk_rf,
			  Arrival prev_time,
			  Arrival clk_time,
			  const MinMax *min_max)
{
  const char *rise_fall = asRiseFall(clk_rf);
  auto clk_msg = stdstrPrint("clock %s (%s edge)", clk_name, rise_fall);
  if (clk->isPropagated())
    reportLine(clk_msg.c_str(), clk_time - prev_time, clk_time, min_max);
  else {
    // Report ideal clock slew.
    float clk_slew = clk->slew(clk_rf, min_max);
    reportLine(clk_msg.c_str(), clk_slew, clk_time - prev_time, clk_time, min_max);
  }
}

bool
ReportPath::reportGenClkSrcPath(const Path *clk_path,
				const Clock *clk,
				const RiseFall *clk_rf,
				const MinMax *min_max,
				const EarlyLate *early_late)
{
  bool from_gen_prop_clk = clk_path
    ? pathFromGenPropClk(clk_path, early_late)
    : isGenPropClk(clk, clk_rf, min_max, early_late);
  return from_gen_prop_clk
    && format_ == ReportPathFormat::full_clock_expanded;
}

void
ReportPath::reportGenClkSrcAndPath(const Path *path,
				   const Clock *clk,
				   const RiseFall *clk_rf,
				   const EarlyLate *early_late,
				   const PathAnalysisPt *path_ap,
				   float time_offset,
				   float path_time_offset,
				   bool clk_used_as_data)
{
  const Pin *clk_pin = path
    ? path->clkInfo(search_)->clkSrc()
    : clk->defaultPin();
  float gclk_time = clk->edge(clk_rf)->time() + time_offset;
  bool skip_first_path = reportGenClkSrcPath1(clk, clk_pin, clk_rf,
					      early_late, path_ap, gclk_time,
					      time_offset, clk_used_as_data);
  if (path) {
    PathExpanded expanded(path, this);
    reportPath4(path, expanded, skip_first_path, false, clk_used_as_data,
		path_time_offset);
  }
}

bool
ReportPath::reportGenClkSrcPath1(const Clock *clk,
				 const Pin *clk_pin,
				 const RiseFall *clk_rf,
				 const EarlyLate *early_late,
				 const PathAnalysisPt *path_ap,
				 float gclk_time,
				 float time_offset,
				 bool clk_used_as_data)
{
  PathAnalysisPt *insert_ap = path_ap->insertionAnalysisPt(early_late);
  PathVertex src_path;
  const MinMax *min_max = path_ap->pathMinMax();
  search_->genclks()->srcPath(clk, clk_pin, clk_rf, insert_ap, src_path);
  if (!src_path.isNull()) {
    ClkInfo *src_clk_info = src_path.clkInfo(search_);
    const ClockEdge *src_clk_edge = src_clk_info->clkEdge();
    const Clock *src_clk = src_clk_info->clock();
    if (src_clk) {
      bool skip_first_path = false;
      const RiseFall *src_clk_rf = src_clk_edge->transition();
      const Pin *src_clk_pin = src_clk_info->clkSrc();
      if (src_clk->isGeneratedWithPropagatedMaster()
          && src_clk_info->isPropagated()) {
        skip_first_path = reportGenClkSrcPath1(src_clk, src_clk_pin,
                                               src_clk_rf, early_late, path_ap,
                                               gclk_time, time_offset,
                                               clk_used_as_data);
      }
      else {
        const Arrival insertion = search_->clockInsertion(src_clk, src_clk_pin,
                                                          src_clk_rf,
                                                          path_ap->pathMinMax(),
                                                          early_late, path_ap);
        reportClkSrcLatency(insertion, gclk_time, early_late);
      }
      PathExpanded src_expanded(&src_path, this);
      reportPath4(&src_path, src_expanded, skip_first_path, false,
                  clk_used_as_data, gclk_time);
      if (!clk->isPropagated())
        reportLine("clock network delay (ideal)", 0.0,
                   src_path.arrival(this), min_max);
    }
  }
  else {
    if (clk->isPropagated())
      reportClkSrcLatency(0.0, gclk_time, early_late);
    else if (!clk_used_as_data)
      reportLine("clock network delay (ideal)", 0.0, gclk_time, min_max);
  }
  return !src_path.isNull();
}

void
ReportPath::reportClkSrcLatency(Arrival insertion,
				float clk_time,
				const EarlyLate *early_late)
{
  reportLine("clock source latency", insertion, clk_time + insertion, early_late);
}

void
ReportPath::reportPathLine(const Path *path,
			   Arrival incr,
			   Arrival time,
			   const char *line_case)
{
  Vertex *vertex = path->vertex(this);
  Pin *pin = vertex->pin();
  auto what = descriptionField(vertex);
  const RiseFall *rf = path->transition(this);
  bool is_driver = network_->isDriver(pin);
  PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
  const EarlyLate *early_late = path_ap->pathMinMax();
  DcalcAnalysisPt *dcalc_ap = path_ap->dcalcAnalysisPt();
  DcalcAPIndex ap_index = dcalc_ap->index();
  Slew slew = graph_->slew(vertex, rf, ap_index);
  float cap = field_blank_;
  Instance *inst = network_->instance(pin);
  string src_attr = "";
  if (inst)
    src_attr = network_->getAttribute(inst, "src");
  // Don't show capacitance field for input pins.
  if (is_driver && field_capacitance_->enabled())
    cap = graph_delay_calc_->loadCap(pin, rf, dcalc_ap);
  reportLine(what.c_str(), cap, slew, field_blank_,
	     incr, time, false, early_late, rf, src_attr,
	     line_case);
}

void
ReportPath::reportRequired(const PathEnd *end,
			   string margin_msg)
{
  Required req_time = end->requiredTimeOffset(this);
  const EarlyLate *early_late = end->clkEarlyLate(this);
  ArcDelay margin = end->margin(this);
  if (end->minMax(this) == MinMax::max())
    margin = -margin;
  reportLine(margin_msg.c_str(), margin, req_time, early_late);
  reportLine("data required time", req_time, early_late);
  reportDashLine();
}

void
ReportPath::reportSlack(const PathEnd *end)
{
  const EarlyLate *early_late = end->pathEarlyLate(this);
  reportLine("data required time", end->requiredTimeOffset(this),
	     early_late->opposite());
  reportLineNegative("data arrival time", end->dataArrivalTimeOffset(this), early_late);
  reportDashLine();
  reportSlack(end->slack(this));
}

void
ReportPath::reportSlack(Slack slack)
{
  const EarlyLate *early_late = EarlyLate::early();
  const char *msg = (delayAsFloat(slack, early_late, this) >= 0.0)
    ? "slack (MET)"
    : "slack (VIOLATED)";
  reportLine(msg, slack, early_late);
}

void
ReportPath::reportSpaceSlack(PathEnd *end,
			     string &result)
{
  Slack slack = end->slack(this);
  reportSpaceSlack(slack, result);
}

void
ReportPath::reportSpaceSlack(Slack slack,
			     string &result)
{
  const EarlyLate *early_late = EarlyLate::early();
  reportSpaceFieldDelay(slack, early_late, result);
  result += (delayAsFloat(slack, early_late, this) >= 0.0)
    ? " (MET)"
    : " (VIOLATED)";
}

void
ReportPath::reportCommonClkPessimism(const PathEnd *end,
				     Arrival &clk_arrival)
{
  if (sdc_->crprEnabled()) {
    Crpr pessimism = end->checkCrpr(this);
    clk_arrival += pessimism;
    reportLine("clock reconvergence pessimism", pessimism, clk_arrival,
	       end->clkEarlyLate(this));
  }
}

void
ReportPath::reportClkUncertainty(const PathEnd *end,
				 Arrival &clk_arrival)
{
  const EarlyLate *early_late = end->clkEarlyLate(this);
  float uncertainty = end->targetNonInterClkUncertainty(this);
  clk_arrival += uncertainty;
  if (uncertainty != 0.0)
    reportLine("clock uncertainty", uncertainty, clk_arrival, early_late);
  float inter_uncertainty = end->interClkUncertainty(this);
  clk_arrival += inter_uncertainty;
  if (inter_uncertainty != 0.0)
    reportLine("inter-clock uncertainty", inter_uncertainty,
               clk_arrival, early_late);
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportPath(const PathEnd *end,
		       PathExpanded &expanded)
{
  reportPathHeader();
  // Source clk offset for path delays removes clock phase time.
  float src_clk_offset = end->sourceClkOffset(this);
  reportPath1(end->path(), expanded, pathFromClkPin(expanded), src_clk_offset);
}

void
ReportPath::reportPath(const Path *path)
{
  switch (format_) {
  case ReportPathFormat::full:
  case ReportPathFormat::full_clock:
  case ReportPathFormat::full_clock_expanded:
    reportPathFull(path);
    break;
  case ReportPathFormat::json:
    reportJson(path);
    break;
  case ReportPathFormat::shorter:
  case ReportPathFormat::endpoint:
  case ReportPathFormat::summary:
  case ReportPathFormat::slack_only:
    report_->reportLine("Format not supported.");
    break;
  }
}

void
ReportPath::reportPathFull(const Path *path)
{
  reportPathHeader();
  PathExpanded expanded(path, this);
  reportSrcClkAndPath(path, expanded, 0.0, delay_zero, delay_zero, false);
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportPath1(const Path *path,
			PathExpanded &expanded,
			bool clk_used_as_data,
			float time_offset)
{
  PathRef *d_path, *q_path;
  Edge *d_q_edge;
  expanded.latchPaths(d_path, q_path, d_q_edge);
  if (d_path) {
    Arrival latch_time_given, latch_enable_time;
    PathVertex latch_enable_path;
    latches_->latchTimeGivenToStartpoint(d_path, q_path, d_q_edge,
					 latch_time_given,
					 latch_enable_path);
    if (!latch_enable_path.isNull()) {
      const EarlyLate *early_late = latch_enable_path.minMax(this);
      latch_enable_time = search_->clkPathArrival(&latch_enable_path);
      if (reportClkPath()) {
	PathExpanded enable_expanded(&latch_enable_path, this);
	// Report the path to the latch enable.
	reportPath2(&latch_enable_path, enable_expanded, false,
		    time_offset);
      }
      Arrival time = latch_enable_time + latch_time_given;
      Arrival incr = latch_time_given;
      if (delayGreaterEqual(incr, 0.0, this))
	reportLine("time given to startpoint", incr, time, early_late);
      else
	reportLine("time borrowed from startpoint", incr, time, early_late);
      // Override latch D arrival with enable + given.
      reportPathLine(expanded.path(0), delay_zero, time, "latch_D");
      bool propagated_clk = path->clkInfo(search_)->isPropagated();
      bool report_clk_path = path->isClock(search_) || reportClkPath();
      reportPath5(path, expanded, 1, expanded.size() - 1,
		  propagated_clk, report_clk_path,
		  latch_enable_time + latch_time_given, time_offset);
    }
  }
  else
    reportPath2(path, expanded, clk_used_as_data, time_offset);
}

void
ReportPath::reportPath2(const Path *path,
			PathExpanded &expanded,
			bool clk_used_as_data,
			float time_offset)
{
  // Report the clock path if the end is a clock or we wouldn't have
  // anything to report.
  bool report_clk_path = clk_used_as_data
    || (reportClkPath()
	&& path->clkInfo(search_)->isPropagated());
  reportPath3(path, expanded, clk_used_as_data, report_clk_path,
	      delay_zero, time_offset);
}

void
ReportPath::reportPath3(const Path *path,
			PathExpanded &expanded,
			bool clk_used_as_data,
			bool report_clk_path,
			Arrival prev_time,
			float time_offset)
{
  bool propagated_clk = clk_used_as_data
    || path->clkInfo(search_)->isPropagated();
  size_t path_last_index = expanded.size() - 1;
  reportPath5(path, expanded, 0, path_last_index, propagated_clk,
	      report_clk_path, prev_time, time_offset);
}

void
ReportPath::reportPath4(const Path *path,
			PathExpanded &expanded,
			bool skip_first_path,
			bool skip_last_path,
			bool clk_used_as_data,
			float time_offset)
{
  size_t path_first_index = 0;
  Arrival prev_time(0.0);
  if (skip_first_path) {
    path_first_index = 1;
    const PathRef *start = expanded.path(0);
    prev_time = start->arrival(this) + time_offset;
  }
  size_t path_last_index = expanded.size() - 1;
  if (skip_last_path
      && path_last_index > 1)
    path_last_index--;
  bool propagated_clk = clk_used_as_data
    || path->clkInfo(search_)->isPropagated();
  // Report the clock path if the end is a clock or we wouldn't have
  // anything to report.
  bool report_clk_path = path->isClock(search_) 
    || (reportClkPath() && propagated_clk);
  reportPath5(path, expanded, path_first_index, path_last_index,
	      propagated_clk, report_clk_path, prev_time, time_offset);
}

void
ReportPath::reportPath5(const Path *path,
			PathExpanded &expanded,
			size_t path_first_index,
			size_t path_last_index,
			bool propagated_clk,
			bool report_clk_path,
			Arrival prev_time,
			float time_offset)
{
  const MinMax *min_max = path->minMax(this);
  DcalcAnalysisPt *dcalc_ap = path->pathAnalysisPt(this)->dcalcAnalysisPt();
  DcalcAPIndex ap_index = dcalc_ap->index();
  PathRef clk_path;
  expanded.clkPath(clk_path);
  Vertex *clk_start = clk_path.vertex(this);
  for (size_t i = path_first_index; i <= path_last_index; i++) {
    const PathRef *path1 = expanded.path(i);
    TimingArc *prev_arc = expanded.prevArc(i);
    Vertex *vertex = path1->vertex(this);
    Pin *pin = vertex->pin();
    Arrival time = path1->arrival(this) + time_offset;
    Delay incr = 0.0;
    const char *line_case = nullptr;
    bool is_clk_start = path1->vertex(this) == clk_start;
    bool is_clk = path1->isClock(search_);
    Instance *inst = network_->instance(pin);
    string src_attr = "";
    if (inst)
      src_attr = network_->getAttribute(inst, "src");
    // Always show the search start point (register clk pin).
    // Skip reporting the clk tree unless it is requested.
    if (is_clk_start
	|| report_clk_path
	|| !is_clk) {
      const RiseFall *rf = path1->transition(this);
      Slew slew = graph_->slew(vertex, rf, ap_index);
      if (prev_arc == nullptr) {
	// First path.
	reportInputExternalDelay(path1, time_offset);
	size_t next_index = i + 1;
	const PathRef *next_path = expanded.path(next_index);
	if (network_->isTopLevelPort(pin)
	    && next_path
	    && !nextArcAnnotated(next_path, next_index, expanded, ap_index)
	    && hasExtInputDriver(pin, rf, min_max)) {
	  // Pin is an input port with drive_cell/drive_resistance.
	  // The delay calculator annotates wire delays on the edges
	  // from the input to the loads.  Report the wire delay on the
	  // input pin instead.
	  Arrival next_time = next_path->arrival(this) + time_offset;
	  incr = delayIncr(next_time, time, min_max);
	  time = next_time;
	  line_case = "input_drive";
	}
	else if (is_clk) {
	  if (!propagated_clk)
	    // Clock latency at path endpoint in case latency was set
	    // on a clock pin other than the clock source.
	    time = search_->clkPathArrival(path1) + time_offset;
	  incr = 0.0;
	  line_case = "clk_first";
	}
	else {
	  incr = 0.0;
	  line_case = "first";
	}
      }
      else if (is_clk_start
	       && is_clk
	       && !report_clk_path) {
	// Clock start point and clock path are not reported.
	incr = 0.0;
	if (!propagated_clk) {
	  // Ideal clock.
	   const ClockEdge *src_clk_edge = path->clkEdge(this);
	  time = search_->clkPathArrival(path1) + time_offset;
	  if (src_clk_edge) {
	    Clock *src_clk = src_clk_edge->clock();
	    RiseFall *src_clk_rf = src_clk_edge->transition();
	    slew = src_clk->slew(src_clk_rf, min_max);
	  }
	}
	line_case = "clk_start";
      }
      else if (is_clk
	       && report_clk_path
	       && !propagated_clk) {
	// Zero the clock network delays for ideal clocks.
	incr = 0.0;
	time = prev_time;
	const ClockEdge *src_clk_edge = path->clkEdge(this);
	const Clock *src_clk = src_clk_edge->clock();
	RiseFall *src_clk_rf = src_clk_edge->transition();
	slew = src_clk->slew(src_clk_rf, min_max);
	line_case = "clk_ideal";
      }
      else if (is_clk && !is_clk_start) {
	incr = delayIncr(time, prev_time, min_max);
	line_case = "clk_prop";
      }
      else {
	incr = delayIncr(time, prev_time, min_max);
	line_case = "normal";
      }
      if (report_input_pin_
	  || (i == path_last_index)
	  || is_clk_start
	  || (prev_arc == nullptr)
	  // Filter wire edges from report unless reporting
	  // input pins.
	  || (prev_arc
	      && !prev_arc->role()->isWire())) {
	bool is_driver = network_->isDriver(pin);
	float cap = field_blank_;
        float fanout = field_blank_;
	// Don't show capacitance field for input pins.
	if (is_driver && field_capacitance_->enabled())
          cap = graph_delay_calc_->loadCap(pin, rf, dcalc_ap);
	// Don't show fanout field for input pins.
	if (is_driver && field_fanout_->enabled())
	  fanout = drvrFanout(vertex, dcalc_ap->corner(), min_max);
	auto what = descriptionField(vertex);
	if (report_net_ && is_driver) {
	  reportLine(what.c_str(), cap, slew, fanout,
		     incr, time, false, min_max, rf,
		     src_attr, line_case);
	  string what2;
	  if (network_->isTopLevelPort(pin)) {
	    const char *pin_name = cmd_network_->pathName(pin);
	    what2 = stdstrPrint("%s (net)", pin_name);
	  }
	  else {
	    Net *net = network_->net(pin);
	    if (net) {
	      Net *highest_net = network_->highestNetAbove(net);
	      const char *net_name = cmd_network_->pathName(highest_net);
	      what2 = stdstrPrint("%s (net)", net_name);
	    }
	    else
	      what2 = "(unconnected)";
	  }
	  reportLine(what2.c_str(), field_blank_, field_blank_, field_blank_,
		     field_blank_, field_blank_, false, min_max,
                     nullptr, src_attr, line_case);
	}
	else
	  reportLine(what.c_str(), cap, slew, fanout,
		     incr, time, false, min_max, rf, src_attr,
		     line_case);
	prev_time = time;
      }
    }
    else
      prev_time = time;
  }
}

Delay
ReportPath::delayIncr(Delay time,
		      Delay prev,
		      const MinMax *min_max)
{
  if (report_sigmas_)
    return delayRemove(time, prev);
  else
    return delayAsFloat(time, min_max, this) - delayAsFloat(prev, min_max, this);
}

bool
ReportPath::nextArcAnnotated(const PathRef *next_path,
			     size_t next_index,
			     PathExpanded &expanded,
			     DcalcAPIndex ap_index)
{
  TimingArc *arc = expanded.prevArc(next_index);
  Edge *edge = next_path->prevEdge(arc, this);
  return graph_->arcDelayAnnotated(edge, arc, ap_index);
}

string
ReportPath::descriptionField(Vertex *vertex)
{
  Pin *pin = vertex->pin();
  const char *pin_name = cmd_network_->pathName(pin);
  const char *name2;
  if (network_->isTopLevelPort(pin)) {
    PortDirection *dir = network_->direction(pin);
    // Translate port direction.  Note that this is intentionally
    // inconsistent with the direction reported for top level ports as
    // startpoints.
    if (dir->isInput())
      name2 = "in";
    else if (dir->isOutput() || dir->isTristate())
      name2 = "out";
    else if (dir->isBidirect())
      name2 = "inout";
    else
      name2 = "?";
  }
  else {
    Instance *inst = network_->instance(pin);
    name2 = network_->cellName(inst);
  }
  return stdstrPrint("%s (%s)", pin_name, name2);
}

float
ReportPath::drvrFanout(Vertex *drvr,
                       const Corner *corner,
		       const MinMax *min_max)
{
  float fanout = 0.0;
  VertexOutEdgeIterator iter(drvr, graph_);
  while (iter.hasNext()) {
    Edge *edge = iter.next();
    if (edge->isWire()) {
      Pin *pin = edge->to(graph_)->pin();
      if (network_->isTopLevelPort(pin)) {
        // Output port counts as a fanout.
        Port *port = network_->port(pin);
        fanout += sdc_->portExtFanout(port, corner, min_max) + 1;
      }
      else
        fanout++;
    }
  }
  return fanout;
}

bool
ReportPath::hasExtInputDriver(const Pin *pin,
			      const RiseFall *rf,
			      const MinMax *min_max)
{
  Port *port = network_->port(pin);
  InputDrive *drive = sdc_->findInputDrive(port);
  return (drive
	  && (drive->hasDriveResistance(rf, min_max)
	      || drive->hasDriveCell(rf, min_max)));
}

void
ReportPath::reportInputExternalDelay(const Path *first_path,
				     float time_offset)
{
  const Pin *first_pin = first_path->pin(graph_);
  if (!pathFromClkPin(first_path, first_pin)) {
    const RiseFall *rf = first_path->transition(this);
    Arrival time = first_path->arrival(this) + time_offset;
    const EarlyLate *early_late = first_path->minMax(this);
    InputDelay *input_delay = pathInputDelay(first_path);
    if (input_delay) {
      const Pin *ref_pin = input_delay->refPin();
      if (ref_pin) {
	PathRef ref_path;
	pathInputDelayRefPath(first_path, input_delay, ref_path);
	if (!ref_path.isNull() && reportClkPath()) {
	  PathExpanded ref_expanded(&ref_path, this);
	  reportPath3(&ref_path, ref_expanded, false, true,
		      delay_zero, 0.0);
	}
      }
      float input_arrival =
	input_delay->delays()->value(rf, first_path->minMax(this));
      reportLine("input external delay", input_arrival, time,
		 early_late, rf);
    }
    else if (network_->isTopLevelPort(first_pin))
      reportLine("input external delay", 0.0, time, early_late, rf);
  }
}

// Return the input delay at the start of a path.
InputDelay *
ReportPath::pathInputDelay(const Path *first_path) const
{
  return first_path->tag(this)->inputDelay();
}

void
ReportPath::pathInputDelayRefPath(const Path *path,
				  InputDelay *input_delay,
				  // Return value.
				  PathRef &ref_path)
{
  const Pin *ref_pin = input_delay->refPin();
  RiseFall *ref_rf = input_delay->refTransition();
  Vertex *ref_vertex = graph_->pinDrvrVertex(ref_pin);
  if (ref_vertex) {
    const PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
    const ClockEdge *clk_edge = path->clkEdge(this);
    VertexPathIterator path_iter(ref_vertex, ref_rf, path_ap, this);
    while (path_iter.hasNext()) {
      PathVertex *path = path_iter.next();
      if (path->isClock(this)
          && path->clkEdge(this) == clk_edge) {
        ref_path.init(path);
        break;
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportPathHeader()
{
  ReportFieldSeq::Iterator field_iter(fields_);
  string line;
  bool first_field = true;
  while (field_iter.hasNext()) {
    ReportField *field = field_iter.next();
    if (field->enabled()) {
      if (!first_field)
	line += ' ';
      reportField(field->title(), field, line);
      first_field = false;
    }
  }
  trimRight(line);
  report_->reportLineString(line);
  reportDashLine();
}

// Report total.
void
ReportPath::reportLine(const char *what,
		       Delay total,
		       const EarlyLate *early_late)
{
  reportLine(what, field_blank_, field_blank_, field_blank_,
	     field_blank_, total, false, early_late, nullptr,
	     "", nullptr);
}

// Report negative total.
void
ReportPath::reportLineNegative(const char *what,
			       Delay total,
			       const EarlyLate *early_late)
{
  reportLine(what, field_blank_, field_blank_, field_blank_,
	     field_blank_, total, true, early_late, nullptr,
	     "", nullptr);
}

// Report total, and transition suffix.
void
ReportPath::reportLine(const char *what,
		       Delay total,
		       const EarlyLate *early_late,
		       const RiseFall *rf)
{
  reportLine(what, field_blank_, field_blank_, field_blank_,
	     field_blank_, total, false, early_late, rf, "",
	     nullptr);
}

// Report increment, and total.
void
ReportPath::reportLine(const char *what,
		       Delay incr,
		       Delay total,
		       const EarlyLate *early_late)
{
  reportLine(what, field_blank_, field_blank_, field_blank_,
	     incr, total, false, early_late, nullptr, "",
	     nullptr);
}

// Report increment, total, and transition suffix.
void
ReportPath::reportLine(const char *what,
		       Delay incr,
		       Delay total,
		       const EarlyLate *early_late,
		       const RiseFall *rf)
{
  reportLine(what, field_blank_, field_blank_, field_blank_,
	     incr, total, false, early_late, rf, "",
	     nullptr);
}

// Report slew, increment, and total.
void
ReportPath::reportLine(const char *what,
		       Slew slew,
		       Delay incr,
		       Delay total,
		       const EarlyLate *early_late)
{
  reportLine(what, field_blank_, slew, field_blank_,
	     incr, total, false, early_late, nullptr,
	     "", nullptr);
}

void
ReportPath::reportLine(const char *what,
		       float cap,
		       Slew slew,
		       float fanout,
		       Delay incr,
		       Delay total,
		       bool total_with_minus,
		       const EarlyLate *early_late,
		       const RiseFall *rf,
		       string src_attr,
		       const char *line_case)
{
  ReportFieldSeq::Iterator field_iter(fields_);
  string line;
  size_t field_index = 0;
  bool first_field = true;
  while (field_iter.hasNext()) {
    ReportField *field = field_iter.next();
    bool last_field = field_index == (fields_.size() - 1);
    
    if (field->enabled()) {
      if (!first_field)
	line += ' ';

      if (field == field_description_)
	reportDescription(what, first_field, last_field, line);
      else if (field == field_fanout_) {
	if (fanout == field_blank_)
	  reportFieldBlank(field, line);
	else
	  line += stdstrPrint("%*d",
                              field_fanout_->width(),
                              static_cast<int>(fanout));
      }
      else if (field == field_capacitance_)
	reportField(cap, field, line);
      else if (field == field_slew_)
	reportFieldDelay(slew, early_late, field, line);
      else if (field == field_incr_)
	reportFieldDelay(incr, early_late, field, line);
      else if (field == field_total_) {
	if (total_with_minus)
	  reportFieldDelayMinus(total, early_late, field, line);
	else
	  reportFieldDelay(total, early_late, field, line);
      }
      else if (field == field_edge_) {
	if (rf)
	  reportField(rf->shortName(), field, line);
	else
	  reportFieldBlank(field, line);
      }
      else if (field == field_src_attr_) {
	if (src_attr != "")
	  reportField(src_attr.c_str(), field, line);
	else
	  reportFieldBlank(field, line);
      }
      else if (field == field_case_ && line_case)
	line += line_case;

      first_field = false;
    }
    field_index++;
  }
  // Trim trailing spaces and report the line.
  string line_stdstr = line;
  trimRight(line_stdstr);
  report_->reportLineString(line_stdstr.c_str());
}

////////////////////////////////////////////////////////////////

// Only the total field.
void
ReportPath::reportLineTotal(const char *what,
			    Delay incr,
			    const EarlyLate *early_late)
{
  reportLineTotal1(what, incr, false, early_late);
}

// Only the total field and always with leading minus sign.
void
ReportPath::reportLineTotalMinus(const char *what,
				 Delay decr,
				 const EarlyLate *early_late)
{
  reportLineTotal1(what, decr, true, early_late);
}

void
ReportPath::reportLineTotal1(const char *what,
			     Delay incr,
			     bool incr_with_minus,
			     const EarlyLate *early_late)
{
  string line;
  reportDescription(what, line);
  line += ' ';
  if (incr_with_minus)
    reportFieldDelayMinus(incr, early_late, field_total_, line);
  else
    reportFieldDelay(incr, early_late, field_total_, line);
  report_->reportLineString(line);
}

void
ReportPath::reportDashLineTotal()
{
  reportDashLine(field_description_->width() + field_total_->width() + 1);
}

////////////////////////////////////////////////////////////////

void
ReportPath::reportDescription(const char *what,
			      string &line)
{
  reportDescription(what, false, false, line);
}

void
ReportPath::reportDescription(const char *what,
			      bool first_field,
			      bool last_field,
			      string &line)
{
  line += what;
  int length = strlen(what);
  if (!no_split_
      && first_field
      && length > field_description_->width()) {
    reportBlankLine();
    for (int i = 0; i < field_description_->width(); i++)
      line += ' ';
  }
  else if (!last_field) {
    for (int i = length; i < field_description_->width(); i++)
      line += ' ';
  }
}

void
ReportPath::reportFieldTime(float value,
			    ReportField *field,
			    string &line)
{
  if (delayAsFloat(value) == field_blank_)
    reportFieldBlank(field, line);
  else {
    const char *str = units_->timeUnit()->asString(value, digits_);
    if (stringEq(str, minus_zero_))
      // Filter "-0.00" fields.
      str = plus_zero_;
    reportField(str, field, line);
  }
}

void
ReportPath::reportSpaceFieldTime(float value,
				 string &line)
{
  line += ' ';
  reportFieldTime(value, field_total_, line);
}

void
ReportPath::reportSpaceFieldDelay(Delay value,
				  const EarlyLate *early_late,
				  string &line)
{
  line += ' ';
  reportTotalDelay(value, early_late, line);
}

void
ReportPath::reportTotalDelay(Delay value,
			     const EarlyLate *early_late,
			     string &line)
{
  const char *str = delayAsString(value, early_late, this, digits_);
  if (stringEq(str, minus_zero_))
    // Filter "-0.00" fields.
    str = plus_zero_;
  reportField(str, field_total_, line);
}

// Total time always with leading minus sign.
void
ReportPath::reportFieldDelayMinus(Delay value,
				  const EarlyLate *early_late,
				  ReportField *field,
				  string &line)
{
  if (delayAsFloat(value) == field_blank_)
    reportFieldBlank(field, line);
  else {
    const char *str = report_sigmas_
      ? delayAsString(-value, this, digits_)
      // Opposite min/max for negative value.
      : delayAsString(-value, early_late->opposite(), this, digits_);
    if (stringEq(str, plus_zero_))
      // Force leading minus sign.
      str = minus_zero_;
    reportField(str, field, line);
  }
}

void
ReportPath::reportFieldDelay(Delay value,
			     const EarlyLate *early_late,
			     ReportField *field,
			     string &line)
{
  if (delayAsFloat(value) == field_blank_)
    reportFieldBlank(field, line);
  else {
    const char *str = report_sigmas_
      ? delayAsString(value, this, digits_)
      : delayAsString(value, early_late, this, digits_);
    if (stringEq(str, minus_zero_))
      // Filter "-0.00" fields.
      str = plus_zero_;
    reportField(str, field, line);
  }
}

void
ReportPath::reportField(float value,
			const ReportField *field,
			string &line)
{
  if (value == field_blank_)
    reportFieldBlank(field, line);
  else {
    Unit *unit = field->unit();
    if (unit) {
      const char *value_str = unit->asString(value, digits_);
      reportField(value_str, field, line);
    }
    else {
      // fanout
      string value_str;
      stringPrint(value_str, "%.0f", value);
      reportField(value_str.c_str(), field, line);
    }
  }
}

void
ReportPath::reportField(const char *value,
			const ReportField *field,
			string &line)
{
  if (field->leftJustify())
    line += value;
  for (int i = strlen(value); i < field->width(); i++)
    line += ' ';
  if (!field->leftJustify())
    line += value;
}

void
ReportPath::reportFieldBlank(const ReportField *field,
			     string &line)
{
  line += field->blank();
}

void
ReportPath::reportDashLine()
{
  string line;
  ReportFieldSeq::Iterator field_iter(fields_);
  while (field_iter.hasNext()) {
    ReportField *field = field_iter.next();
    if (field->enabled()) {
      for (int i = 0; i < field->width(); i++)
	line += '-';
    }
  }
  line += "------";
  report_->reportLineString(line);
}

void
ReportPath::reportDashLine(int line_width)
{
  string line;
  for (int i = 0; i < line_width; i++)
    line += '-';
  report_->reportLineString(line);
}

void
ReportPath::reportBlankLine()
{
  report_->reportBlankLine();
}

bool
ReportPath::reportClkPath() const
{
  return format_ == ReportPathFormat::full_clock
    || format_ == ReportPathFormat::full_clock_expanded;
}

////////////////////////////////////////////////////////////////

const char *
ReportPath::asRisingFalling(const RiseFall *rf)
{
  if (rf == RiseFall::rise())
    return "rising";
  else
    return "falling";
}

const char *
ReportPath::asRiseFall(const RiseFall *rf)
{
  if (rf == RiseFall::rise())
    return "rise";
  else
    return "fall";
}

// Find the startpoint type from the first path edge.
const char *
ReportPath::edgeRegLatchDesc(Edge *first_edge,
			     TimingArc *first_arc)
{
  TimingRole *role = first_arc->role();
  if (role == TimingRole::latchDtoQ()) {
    Instance *inst = network_->instance(first_edge->to(graph_)->pin());
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      const LibertyPort *enable_port;
      const FuncExpr *enable_func;
      const RiseFall *enable_rf;
      cell->latchEnable(first_edge->timingArcSet(),
			enable_port, enable_func, enable_rf);
      return latchDesc(enable_rf);
    }
  }
  else if (role == TimingRole::regClkToQ())
    return regDesc(first_arc->fromEdge()->asRiseFall());
  else if (role == TimingRole::latchEnToQ())
    return latchDesc(first_arc->fromEdge()->asRiseFall());
  // Who knows...
  return regDesc(first_arc->fromEdge()->asRiseFall());
}

const char *
ReportPath::checkRegLatchDesc(const TimingRole *role,
			      const RiseFall *clk_rf) const
{
  if (role == TimingRole::regClkToQ())
    return regDesc(clk_rf);
  else if (role == TimingRole::latchEnToQ()
	   || role == TimingRole::latchDtoQ())
    return latchDesc(clk_rf);
  else
    // Default when we don't know better.
    return "edge-triggered flip-flop";
}

const char *
ReportPath::regDesc(const RiseFall *clk_rf) const
{
  if (clk_rf == RiseFall::rise())
    return "rising edge-triggered flip-flop";
  else if (clk_rf == RiseFall::fall())
    return "falling edge-triggered flip-flop";
  else
    return "edge-triggered flip-flop";
}

const char *
ReportPath::latchDesc(const RiseFall *clk_rf) const
{
  return (clk_rf == RiseFall::rise()) 
    ? "positive level-sensitive latch"
    : "negative level-sensitive latch";
}

} // namespace
