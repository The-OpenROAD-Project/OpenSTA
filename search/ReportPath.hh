// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

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
  ReportPath(StaState *sta);
  virtual ~ReportPath();
  ReportPathFormat pathFormat() const { return format_; }
  void setPathFormat(ReportPathFormat format);
  void setReportFieldOrder(StringSeq *field_names);
  void setReportFields(bool report_input_pin,
                       bool report_hier_pins,
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
  ReportField *findField(const char *name) const;

  // Header above reportPathEnd results.
  void reportPathEndHeader() const;
  // Footer below reportPathEnd results.
  void reportPathEndFooter() const;
  void reportPathEnd(const PathEnd *end) const;
  // Format report_path_endpoint only:
  //   Previous path end is used to detect path group changes
  //   so headers are reported by group.
  void reportPathEnd(const PathEnd *end,
		     const PathEnd *prev_end,
                     bool last) const;
  void reportPathEnds(const PathEndSeq *ends) const;
  void reportPath(const Path *path) const;

  void reportShort(const PathEndUnconstrained *end) const;
  void reportShort(const PathEndCheck *end) const;
  void reportShort(const PathEndLatchCheck *end) const;
  void reportShort(const PathEndPathDelay *end) const;
  void reportShort(const PathEndOutputDelay *end) const;
  void reportShort(const PathEndGatedClock *end) const;
  void reportShort(const PathEndDataCheck *end) const;

  void reportFull(const PathEndUnconstrained *end) const;
  void reportFull(const PathEndCheck *end) const;
  void reportFull(const PathEndLatchCheck *end) const;
  void reportFull(const PathEndPathDelay *end) const;
  void reportFull(const PathEndOutputDelay *end) const;
  void reportFull(const PathEndGatedClock *end) const;
  void reportFull(const PathEndDataCheck *end) const;

  void reportJsonHeader() const;
  void reportJsonFooter() const;
  void reportJson(const PathEnd *end,
                  bool last) const;
  void reportJson(const Path *path) const;
  void reportJson(const Path *path,
                  const char *path_name,
                  int indent,
                  bool trailing_comma,
                  string &result) const;
  void reportJson(const PathExpanded &expanded,
                  const char *path_name,
                  int indent,
                  bool trailing_comma,
                  string &result) const;

  void reportEndHeader() const;
  void reportEndLine(const PathEnd *end) const;

  void reportSummaryHeader() const;
  void reportSummaryLine(const PathEnd *end) const;

  void reportSlackOnlyHeader() const;
  void reportSlackOnly(const PathEnd *end) const;

  void reportMpwCheck(const MinPulseWidthCheck *check,
		      bool verbose) const;
  void reportMpwChecks(const MinPulseWidthCheckSeq *checks,
		       bool verbose) const;
  void reportMpwHeaderShort() const;
  void reportShort(const MinPulseWidthCheck *check) const;
  void reportVerbose(const MinPulseWidthCheck *check) const;

  void reportCheck(const MinPeriodCheck *check,
		   bool verbose) const;
  void reportChecks(const MinPeriodCheckSeq *checks,
		    bool verbose) const;
  void reportPeriodHeaderShort() const;
  void reportShort(const MinPeriodCheck *check) const;
  void reportVerbose(const MinPeriodCheck *check) const;

  void reportCheck(const MaxSkewCheck *check,
		   bool verbose) const;
  void reportChecks(const MaxSkewCheckSeq *checks,
		    bool verbose) const;
  void reportMaxSkewHeaderShort() const;
  void reportShort(const MaxSkewCheck *check) const;
  void reportVerbose(const MaxSkewCheck *check) const;

  void reportLimitShortHeader(const ReportField *field) const;
  void reportLimitShort(const ReportField *field,
			Pin *pin,
			float value,
			float limit,
			float slack) const;
  void reportLimitVerbose(const ReportField *field,
			  Pin *pin,
			  const RiseFall *rf,
			  float value,
			  float limit,
			  float slack,
                          const Corner *corner,
			  const MinMax *min_max) const;
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
  void reportEndpointHeader(const PathEnd *end,
			    const PathEnd *prev_end) const;
  void reportShort(const PathEndUnconstrained *end,
		   const PathExpanded &expanded) const;
  void reportShort(const PathEndCheck *end,
		   const PathExpanded &expanded) const;
  void reportShort(const PathEndLatchCheck *end,
		   const PathExpanded &expanded) const;
  void reportShort(const PathEndPathDelay *end,
		   const PathExpanded &expanded) const;
  void reportShort(const PathEndOutputDelay *end,
		   const PathExpanded &expanded) const;
  void reportShort(const PathEndGatedClock *end,
		   const PathExpanded &expanded) const;
  void reportShort(const PathEndDataCheck *end,
		   const PathExpanded &expanded) const;
  void reportEndpoint(const PathEndOutputDelay *end) const;
  void reportEndpointOutputDelay(const PathEndClkConstrained *end) const;
  void reportEndpoint(const PathEndPathDelay *end) const;
  void reportEndpoint(const PathEndGatedClock *end) const;
  string pathEndpoint(const PathEnd *end) const;
  string pathStartpoint(const PathEnd *end,
			const PathExpanded &expanded) const;
  void reportBorrowing(const PathEndLatchCheck *end,
		       Arrival &borrow,
		       Arrival &time_given_to_startpoint) const;
  void reportEndpoint(const PathEndDataCheck *end) const;
  const char *clkNetworkDelayIdealProp(bool is_ideal) const;

  string checkRoleReason(const PathEnd *end) const;
  string checkRoleString(const PathEnd *end) const;
  virtual void reportGroup(const PathEnd *end) const;
  void reportStartpoint(const PathEnd *end,
			const PathExpanded &expanded) const;
  void reportUnclockedEndpoint(const PathEnd *end,
			       const char *default_reason) const;
  void reportEndpoint(const PathEndCheck *end) const;
  void reportEndpoint(const PathEndLatchCheck *end) const;
  const char *latchDesc(const PathEndLatchCheck *end) const;
  void reportStartpoint(const char *start,
			const string reason) const;
  void reportEndpoint(const char *end,
		      const string reason) const;
  void reportStartEndPoint(const char *pt,
			   const string reason,
			   const char *key) const;
  string tgtClkName(const PathEnd *end) const;
  const char *clkRegLatchDesc(const PathEnd *end) const;
  void reportSrcPath(const PathEnd *end,
		     const PathExpanded &expanded) const;
  void reportTgtClk(const PathEnd *end) const;
  void reportTgtClk(const PathEnd *end,
		    float prev_time) const;
  void reportTgtClk(const PathEnd *end,
		    float prev_time,
		    bool is_prop) const;
  void reportTgtClk(const PathEnd *end,
                    float prev_time,
                    float src_offset,
                    bool is_prop) const;
  bool pathFromGenPropClk(const Path *clk_path,
			  const EarlyLate *early_late) const;
  bool isGenPropClk(const Clock *clk,
		    const RiseFall *clk_rf,
		    const MinMax *min_max,
		    const EarlyLate *early_late) const;
  void reportSrcClkAndPath(const Path *path,
			   const PathExpanded &expanded,
			   float time_offset,
			   Arrival clk_insertion,
			   Arrival clk_latency,
			   bool is_path_delay) const;
  bool reportGenClkSrcPath(const Path *clk_path,
                           const Clock *clk,
			   const RiseFall *clk_rf,
			   const MinMax *min_max,
			   const EarlyLate *early_late) const;
  void reportGenClkSrcAndPath(const Path *path,
			      const Clock *clk,
			      const RiseFall *clk_rf,
			      const EarlyLate *early_late,
			      const PathAnalysisPt *path_ap,
			      float time_offset,
			      float path_time_offset,
			      bool clk_used_as_data) const;
  bool reportGenClkSrcPath1(const Clock *clk,
			    const Pin *clk_pin,
			    const RiseFall *clk_rf,
			    const EarlyLate *early_late,
			    const PathAnalysisPt *path_ap,
			    float gclk_time,
			    float time_offset,
			    bool clk_used_as_data) const;
  void reportClkSrcLatency(Arrival insertion,
			   float clk_time,
			   const EarlyLate *early_late) const;
  void reportPathLine(const Path *path,
		      Delay incr,
		      Arrival time,
		      const char *line_case) const;
  void reportCommonClkPessimism(const PathEnd *end,
				Arrival &clk_arrival) const ;
  void reportClkUncertainty(const PathEnd *end,
			    Arrival &clk_arrival) const ;
  void reportClkLine(const Clock *clk,
		     const char *clk_name,
		     const RiseFall *clk_rf,
		     Arrival clk_time,
		     const MinMax *min_max) const ;
  void reportClkLine(const Clock *clk,
		     const char *clk_name,
		     const RiseFall *clk_rf,
		     Arrival prev_time,
		     Arrival clk_time,
		     const MinMax *min_max) const ;
  void reportRequired(const PathEnd *end,
		      string margin_msg) const ;
  void reportSlack(const PathEnd *end) const ;
  void reportSlack(Slack slack) const ;
  void reportSpaceSlack(const PathEnd *end,
                        string &line) const ;
  void reportSpaceSlack(Slack slack,
                        string &line) const ;
  void reportSrcPathArrival(const PathEnd *end,
			    const PathExpanded &expanded) const ;
  void reportPath(const PathEnd *end,
		  const PathExpanded &expanded) const;
  void reportPathFull(const Path *path) const;
  void reportPathHeader() const;
  void reportPath1(const Path *path,
		   const PathExpanded &expanded,
		   bool clk_used_as_data,
		   float time_offset) const;
  void reportPath2(const Path *path,
		   const PathExpanded &expanded,
		   bool clk_used_as_data,
		   float time_offset) const;
  void  reportPath3(const Path *path,
		    const PathExpanded &expanded,
		    bool clk_used_as_data,
		    bool report_clk_path,
		    Arrival prev_time,
		    float time_offset) const;
  void reportPath4(const Path *path,
		   const PathExpanded &expanded,
		   bool clk_used_as_data,
		   bool skip_first_path,
		   bool skip_last_path,
		   float time_offset) const;
  void reportPath5(const Path *path,
		   const PathExpanded &expanded,
		   size_t path_first_index,
		   size_t path_last_index,
		   bool propagated_clk,
		   bool report_clk_path,
		   Arrival prev_time,
		   float time_offset) const;
  void reportHierPinsThru(const Path *path) const;
  void reportInputExternalDelay(const Path *path,
				float time_offset) const;
  void reportLine(const char *what,
		  Delay total,
		  const EarlyLate *early_late) const;
  void reportLineNegative(const char *what,
			  Delay total,
			  const EarlyLate *early_late) const;
  void reportLine(const char *what,
		  Delay total,
		  const EarlyLate *early_late,
		  const RiseFall *rf) const;
  void reportLine(const char *what,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late) const;
  void reportLine(const char *what,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late,
		  const RiseFall *rf) const;
  void reportLine(const char *what,
		  Slew slew,
		  Delay incr,
		  Delay total,
		  const EarlyLate *early_late) const;
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
		  const char *line_case) const;
  void reportLineTotal(const char *what,
		       Delay incr,
		       const EarlyLate *early_late) const;
  void reportLineTotalMinus(const char *what,
			    Delay decr,
			    const EarlyLate *early_late) const;
  void reportLineTotal1(const char *what,
			Delay incr,
			bool incr_with_minus,
			const EarlyLate *early_late) const;
  void reportDashLineTotal() const;
  void reportDescription(const char *what,
                         string &result) const;
  void reportDescription(const char *what,
			 bool first_field,
			 bool last_field,
                         string &result) const;
  void reportFieldTime(float value,
		       ReportField *field,
		       string &result) const;
  void reportSpaceFieldTime(float value,
			    string &result) const;
  void reportSpaceFieldDelay(Delay value,
			     const EarlyLate *early_late,
			     string &result) const;
  void reportFieldDelayMinus(Delay value,
			     const EarlyLate *early_late,
			     const ReportField *field,
			     string &result) const;
  void reportTotalDelay(Delay value,
			const EarlyLate *early_late,
			string &result) const;
  void reportFieldDelay(Delay value,
			const EarlyLate *early_late,
			const ReportField *field,
			string &result) const;
  void reportField(float value,
		   const ReportField *field,
		   string &result) const;
  void reportField(const char *value,
		   const ReportField *field,
		   string &result) const;
  void reportFieldBlank(const ReportField *field,
			string &result) const;
  void reportDashLine() const;
  void reportDashLine(int line_width) const;
  void reportBlankLine() const;
  string descriptionField(const Vertex *vertex) const;
  string descriptionField(const Pin *pin) const;
  string descriptionNet(const Pin *pin) const;
  bool reportClkPath() const;
  string clkName(const Clock *clk,
		 bool inverted) const;
  bool hasExtInputDriver(const Pin *pin,
			 const RiseFall *rf,
			 const MinMax *min_max) const;
  float drvrFanout(Vertex *drvr,
                   const Corner *corner,
		   const MinMax *min_max) const;
  const char *mpwCheckHiLow(const MinPulseWidthCheck *check) const;
  void reportSkewClkPath(const char *arrival_msg,
			 const Path *clk_path) const;
  const char *edgeRegLatchDesc(const Edge *edge,
			       const TimingArc *arc) const;
  const char *checkRegLatchDesc(const TimingRole *role,
				const RiseFall *clk_rf) const;
  const char *regDesc(const RiseFall *clk_rf) const;
  const char *latchDesc(const RiseFall *clk_rf) const;
  void pathClkPath(const Path *path,
		   const Path &clk_path) const;
  bool isPropagated(const Path *clk_path) const;
  bool isPropagated(const Path *clk_path,
		    const Clock *clk) const;
  bool pathFromClkPin(const PathExpanded &expanded) const;
  bool pathFromClkPin(const Path *path,
		      const Pin *start_pin) const;
  void latchPaths(const Path *path,
                  // Return values.
                  Path &d_path,
		  Path &q_path,
		  Edge *&d_q_edge) const;
  bool nextArcAnnotated(const Path *next_path,
			size_t next_index,
			const PathExpanded &expanded,
			DcalcAPIndex ap_index) const;
  float tgtClkInsertionOffet(const Path *clk_path,
			     const EarlyLate *early_late,
			     const PathAnalysisPt *path_ap) const;
  // InputDelay used to seed path root.
  InputDelay *pathInputDelay(const Path *path) const;
  void pathInputDelayRefPath(const Path *path,
			     const InputDelay *input_delay,
			     // Return value.
			     Path &ref_path) const;
  const char *asRisingFalling(const RiseFall *rf) const;
  const char *asRiseFall(const RiseFall *rf) const;
  Delay delayIncr(Delay time,
		  Delay prev,
		  const MinMax *min_max) const;

  // Path options.
  ReportPathFormat format_;
  ReportFieldSeq fields_;
  bool report_input_pin_;
  bool report_hier_pins_;
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
