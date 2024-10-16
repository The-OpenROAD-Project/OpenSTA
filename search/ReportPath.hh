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

#pragma once

#include <string>

#include "StringSeq.hh"
#include "SearchClass.hh"
#include "PathEnd.hh"

namespace sta {

class Corner;
class DcalcAnalysisPt;
class PathExpanded;
class ReportField;

using std::string;

typedef Vector<ReportField*> ReportFieldSeq;

class ReportPath : public StaState
{
public:
  explicit ReportPath(StaState *sta);
  virtual ~ReportPath();
  void setPathFormat(ReportPathFormat format);
  void setReportFieldOrder(StringSeq *field_names);
  void setReportFields(bool report_input_pin,
		       bool report_net,
		       bool report_cap,
		       bool report_slew,
		       bool report_fanout,
		       bool report_src_attr);
  int digits() const { return digits_; }
  void setDigits(int digits);
  void setNoSplit(bool no_split);
  bool reportSigmas() const { return report_sigmas_; }
  void setReportSigmas(bool report);
  ReportField *findField(const char *name);

  // Header above reportPathEnd results.
  void reportPathEndHeader();
  // Footer below reportPathEnd results.
  void reportPathEndFooter();
  void reportPathEnd(PathEnd *end);
  // Format report_path_endpoint only:
  //   Previous path end is used to detect path group changes
  //   so headers are reported by group.
  void reportPathEnd(PathEnd *end,
		     PathEnd *prev_end,
                     bool last);
  void reportPathEnds(PathEndSeq *ends);
  void reportPath(const Path *path);

  void reportShort(const PathEndUnconstrained *end);
  void reportShort(const PathEndCheck *end);
  void reportShort(const PathEndLatchCheck *end);
  void reportShort(const PathEndPathDelay *end);
  void reportShort(const PathEndOutputDelay *end);
  void reportShort(const PathEndGatedClock *end);
  void reportShort(const PathEndDataCheck *end);

  void reportFull(const PathEndUnconstrained *end);
  void reportFull(const PathEndCheck *end);
  void reportFull(const PathEndLatchCheck *end);
  void reportFull(const PathEndPathDelay *end);
  void reportFull(const PathEndOutputDelay *end);
  void reportFull(const PathEndGatedClock *end);
  void reportFull(const PathEndDataCheck *end);

  void reportJsonHeader();
  void reportJsonFooter();
  void reportJson(const PathEnd *end,
                  bool last);
  void reportJson(const Path *path);
  void reportJson(const Path *path,
                  const char *path_name,
                  int indent,
                  bool trailing_comma,
                  string &result);
  void reportJson(const PathExpanded &expanded,
                  const char *path_name,
                  int indent,
                  bool trailing_comma,
                  string &result);

  void reportEndHeader();
  void reportEndLine(PathEnd *end);

  void reportSummaryHeader();
  void reportSummaryLine(PathEnd *end);

  void reportSlackOnlyHeader();
  void reportSlackOnly(PathEnd *end);

  void reportMpwCheck(MinPulseWidthCheck *check,
		      bool verbose);
  void reportMpwChecks(MinPulseWidthCheckSeq *checks,
		       bool verbose);
  void reportMpwHeaderShort();
  void reportShort(MinPulseWidthCheck *check);
  void reportVerbose(MinPulseWidthCheck *check);

  void reportCheck(MinPeriodCheck *check,
		   bool verbose);
  void reportChecks(MinPeriodCheckSeq *checks,
		    bool verbose);
  void reportPeriodHeaderShort();
  void reportShort(MinPeriodCheck *check);
  void reportVerbose(MinPeriodCheck *check);

  void reportCheck(MaxSkewCheck *check,
		   bool verbose);
  void reportChecks(MaxSkewCheckSeq *checks,
		    bool verbose);
  void reportMaxSkewHeaderShort();
  void reportShort(MaxSkewCheck *check);
  void reportVerbose(MaxSkewCheck *check);

  void reportLimitShortHeader(const ReportField *field);
  void reportLimitShort(const ReportField *field,
			Pin *pin,
			float value,
			float limit,
			float slack);
  void reportLimitVerbose(const ReportField *field,
			  Pin *pin,
			  const RiseFall *rf,
			  float value,
			  float limit,
			  float slack,
                          const Corner *corner,
			  const MinMax *min_max);
  ReportField *fieldSlew() const { return field_slew_; }
  ReportField *fieldFanout() const { return field_fanout_; }
  ReportField *fieldCapacitance() const { return field_capacitance_; }
  ReportField *fieldSrcAttr() const { return field_src_attr_; }

protected:
  void makeFields();
  ReportField *makeField(const char *name,
			 const char *title,
			 int width,
			 bool left_justify,
			 Unit *unit,
			 bool enabled);
  void reportEndpointHeader(PathEnd *end,
			    PathEnd *prev_end);
  void reportShort(const PathEndUnconstrained *end,
		   PathExpanded &expanded);
  void reportShort(const PathEndCheck *end,
		   PathExpanded &expanded);
  void reportShort(const PathEndLatchCheck *end,
		   PathExpanded &expanded);
  void reportShort(const PathEndPathDelay *end,
		   PathExpanded &expanded);
  void reportShort(const PathEndOutputDelay *end,
		   PathExpanded &expanded);
  void reportShort(const PathEndGatedClock *end,
		   PathExpanded &expanded);
  void reportShort(const PathEndDataCheck *end,
		   PathExpanded &expanded);
  void reportEndpoint(const PathEndOutputDelay *end);
  void reportEndpointOutputDelay(const PathEndClkConstrained *end);
  void reportEndpoint(const PathEndPathDelay *end);
  void reportEndpoint(const PathEndGatedClock *end);
  string pathEndpoint(PathEnd *end);
  string pathStartpoint(PathEnd *end,
			PathExpanded &expanded);
  void reportBorrowing(const PathEndLatchCheck *end,
		       Arrival &borrow,
		       Arrival &time_given_to_startpoint);
  void reportEndpoint(const PathEndDataCheck *end);
  const char *clkNetworkDelayIdealProp(bool is_ideal);

  string checkRoleReason(const PathEnd *end);
  string checkRoleString(const PathEnd *end);
  virtual void reportGroup(const PathEnd *end);
  void reportStartpoint(const PathEnd *end,
			PathExpanded &expanded);
  void reportUnclockedEndpoint(const PathEnd *end,
			       const char *default_reason);
  void reportEndpoint(const PathEndCheck *end);
  void reportEndpoint(const PathEndLatchCheck *end);
  const char *latchDesc(const PathEndLatchCheck *end);
  void reportStartpoint(const char *start,
			string reason);
  void reportEndpoint(const char *end,
		      string reason);
  void reportStartEndPoint(const char *pt,
			   string reason,
			   const char *key);
  string tgtClkName(const PathEnd *end);
  const char *clkRegLatchDesc(const PathEnd *end);
  void reportSrcPath(const PathEnd *end,
		     PathExpanded &expanded);
  void reportTgtClk(const PathEnd *end);
  void reportTgtClk(const PathEnd *end,
		    float prev_time);
  void reportTgtClk(const PathEnd *end,
		    float prev_time,
		    bool is_prop);
  void reportTgtClk(const PathEnd *end,
                    float prev_time,
                    float src_offset,
                    bool is_prop);
  bool pathFromGenPropClk(const Path *clk_path,
			  const EarlyLate *early_late);
  bool isGenPropClk(const Clock *clk,
		    const RiseFall *clk_rf,
		    const MinMax *min_max,
		    const EarlyLate *early_late);
  void reportSrcClkAndPath(const Path *path,
			   PathExpanded &expanded,
			   float time_offset,
			   Arrival clk_insertion,
			   Arrival clk_latency,
			   bool is_path_delay);
  bool reportGenClkSrcPath(const Path *clk_path,
                           const Clock *clk,
			   const RiseFall *clk_rf,
			   const MinMax *min_max,
			   const EarlyLate *early_late);
  void reportGenClkSrcAndPath(const Path *path,
			      const Clock *clk,
			      const RiseFall *clk_rf,
			      const EarlyLate *early_late,
			      const PathAnalysisPt *path_ap,
			      float time_offset,
			      float path_time_offset,
			      bool clk_used_as_data);
  bool reportGenClkSrcPath1(const Clock *clk,
			    const Pin *clk_pin,
			    const RiseFall *clk_rf,
			    const EarlyLate *early_late,
			    const PathAnalysisPt *path_ap,
			    float gclk_time,
			    float time_offset,
			    bool clk_used_as_data);
  void reportClkSrcLatency(Arrival insertion,
			   float clk_time,
			   const EarlyLate *early_late);
  void reportPathLine(const Path *path,
		      Delay incr,
		      Arrival time,
		      const char *line_case);
  void reportCommonClkPessimism(const PathEnd *end,
				Arrival &clk_arrival);
  void reportClkUncertainty(const PathEnd *end,
			    Arrival &clk_arrival);
  void reportClkLine(const Clock *clk,
		     const char *clk_name,
		     const RiseFall *clk_rf,
		     Arrival clk_time,
		     const MinMax *min_max);
  void reportClkLine(const Clock *clk,
		     const char *clk_name,
		     const RiseFall *clk_rf,
		     Arrival prev_time,
		     Arrival clk_time,
		     const MinMax *min_max);
  void reportRequired(const PathEnd *end,
		      string margin_msg);
  void reportSlack(const PathEnd *end);
  void reportSlack(Slack slack);
  void reportSpaceSlack(PathEnd *end,
                        string &line);
  void reportSpaceSlack(Slack slack,
                        string &line);
  void reportSrcPathArrival(const PathEnd *end,
			    PathExpanded &expanded);
  void reportPath(const PathEnd *end,
		  PathExpanded &expanded);
  void reportPathFull(const Path *path);
  void reportPathHeader();
  void reportPath1(const Path *path,
		   PathExpanded &expanded,
		   bool clk_used_as_data,
		   float time_offset);
  void reportPath2(const Path *path,
		   PathExpanded &expanded,
		   bool clk_used_as_data,
		   float time_offset);
  void  reportPath3(const Path *path,
		    PathExpanded &expanded,
		    bool clk_used_as_data,
		    bool report_clk_path,
		    Arrival prev_time,
		    float time_offset);
  void reportPath4(const Path *path,
		   PathExpanded &expanded,
		   bool clk_used_as_data,
		   bool skip_first_path,
		   bool skip_last_path,
		   float time_offset);
  void reportPath5(const Path *path,
		   PathExpanded &expanded,
		   size_t path_first_index,
		   size_t path_last_index,
		   bool propagated_clk,
		   bool report_clk_path,
		   Arrival prev_time,
		   float time_offset);
  void reportInputExternalDelay(const Path *path,
				float time_offset);
  void reportLine(const char *what,
		  Delay total,
		  const EarlyLate *early_late);
  void reportLineNegative(const char *what,
			  Delay total,
			  const EarlyLate *early_late);
  void reportLine(const char *what,
		  Delay total,
		  const EarlyLate *early_late,
		  const RiseFall *rf);
  void reportLine(const char *what,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late);
  void reportLine(const char *what,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late,
		  const RiseFall *rf);
  void reportLine(const char *what,
		  Slew slew,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late);
  void reportLine(const char *what,
		  float cap,
		  Slew slew,
		  float fanout,
		  Delay incr,
		  Delay total,
		  bool total_with_minus,
		  const EarlyLate *early_late,
		  const RiseFall *rf,
		  string src_attr,
		  const char *line_case);
  void reportLineTotal(const char *what,
		       Delay incr,
		       const EarlyLate *early_late);
  void reportLineTotalMinus(const char *what,
			    Delay decr,
			    const EarlyLate *early_late);
  void reportLineTotal1(const char *what,
			Delay incr,
			bool incr_with_minus,
			const EarlyLate *early_late);
  void reportDashLineTotal();
  void reportDescription(const char *what,
                         string &result);
  void reportDescription(const char *what,
			 bool first_field,
			 bool last_field,
                         string &result);
  void reportFieldTime(float value,
		       ReportField *field,
		       string &result);
  void reportSpaceFieldTime(float value,
			    string &result);
  void reportSpaceFieldDelay(Delay value,
			     const EarlyLate *early_late,
			     string &result);
  void reportFieldDelayMinus(Delay value,
			     const EarlyLate *early_late,
			     ReportField *field,
			     string &result);
  void reportTotalDelay(Delay value,
			const EarlyLate *early_late,
			string &result);
  void reportFieldDelay(Delay value,
			const EarlyLate *early_late,
			ReportField *field,
			string &result);
  void reportField(float value,
		   const ReportField *field,
		   string &result);
  void reportField(const char *value,
		   const ReportField *field,
		   string &result);
  void reportFieldBlank(const ReportField *field,
			string &result);
  void reportDashLine();
  void reportDashLine(int line_width);
  void reportBlankLine();
  string descriptionField(Vertex *vertex);
  bool reportClkPath() const;
  string clkName(const Clock *clk,
		 bool inverted);
  bool hasExtInputDriver(const Pin *pin,
			 const RiseFall *rf,
			 const MinMax *min_max);
  float drvrFanout(Vertex *drvr,
                   const Corner *corner,
		   const MinMax *min_max);
  const char *mpwCheckHiLow(MinPulseWidthCheck *check);
  void reportSkewClkPath(const char *arrival_msg,
			 const PathVertex *clk_path);
  const char *edgeRegLatchDesc(Edge *edge,
			       TimingArc *arc);
  const char *checkRegLatchDesc(const TimingRole *role,
				const RiseFall *clk_rf) const;
  const char *regDesc(const RiseFall *clk_rf) const;
  const char *latchDesc(const RiseFall *clk_rf) const;
  void pathClkPath(const Path *path,
		   PathRef &clk_path) const;
  bool isPropagated(const Path *clk_path);
  bool isPropagated(const Path *clk_path,
		    const Clock *clk);
  bool pathFromClkPin(PathExpanded &expanded);
  bool pathFromClkPin(const Path *path,
		      const Pin *start_pin);
  void latchPaths(const Path *path,
		   // Return values.
		   PathRef &d_path,
		  PathRef &q_path,
		  Edge *&d_q_edge) const;
  bool nextArcAnnotated(const PathRef *next_path,
			size_t next_index,
			PathExpanded &expanded,
			DcalcAPIndex ap_index);
  float tgtClkInsertionOffet(const Path *clk_path,
			     const EarlyLate *early_late,
			     PathAnalysisPt *path_ap);
  // InputDelay used to seed path root.
  InputDelay *pathInputDelay(const Path *path) const;
  void pathInputDelayRefPath(const Path *path,
			     InputDelay *input_delay,
			     // Return value.
			     PathRef &ref_path);
  const char *asRisingFalling(const RiseFall *rf);
  const char *asRiseFall(const RiseFall *rf);
  Delay delayIncr(Delay time,
		  Delay prev,
		  const MinMax *min_max);

  // Path options.
  ReportPathFormat format_;
  ReportFieldSeq fields_;
  bool report_input_pin_;
  bool report_net_;
  bool no_split_;
  int digits_;
  bool report_sigmas_;

  int start_end_pt_width_;

  ReportField *field_description_;
  ReportField *field_total_;
  ReportField *field_incr_;
  ReportField *field_capacitance_;
  ReportField *field_slew_;
  ReportField *field_fanout_;
  ReportField *field_src_attr_;
  ReportField *field_edge_;
  ReportField *field_case_;

  const char *plus_zero_;
  const char *minus_zero_;

  static const float field_blank_;
  static const float field_skip_;
};

class ReportField
{
public:
  ReportField(const char *name,
	      const char *title,
	      int width,
	      bool left_justify,
	      Unit *unit,
	      bool enabled);
  ~ReportField();
  void setProperties(const char *title,
		     int width,
		     bool left_justify);
  const char *name() const { return name_; }
  const char *title() const { return title_; }
  int width() const { return width_; }
  void setWidth(int width);
  bool leftJustify() const { return left_justify_; }
  Unit *unit() const { return unit_; }
  const char *blank() const { return blank_; }
  void setEnabled(bool enabled);
  bool enabled() const { return enabled_; }

protected:
  const char *name_;
  const char *title_;
  int width_;
  bool left_justify_;
  Unit *unit_;
  bool enabled_;
  char *blank_;
};

} // namespace
