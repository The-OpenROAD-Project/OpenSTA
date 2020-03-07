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

#pragma once

#include <string>
#include "DisallowCopyAssign.hh"
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
		       bool report_slew);
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
		     PathEnd *prev_end);
  void reportPathEnds(PathEndSeq *ends);
  void reportPath(const Path *path);

  void reportShort(const PathEndUnconstrained *end,
		   string &result);
  void reportShort(const PathEndCheck *end,
		   string &result);
  void reportShort(const PathEndLatchCheck *end,
		   string &result);
  void reportShort(const PathEndPathDelay *end,
		   string &result);
  void reportShort(const PathEndOutputDelay *end,
		   string &result);
  void reportShort(const PathEndGatedClock *end,
		   string &result);
  void reportShort(const PathEndDataCheck *end,
		   string &result);

  void reportFull(const PathEndUnconstrained *end,
		  string &result);
  void reportFull(const PathEndCheck *end,
		  string &result);
  void reportFull(const PathEndLatchCheck *end,
		  string &result);
  void reportFull(const PathEndPathDelay *end,
		  string &result);
  void reportFull(const PathEndOutputDelay *end,
		  string &result);
  void reportFull(const PathEndGatedClock *end,
		  string &result);
  void reportFull(const PathEndDataCheck *end,
		  string &result);

  void reportEndHeader(string &result);
  void reportEndLine(PathEnd *end,
		     string &result);

  void reportSummaryHeader(string &result);
  void reportSummaryLine(PathEnd *end,
			 string &result);

  void reportSlackOnlyHeader(string &result);
  void reportSlackOnly(PathEnd *end,
		       string &result);

  void reportMpwCheck(MinPulseWidthCheck *check,
		      bool verbose);
  void reportMpwChecks(MinPulseWidthCheckSeq *checks,
		       bool verbose);
  void reportMpwHeaderShort(string &result);
  void reportShort(MinPulseWidthCheck *check,
		   string &result);
  void reportVerbose(MinPulseWidthCheck *check,
		     string &result);

  void reportCheck(MinPeriodCheck *check,
		   bool verbose);
  void reportChecks(MinPeriodCheckSeq *checks,
		    bool verbose);
  void reportPeriodHeaderShort(string &result);
  void reportShort(MinPeriodCheck *check,
		   string &result);
  void reportVerbose(MinPeriodCheck *check,
		     string &result);

  void reportCheck(MaxSkewCheck *check,
		   bool verbose);
  void reportChecks(MaxSkewCheckSeq *checks,
		    bool verbose);
  void reportMaxSkewHeaderShort(string &result);
  void reportShort(MaxSkewCheck *check,
		   string &result);
  void reportVerbose(MaxSkewCheck *check,
		     string &result);

  void reportSlewLimitShortHeader();
  void reportSlewLimitShortHeader(string &result);
  void reportSlewLimitShort(Pin *pin,
			    const RiseFall *rf,
			    Slew slew,
			    float limit,
			    float slack);
  void reportSlewLimitShort(Pin *pin, const
			    RiseFall *rf,
			    Slew slew,
			    float limit,
			    float slack,
			    string &result);
  void reportSlewLimitVerbose(Pin *pin,
			      const Corner *corner,
			      const RiseFall *rf,
			      Slew slew,
			      float limit,
			      float slack,
			      const MinMax *min_max);
  void reportSlewLimitVerbose(Pin *pin,
			      const Corner *corner,
			      const RiseFall *rf,
			      Slew slew,
			      float limit,
			      float slack,
			      const MinMax *min_max,
			      string &result);

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
		   PathExpanded &expanded,
		   string &result);
  void reportShort(const PathEndCheck *end,
		   PathExpanded &expanded,
		   string &result);
  void reportShort(const PathEndLatchCheck *end,
		   PathExpanded &expanded,
		   string &result);
  void reportShort(const PathEndPathDelay *end,
		   PathExpanded &expanded,
		   string &result);
  void reportShort(const PathEndOutputDelay *end,
		   PathExpanded &expanded,
		   string &result);
  void reportShort(const PathEndGatedClock *end,
		   PathExpanded &expanded,
		   string &result);
  void reportShort(const PathEndDataCheck *end,
		   PathExpanded &expanded,
		   string &result);
  void reportEndpoint(const PathEndOutputDelay *end,
		      string &result);
  void reportEndpoint(const PathEndPathDelay *end,
		      string &result);
  void reportEndpoint(const PathEndGatedClock *end,
		      string &result);
  string pathEndpoint(PathEnd *end);
  string pathStartpoint(PathEnd *end,
			PathExpanded &expanded);
  void reportBorrowing(const PathEndLatchCheck *end,
		       Arrival &borrow,
		       Arrival &time_given_to_startpoint,
		       string &result);
  void reportEndpoint(const PathEndDataCheck *end,
		      string &result);
  const char *clkNetworkDelayIdealProp(bool is_ideal);

  string checkRoleReason(const PathEnd *end);
  string checkRoleString(const PathEnd *end);
  virtual void reportGroup(const PathEnd *end,
			   string &result);
  void reportStartpoint(const PathEnd *end,
			PathExpanded &expanded,
			string &result);
  void reportUnclockedEndpoint(const PathEnd *end,
			       const char *default_reason,
			       string &result);
  void reportEndpoint(const PathEndCheck *end,
		      string &result);
  void reportEndpoint(const PathEndLatchCheck *end,
		      string &result);
  const char *latchDesc(const PathEndLatchCheck *end);
  void reportStartpoint(const char *start,
			string reason,
			string &result);
  void reportEndpoint(const char *end,
		      string reason,
		      string &result);
  void reportStartEndPoint(const char *pt,
			   string reason,
			   const char *key,
			   string &result);
  string tgtClkName(const PathEnd *end);
  const char *clkRegLatchDesc(const PathEnd *end);
  void reportSrcPath(const PathEnd *end,
		     PathExpanded &expanded,
		     string &result);
  void reportTgtClk(const PathEnd *end,
		    string &result);
  void reportTgtClk(const PathEnd *end,
		    float prev_time,
		    string &result);
  void reportTgtClk(const PathEnd *end,
		    float prev_time,
		    bool is_prop,
		    string &result);
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
			   bool is_path_delay,
			   string &result);
  bool reportGenClkSrcPath(const Path *clk_path, Clock *clk,
			   const RiseFall *clk_rf,
			   const MinMax *min_max,
			   const EarlyLate *early_late);
  void reportGenClkSrcAndPath(const Path *path,
			      Clock *clk,
			      const RiseFall *clk_rf,
			      const EarlyLate *early_late,
			      const PathAnalysisPt *path_ap,
			      float time_offset,
			      float path_time_offset,
			      bool clk_used_as_data,
			      string &result);
  bool reportGenClkSrcPath1(Clock *clk,
			    const Pin *clk_pin,
			    const RiseFall *clk_rf,
			    const EarlyLate *early_late,
			    const PathAnalysisPt *path_ap,
			    float gclk_time,
			    float time_offset,
			    bool clk_used_as_data,
			    string &result);
  void reportClkSrcLatency(Arrival insertion,
			   float clk_time,
			   const EarlyLate *early_late,
			   string &result);
  void reportPathLine(const Path *path,
		      Delay incr,
		      Arrival time,
		      const char *line_case,
		      string &result);
  void reportCommonClkPessimism(const PathEnd *end,
				Arrival &clk_arrival,
				string &result);
  void reportClkUncertainty(const PathEnd *end,
			    Arrival &clk_arrival,
			    string &result);
  void reportClkLine(const Clock *clk,
		     const char *clk_name,
		     const RiseFall *clk_rf,
		     Arrival clk_time,
		     const MinMax *min_max,
		     string &result);
  void reportClkLine(const Clock *clk,
		     const char *clk_name,
		     const RiseFall *clk_rf,
		     Arrival prev_time,
		     Arrival clk_time,
		     const MinMax *min_max,
		     string &result);
  void reportRequired(const PathEnd *end,
		      string margin_msg,
		      string &result);
  void reportSlack(const PathEnd *end,
		   string &result);
  void reportSlack(Slack slack,
		   string &result);
  void reportSpaceSlack(PathEnd *end,
			string &result);
  void reportSpaceSlack(Slack slack,
			string &result);
  void reportSrcPathArrival(const PathEnd *end,
			    PathExpanded &expanded,
			    string &result);
  void reportPath(const PathEnd *end,
		  PathExpanded &expanded,
		  string &result);
  void reportPath(const Path *path,
		  string &result);
  void reportPathHeader(string &result);
  void reportPath1(const Path *path,
		   PathExpanded &expanded,
		   bool clk_used_as_data,
		   float time_offset,
		   string &result);
  void reportPath2(const Path *path,
		   PathExpanded &expanded,
		   bool clk_used_as_data,
		   float time_offset,
		   string &result);
  void  reportPath3(const Path *path,
		    PathExpanded &expanded,
		    bool clk_used_as_data,
		    bool report_clk_path,
		    Arrival prev_time,
		    float time_offset,
		    string &result);
  void reportPath4(const Path *path,
		   PathExpanded &expanded,
		   bool clk_used_as_data,
		   bool skip_first_path,
		   bool skip_last_path,
		   float time_offset,
		   string &result);
  void reportPath5(const Path *path,
		   PathExpanded &expanded,
		   size_t path_first_index,
		   size_t path_last_index,
		   bool propagated_clk,
		   bool report_clk_path,
		   Arrival prev_time,
		   float time_offset,
		   string &result);
  void reportInputExternalDelay(const Path *path,
				float time_offset,
				string &result);
  void reportLine(const char *what,
		  Delay total,
		  const EarlyLate *early_late,
		  string &result);
  void reportLineNegative(const char *what,
			  Delay total,
			  const EarlyLate *early_late,
			  string &result);
  void reportLine(const char *what,
		  Delay total,
		  const EarlyLate *early_late,
		  const RiseFall *rf,
		  string &result);
  void reportLine(const char *what,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late,
		  string &result);
  void reportLine(const char *what,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late,
		  const RiseFall *rf,
		  string &result);
  void reportLine(const char *what,
		  Slew slew,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late,
		  string &result);
  void reportLine(const char *what,
		  float cap,
		  Slew slew,
		  float fanout,
		  Delay incr,
		  Delay total,
		  bool total_with_minus,
		  const EarlyLate *early_late,
		  const RiseFall *rf,
		  const char *line_case,
		  string &result);
  void reportLineTotal(const char *what,
		       Delay incr,
		       const EarlyLate *early_late,
		       string &result);
  void reportLineTotalMinus(const char *what,
			    Delay decr,
			    const EarlyLate *early_late,
			    string &result);
  void reportLineTotal1(const char *what,
			Delay incr,
			bool incr_with_minus,
			const EarlyLate *early_late,
			string &result);
  void reportDashLineTotal(string &result);
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
		   ReportField *field,
		   string &result);
  void reportField(const char *value,
		   ReportField *field,
		   string &result);
  void reportFieldBlank(ReportField *field,
			string &result);
  void reportDashLine(string &result);
  void reportDashLine(int line_width,
		      string &result);
  void reportEndOfLine(string &result);
  string descriptionField(Vertex *vertex);
  bool reportClkPath() const;
  string clkName(const Clock *clk,
		 bool inverted);
  bool hasExtInputDriver(const Pin *pin,
			 const RiseFall *rf,
			 const MinMax *min_max);
  float loadCap(Pin *drvr_pin,
		const RiseFall *rf,
		DcalcAnalysisPt *dcalc_ap);
  float drvrFanout(Vertex *drvr,
		   const MinMax *min_max);
  const char *mpwCheckHiLow(MinPulseWidthCheck *check);
  void reportSkewClkPath(const char *arrival_msg,
			 const PathVertex *clk_path,
			 string &result);
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
		    Clock *clk);
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
  ReportField *field_edge_;
  ReportField *field_case_;

  const char *plus_zero_;
  const char *minus_zero_;

  static const float field_blank_;
  static const float field_skip_;

private:
  DISALLOW_COPY_AND_ASSIGN(ReportPath);
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
  const char *blank() { return blank_; }
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
