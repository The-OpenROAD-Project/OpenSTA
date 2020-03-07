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

#include <mutex>
#include "MinMax.hh"
#include "StaState.hh"
#include "HashSet.hh"
#include "Transition.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "Delay.hh"
#include "SdcClass.hh"
#include "SearchClass.hh"
#include "SearchPred.hh"
#include "VertexVisitor.hh"

namespace sta {

class BfsFwdIterator;
class BfsBkwdIterator;
class SearchPred;
class SearchThru;
class ClkInfoLess;
class PathEndVisitor;
class ArrivalVisitor;
class RequiredVisitor;
class ClkPathIterator;
class EvalPred;
class TagGroup;
class TagGroupBldr;
class PathGroups;
class WorstSlacks;
class DcalcAnalysisPt;
class VisitPathEnds;
class GatedClk;
class CheckCrpr;
class Genclks;
class Corner;

typedef Set<ClkInfo*, ClkInfoLess> ClkInfoSet;
typedef HashSet<Tag*, TagHash, TagEqual> TagHashSet;
typedef HashSet<TagGroup*, TagGroupHash, TagGroupEqual> TagGroupSet;
typedef Map<Vertex*, Slack> VertexSlackMap;
typedef Vector<VertexSlackMap> VertexSlackMapSeq;
typedef Vector<WorstSlacks> WorstSlacksSeq;

class Search : public StaState
{
public:
  explicit Search(StaState *sta);
  virtual ~Search();
  virtual void copyState(const StaState *sta);
  // Reset to virgin state.
  void clear();
  // When enabled, non-critical path arrivals are pruned to improve
  // run time and reduce memory.
  bool crprPathPruningEnabled() const;
  void setCrprpathPruningEnabled(bool enabled);
  // When path pruning is enabled required times for non-critical paths
  // that have been pruned require additional search. This option
  // disables additional search to returns approximate required times.
  bool crprApproxMissingRequireds() const;
  void setCrprApproxMissingRequireds(bool enabled);

  bool unconstrainedPaths() const { return unconstrained_paths_; }
  // from/thrus/to are owned and deleted by Search.
  // Use corner nullptr to report timing for all corners.
  // Returned sequence is owned by the caller.
  // PathEnds are owned by Search PathGroups and deleted on next call.
  PathEndSeq *findPathEnds(ExceptionFrom *from,
			   ExceptionThruSeq *thrus,
			   ExceptionTo *to,
			   bool unconstrained,
			   const Corner *corner,
			   const MinMaxAll *min_max,
			   int group_count,
			   int endpoint_count,
			   bool unique_pins,
			   float slack_min,
			   float slack_max,
			   bool sort_by_slack,
			   PathGroupNameSet *group_names,
			   bool setup,
			   bool hold,
			   bool recovery,
			   bool removal,
			   bool clk_gating_setup,
			   bool clk_gating_hold);
  // Invalidate all arrival and required times.
  void arrivalsInvalid();
  // Invalidate vertex arrival time.
  void arrivalInvalid(Vertex *vertex);
  void arrivalInvalidDelete(Vertex *vertex);
  void arrivalInvalid(const Pin *pin);
  // Invalidate all required times.
  void requiredsInvalid();
  // Invalidate vertex required time.
  void requiredInvalid(Vertex *vertex);
  void requiredInvalid(Instance *inst);
  void requiredInvalid(const Pin *pin);
  // Vertex will be deleted.
  void deleteVertexBefore(Vertex *vertex);
  // Find all arrival times (propatating thru latches).
  void findAllArrivals();
  // Find all arrivals (without latch propagation).
  void findArrivals();
  virtual void findAllArrivals(VertexVisitor *visitor);
  // Find arrival times up thru level.
  void findArrivals(Level level);
  void findRequireds();
  // Find required times down thru level.
  void findRequireds(Level level);
  bool requiredsSeeded() const { return requireds_seeded_; }
  bool requiredsExist() const { return requireds_exist_; }
  // The sum of all negative endpoints slacks.
  // Incrementally updated.
  Slack totalNegativeSlack(const MinMax *min_max);
  Slack totalNegativeSlack(const Corner *corner,
			   const MinMax *min_max);
  // Worst endpoint slack and vertex.
  // Incrementally updated.
  void worstSlack(const MinMax *min_max,
		  // Return values.
		  Slack &worst_slack,
		  Vertex *&worst_vertex);
  void worstSlack(const Corner *corner,
		  const MinMax *min_max,
		  // Return values.
		  Slack &worst_slack,
		  Vertex *&worst_vertex);
  // Clock arrival respecting ideal clock insertion delay and latency.
  Arrival clkPathArrival(const Path *clk_path) const;
  Arrival clkPathArrival(const Path *clk_path,
			 ClkInfo *clk_info,
			 ClockEdge *clk_edge,
			 const MinMax *min_max,
			 const PathAnalysisPt *path_ap) const;
  // Clock arrival at the path source/launch point.
  Arrival pathClkPathArrival(const Path *path) const;

  PathGroup *pathGroup(const PathEnd *path_end) const;
  void deletePathGroups();
  virtual ExceptionPath *exceptionTo(ExceptionPathType type,
				     const Path *path,
				     const Pin *pin,
				     const RiseFall *rf,
				     const ClockEdge *clk_edge,
				     const MinMax *min_max,
				     bool match_min_max_exactly,
				     bool require_to_pin) const;
  FilterPath *filter() const { return filter_; }
  void deleteFilter();
  void deleteFilteredArrivals();

  // Endpoints are discovered during arrival search, so are only
  // defined after findArrivals.
  VertexSet *endpoints();
  void endpointsInvalid();

  // Clock tree vertices between the clock source pin and register clk pins.
  // This does NOT include generated clock source paths.
  bool isClock(const Vertex *vertex) const;
  // Vertices on propagated generated clock source paths.
  bool isGenClkSrc(const Vertex *vertex) const;
  // The set of clocks that arrive at vertex.
  void clocks(const Vertex *vertex,
	      // Return value.
	      ClockSet &clks) const;
  void clocks(const Pin *pin,
	      // Return value.
	      ClockSet &clks) const;
  void visitStartpoints(VertexVisitor *visitor);
  void visitEndpoints(VertexVisitor *visitor);
  bool havePathGroups() const;
  PathGroup *findPathGroup(const char *name,
			   const MinMax *min_max) const;
  PathGroup *findPathGroup(const Clock *clk,
			   const MinMax *min_max) const;

  ////////////////////////////////////////////////////////////////
  //
  // Somewhat protected functions.
  //
  ////////////////////////////////////////////////////////////////

  // Find arrivals for the clock tree.
  void findClkArrivals();
  void seedArrival(Vertex *vertex);
  EvalPred *evalPred() const { return eval_pred_; }
  SearchPred *searchAdj() const { return search_adj_; }
  Tag *tag(TagIndex index) const;
  TagIndex tagCount() const;
  TagGroupIndex tagGroupCount() const;
  void reportTagGroups() const;
  void reportArrivalCountHistogram() const;
  virtual int clkInfoCount() const;
  virtual bool isEndpoint(Vertex *vertex) const;
  virtual bool isEndpoint(Vertex *vertex,
			  SearchPred *pred) const;
  void endpointInvalid(Vertex *vertex);
  Tag *fromUnclkedInputTag(const Pin *pin,
			   const RiseFall *rf,
			   const MinMax *min_max,
 			   const PathAnalysisPt *path_ap,
			   bool is_segment_start);
  Tag *fromRegClkTag(const Pin *from_pin,
		     const RiseFall *from_rf,
		     Clock *clk,
		     const RiseFall *clk_rf,
		     ClkInfo *clk_info,
		     const Pin *to_pin,
		     const RiseFall *to_rf,
		     const MinMax *min_max,
		     const PathAnalysisPt *path_ap);
  virtual Tag *thruTag(Tag *from_tag,
		       Edge *edge,
		       const RiseFall *to_rf,
		       const MinMax *min_max,
 		       const PathAnalysisPt *path_ap);
  virtual Tag *thruClkTag(PathVertex *from_path,
			  Tag *from_tag,
			  bool to_propagates_clk,
			  Edge *edge,
			  const RiseFall *to_rf,
			  const MinMax *min_max,
			  const PathAnalysisPt *path_ap);
  ClkInfo *thruClkInfo(PathVertex *from_path,
		       ClkInfo *from_tag_clk,
		       Edge *edge,
		       Vertex *to_vertex,
		       const Pin *to_pin,
		       const MinMax *min_max,
 		       const PathAnalysisPt *path_ap);
  ClkInfo *clkInfoWithCrprClkPath(ClkInfo *from_clk_info,
				  PathVertex *from_path,
				  const PathAnalysisPt *path_ap);
  void seedClkArrivals(const Pin *pin,
		       Vertex *vertex,
		       TagGroupBldr *tag_bldr);
  void setVertexArrivals(Vertex *vertex,
			 TagGroupBldr *group_bldr);
  void tnsInvalid(Vertex *vertex);
  bool arrivalsChanged(Vertex *vertex,
		       TagGroupBldr *tag_bldr);
  BfsFwdIterator *arrivalIterator() const { return arrival_iter_; }
  BfsBkwdIterator *requiredIterator() const { return required_iter_; }
  bool arrivalsAtEndpointsExist()const{return arrivals_at_endpoints_exist_;}
  bool makeUnclkedPaths(Vertex *vertex,
			bool is_segment_start,
			TagGroupBldr *tag_bldr);
  bool isSegmentStart(const Pin *pin);
  bool isInputArrivalSrchStart(Vertex *vertex);
  void seedInputSegmentArrival(const Pin *pin,
			       Vertex *vertex,
			       TagGroupBldr *tag_bldr);
  void enqueueLatchDataOutputs(Vertex *vertex);
  virtual void seedRequired(Vertex *vertex);
  virtual void seedRequiredEnqueueFanin(Vertex *vertex);
  void seedInputDelayArrival(const Pin *pin,
			     Vertex *vertex,
			     InputDelay *input_delay);
  void seedInputDelayArrival(const Pin *pin,
			     Vertex *vertex,
			     InputDelay *input_delay,
			     bool is_segment_start,
			     TagGroupBldr *tag_bldr);
  // Insertion delay for regular or generated clock.
  Arrival clockInsertion(const Clock *clk,
			 const Pin *pin,
			 const RiseFall *rf,
			 const MinMax *min_max,
			 const EarlyLate *early_late,
			 const PathAnalysisPt *path_ap) const;
  bool propagateClkSense(const Pin *from_pin,
			 Path *from_path,
			 const RiseFall *to_rf);

  Tag *findTag(const RiseFall *rf,
	       const PathAnalysisPt *path_ap,
	       ClkInfo *tag_clk,
	       bool is_clk,
	       InputDelay *input_delay,
	       bool is_segment_start,
	       ExceptionStateSet *states,
	       bool own_states);
  void reportTags() const;
  void reportClkInfos() const;
  virtual ClkInfo *findClkInfo(ClockEdge *clk_edge,
			       const Pin *clk_src,
			       bool is_propagated,
			       const Pin *gen_clk_src,
			       bool gen_clk_src_path,
			       const RiseFall *pulse_clk_sense,
			       Arrival insertion,
			       float latency,
			       ClockUncertainties *uncertainties,
			       const PathAnalysisPt *path_ap,
			       PathVertex *crpr_clk_path);
  ClkInfo *findClkInfo(ClockEdge *clk_edge,
		       const Pin *clk_src,
		       bool is_propagated,
		       Arrival insertion,
		       const PathAnalysisPt *path_ap);
  // Timing derated arc delay for a path analysis point.
  ArcDelay deratedDelay(Vertex *from_vertex,
			TimingArc *arc,
			Edge *edge,
			bool is_clk,
			const PathAnalysisPt *path_ap);

  TagGroup *tagGroup(const Vertex *vertex) const;
  TagGroup *tagGroup(TagGroupIndex index) const;
  void reportArrivals(Vertex *vertex) const;
  Slack wnsSlack(Vertex *vertex,
		 PathAPIndex path_ap_index);
  void levelChangedBefore(Vertex *vertex);
  void seedInputArrival(const Pin *pin,
 			Vertex *vertex,
 			TagGroupBldr *tag_bldr);
  void ensureDownstreamClkPins();
  bool pathPropagatedToClkSrc(const Pin *pin,
			      Path *path);
  // Check paths from inputs from the default arrival clock
  // (missing set_input_delay).
  virtual bool checkDefaultArrivalPaths() { return true; }
  bool matchesFilter(Path *path,
		     const ClockEdge *to_clk_edge);
  CheckCrpr *checkCrpr() { return check_crpr_; }
  VisitPathEnds *visitPathEnds() { return visit_path_ends_; }
  GatedClk *gatedClk() { return gated_clk_; }
  Genclks *genclks() { return genclks_; }
  void findClkVertexPins(PinSet &clk_pins);

protected:
  void init(StaState *sta);
  void initVars();
  void makeAnalysisPts(AnalysisType analysis_type);
  void makeAnalysisPts(bool swap_clk_min_max,
		       bool report_min,
		       bool report_max,
		       DcalcAnalysisPt *dcalc_ap_min,
		       DcalcAnalysisPt *dcalc_ap_max);
  virtual void deleteTags();
  void seedInvalidArrivals();
  void seedArrivals();
  void findClockVertices(VertexSet &vertices);
  void seedClkDataArrival(const Pin *pin,
			  const RiseFall *rf,
			  Clock *clk,
			  ClockEdge *clk_edge,
			  const MinMax *min_max,
			  const PathAnalysisPt *path_ap,
			  Arrival insertion,
			  TagGroupBldr *tag_bldr);
  void seedClkArrival(const Pin *pin,
		      const RiseFall *rf,
		      Clock *clk,
		      ClockEdge *clk_edge,
		      const MinMax *min_max,
		      const PathAnalysisPt *path_ap,
		      Arrival insertion,
		      TagGroupBldr *tag_bldr);
  Tag *clkDataTag(const Pin *pin,
		  Clock *clk,
		  const RiseFall *rf,
		  ClockEdge *clk_edge,
		  Arrival insertion,
		  const MinMax *min_max,
		  const PathAnalysisPt *path_ap);
  void findInputArrivalVertices(VertexSet &vertices);
  void seedInputArrivals(ClockSet *clks);
  void findRootVertices(VertexSet &vertices);
  void findInputDrvrVertices(VertexSet &vertices);
  void seedInputArrival1(const Pin *pin,
			 Vertex *vertex,
			 bool is_segment_start,
			 TagGroupBldr *tag_bldr);
  void seedInputArrival(const Pin *pin,
			Vertex *vertex,
			ClockSet *wrt_clks);
  void seedInputDelayArrival(const Pin *pin,
			     InputDelay *input_delay,
			     ClockEdge *clk_edge,
			     float clk_arrival,
			     float clk_insertion,
			     float clk_latency,
			     bool is_segment_start,
			     const MinMax *min_max,
			     PathAnalysisPt *path_ap,
			     TagGroupBldr *tag_bldr);
  void seedInputDelayArrival(const Pin *pin,
			     const RiseFall *rf,
			     float arrival,
			     InputDelay *input_delay,
			     ClockEdge *clk_edge,
			     float clk_insertion,
			     float clk_latency,
			     bool is_segment_start,
			     const MinMax *min_max,
			     PathAnalysisPt *path_ap,
			     TagGroupBldr *tag_bldr);
  void inputDelayClkArrival(InputDelay *input_delay,
			    ClockEdge *clk_edge,
			    const MinMax *min_max,
			    const PathAnalysisPt *path_ap,
			    // Return values.
			    float &clk_arrival,
			    float &clk_insertion,
			    float &clk_latency);
  void inputDelayRefPinArrival(Path *ref_path,
			       ClockEdge *clk_edge,
			       const MinMax *min_max,
			       // Return values.
			       float &ref_arrival,
			       float &ref_insertion,
			       float &ref_latency);
  Tag *inputDelayTag(const Pin *pin,
		     const RiseFall *rf,
		     ClockEdge *clk_edge,
		     float clk_insertion,
		     float clk_latency,
		     InputDelay *input_delay,
		     bool is_segment_start,
		     const MinMax *min_max,
		     const PathAnalysisPt *path_ap);
  void seedClkVertexArrivals();
  void seedClkVertexArrivals(const Pin *pin,
			     Vertex *vertex);
  void findClkArrivals1();

  virtual void findArrivals(Level level,
			    VertexVisitor *arrival_visitor);
  Tag *mutateTag(Tag *from_tag,
		 const Pin *from_pin,
		 const RiseFall *from_rf,
		 bool from_is_clk,
		 ClkInfo *from_clk_info,
		 const Pin *to_pin,
		 const RiseFall *to_rf,
		 bool to_is_clk,
		 bool to_is_reg_clk,
		 bool to_is_segment_start,
		 ClkInfo *to_clk_info,
		 InputDelay *to_input_delay,
		 const MinMax *min_max,
		 const PathAnalysisPt *path_ap);
  ExceptionPath *exceptionTo(const Path *path,
			     const Pin *pin,
			     const RiseFall *rf,
			     const ClockEdge *clk_edge,
			     const MinMax *min_max) const;
  void seedRequireds();
  void seedInvalidRequireds();
  bool havePendingLatchOutputs();
  void clearPendingLatchOutputs();
  void enqueuePendingLatchOutputs();
  void findFilteredArrivals();
  void findArrivals1();
  void seedFilterStarts();
  bool hasEnabledChecks(Vertex *vertex) const;
  virtual float timingDerate(Vertex *from_vertex,
			     TimingArc *arc,
			     Edge *edge,
			     bool is_clk,
			     const PathAnalysisPt *path_ap);
  void deletePaths();
  void deletePaths(Vertex *vertex);
  void deletePaths1(Vertex *vertex);
  TagGroup *findTagGroup(TagGroupBldr *group_bldr);
  void deleteFilterTags();
  void deleteFilterTagGroups();
  void deleteFilterClkInfos();

  void tnsPreamble();
  void findTotalNegativeSlacks();
  void updateInvalidTns();
  void clearWorstSlack();
  void wnsSlacks(Vertex *vertex,
		 // Return values.
		 SlackSeq &slacks);
  void wnsTnsPreamble();
  void worstSlackPreamble();
  void deleteWorstSlacks();
  void updateWorstSlacks(Vertex *vertex,
			 Slack slacks);
  void updateTns(Vertex *vertex,
		 SlackSeq &slacks);
  void tnsIncr(Vertex *vertex,
	       Slack slack,
	       PathAPIndex path_ap_index);
  void tnsDecr(Vertex *vertex,
	       PathAPIndex path_ap_index);
  void tnsNotifyBefore(Vertex *vertex);
  PathGroups *makePathGroups(int group_count,
			     int endpoint_count,
			     bool unique_pins,
			     float min_slack,
			     float max_slack,
			     PathGroupNameSet *group_names,
			     bool setup,
			     bool hold,
			     bool recovery,
			     bool removal,
			     bool clk_gating_setup,
			     bool clk_gating_hold);
  bool matchesFilterTo(Path *path,
		       const ClockEdge *to_clk_edge) const;
  void pathClkPathArrival1(const Path *path,
			   // Return value.
			   PathRef &clk_path) const;

  ////////////////////////////////////////////////////////////////

  // findPathEnds arg.
  bool unconstrained_paths_;
  bool crpr_path_pruning_enabled_;
  bool crpr_approx_missing_requireds_;
  // Search predicates.
  SearchPred *search_adj_;
  SearchPred *search_clk_;
  EvalPred *eval_pred_;
  ArrivalVisitor *arrival_visitor_;
  // Clock arrivals are known.
  bool clk_arrivals_valid_;
  // Some arrivals exist.
  bool arrivals_exist_;
  // Arrivals at end points exist (but may be invalid).
  bool arrivals_at_endpoints_exist_;
  // Arrivals at start points have been initialized.
  bool arrivals_seeded_;
  // Some requireds exist.
  bool requireds_exist_;
  // Requireds have been seeded by searching arrivals to all endpoints.
  bool requireds_seeded_;
  // Vertices with invalid arrival times to update and search from.
  VertexSet invalid_arrivals_;
  std::mutex invalid_arrivals_lock_;
  BfsFwdIterator *arrival_iter_;
  // Vertices with invalid required times to update and search from.
  VertexSet invalid_requireds_;
  BfsBkwdIterator *required_iter_;
  bool tns_exists_;
  // Endpoint vertices with slacks that have changed since tns was found.
  VertexSet invalid_tns_;
  // Indexed by path_ap->index().
  SlackSeq tns_;
  // Indexed by path_ap->index().
  VertexSlackMapSeq tns_slacks_;
  std::mutex tns_lock_;
  // Indexed by path_ap->index().
  WorstSlacks *worst_slacks_;
  // Use pointer to clk_info set so Tag.hh does not need to be included.
  ClkInfoSet *clk_info_set_;
  std::mutex clk_info_lock_;
  // Use pointer to tag set so Tag.hh does not need to be included.
  TagHashSet *tag_set_;
  // Entries in tags_ may be missing where previous filter tags were deleted.
  TagIndex tag_capacity_;
  Tag **tags_;
  TagIndex tag_next_;
  // Holes in tags_ left by deleting filter tags.
  std::vector<TagIndex> tag_free_indices_;
  std::mutex tag_lock_;
  TagGroupSet *tag_group_set_;
  TagGroup **tag_groups_;
  TagGroupIndex tag_group_next_;
  // Holes in tag_groups_ left by deleting filter tag groups.
  std::vector<TagIndex> tag_group_free_indices_;
  // Capacity of tag_groups_.
  TagGroupIndex tag_group_capacity_;
  std::mutex tag_group_lock_;
  // Latches data outputs to queue on the next search pass.
  VertexSet pending_latch_outputs_;
  std::mutex pending_latch_outputs_lock_;
  VertexSet *endpoints_;
  VertexSet *invalid_endpoints_;
  // Filter exception to tag arrivals for
  // report_timing -from pin|inst -through.
  // -to is always nullptr.
  FilterPath *filter_;
  // filter_from_ is owned by filter_ if it exists.
  ExceptionFrom *filter_from_;
  ExceptionTo *filter_to_;
  bool found_downstream_clk_pins_;
  PathGroups *path_groups_;
  VisitPathEnds *visit_path_ends_;
  GatedClk *gated_clk_;
  CheckCrpr *check_crpr_;
  Genclks *genclks_;
};

// Eval across latch D->Q edges.
// SearchPred0 unless
//  timing check edge
//  disabled loop
//  disabled converging clock edge (Xilinx)
//  clk source pin
class EvalPred : public SearchPred0
{
public:
  explicit EvalPred(const StaState *sta);
  virtual bool searchThru(Edge *edge);
  void setSearchThruLatches(bool thru_latches);
  virtual bool searchTo(const Vertex *to_vertex);

protected:
  bool search_thru_latches_;
};

class ClkArrivalSearchPred : public EvalPred
{
public:
  ClkArrivalSearchPred(const StaState *sta);
  virtual bool searchThru(Edge *edge);

private:
  DISALLOW_COPY_AND_ASSIGN(ClkArrivalSearchPred);
};

// Class for visiting fanin/fanout paths of a vertex.
// This used by forward/backward search to find arrival/required path times.
class PathVisitor : public VertexVisitor
{
public:
  // Uses search->evalPred() for search predicate.
  explicit PathVisitor(const StaState *sta);
  PathVisitor(SearchPred *pred,
	      const StaState *sta);
  virtual void visitFaninPaths(Vertex *to_vertex);
  virtual void visitFanoutPaths(Vertex *from_vertex);

protected:
  // Return false to stop visiting.
  virtual bool visitEdge(const Pin *from_pin, Vertex *from_vertex,
			 Edge *edge, const Pin *to_pin, Vertex *to_vertex);
  // Return false to stop visiting.
  bool visitArc(const Pin *from_pin,
		Vertex *from_vertex,
		const RiseFall *from_rf,
		PathVertex *from_path,
		Edge *edge,
		TimingArc *arc,
		const Pin *to_pin,
		Vertex *to_vertex,
		const MinMax *min_max,
		PathAnalysisPt *path_ap);
  // This calls visit below with everything required to make to_path.
  // Return false to stop visiting.
  virtual bool visitFromPath(const Pin *from_pin,
			     Vertex *from_vertex,
			     const RiseFall *from_rf,
			     PathVertex *from_path,
			     Edge *edge,
			     TimingArc *arc,
			     const Pin *to_pin,
			     Vertex *to_vertex,
			     const RiseFall *to_rf,
			     const MinMax *min_max,
			     const PathAnalysisPt *path_ap);
  // Return false to stop visiting.
  virtual bool visitFromToPath(const Pin *from_pin,
			       Vertex *from_vertex,
			       const RiseFall *from_rf,
			       Tag *from_tag,
			       PathVertex *from_path,
			       Edge *edge,
			       TimingArc *arc,
			       ArcDelay arc_delay,
			       Vertex *to_vertex,
			       const RiseFall *to_rf,
			       Tag *to_tag,
			       Arrival &to_arrival,
			       const MinMax *min_max,
			       const PathAnalysisPt *path_ap) = 0;
  SearchPred *pred_;
  const StaState *sta_;
};

// Visitor called during forward search to record an
// arrival at an path.
class ArrivalVisitor : public PathVisitor
{
public:
  explicit ArrivalVisitor(const StaState *sta);
  virtual ~ArrivalVisitor();
  // Initialize the visitor.
  // Defaults pred to search->eval_pred_.
  void init(bool always_to_endpoints);
  void init(bool always_to_endpoints,
	    SearchPred *pred);
  virtual void visit(Vertex *vertex);
  virtual VertexVisitor *copy();
  // Return false to stop visiting.
  virtual bool visitFromToPath(const Pin *from_pin,
			       Vertex *from_vertex,
			       const RiseFall *from_rf,
			       Tag *from_tag,
			       PathVertex *from_path,
			       Edge *edge,
			       TimingArc *arc,
			       ArcDelay arc_delay,
			       Vertex *to_vertex,
			       const RiseFall *to_rf,
			       Tag *to_tag,
			       Arrival &to_arrival,
			       const MinMax *min_max,
			       const PathAnalysisPt *path_ap);
  void setAlwaysToEndpoints(bool to_endpoints);
  TagGroupBldr *tagBldr() const { return tag_bldr_; }

protected:
  ArrivalVisitor(bool always_to_endpoints,
		 SearchPred *pred,
		 const StaState *sta);
  void init0();
  void enqueueRefPinInputDelays(const Pin *ref_pin);
  void seedInputDelayArrival(const Pin *pin,
			     Vertex *vertex,
			     InputDelay *input_delay);
  void pruneCrprArrivals();
  void constrainedRequiredsInvalid(Vertex *vertex,
				   bool is_clk);
  bool always_to_endpoints_;
  TagGroupBldr *tag_bldr_;
  TagGroupBldr *tag_bldr_no_crpr_;
  SearchPred *adj_pred_;
  bool crpr_active_;
  bool has_fanin_one_;
};

class RequiredCmp
{
public:
  RequiredCmp();
  void requiredsInit(Vertex *vertex,
		     const StaState *sta);
  void requiredSet(int arrival_index,
		   Required required,
		   const MinMax *min_max);
  // Return true if the requireds changed.
  bool requiredsSave(Vertex *vertex,
		     const StaState *sta);
  Required required(int arrival_index);

protected:
  ArrivalSeq requireds_;
  bool have_requireds_;
};

// Visitor called during backward search to record a
// required time at an path.
class RequiredVisitor : public PathVisitor
{
public:
  explicit RequiredVisitor(const StaState *sta);
  virtual ~RequiredVisitor();
  virtual VertexVisitor *copy();
  virtual void visit(Vertex *vertex);

protected:
  // Return false to stop visiting.
  virtual bool visitFromToPath(const Pin *from_pin,
			       Vertex *from_vertex,
			       const RiseFall *from_rf,
			       Tag *from_tag,
			       PathVertex *from_path,
			       Edge *edge,
			       TimingArc *arc,
			       ArcDelay arc_delay,
			       Vertex *to_vertex,
			       const RiseFall *to_rf,
			       Tag *to_tag,
			       Arrival &to_arrival,
			       const MinMax *min_max,
			       const PathAnalysisPt *path_ap);

  RequiredCmp *required_cmp_;
  VisitPathEnds *visit_path_ends_;
};

// This does not use SearchPred as a base class to avoid getting
// two sets of StaState variables when multiple inheritance is used
// to add the functions in this class to another.
class DynLoopSrchPred
{
public:
  explicit DynLoopSrchPred(TagGroupBldr *tag_bldr);

protected:
  bool loopEnabled(Edge *edge,
		   const Sdc *sdc,
		   const Graph *graph,
		   Search *search);
  bool hasPendingLoopPaths(Edge *edge,
			   const Graph *graph,
			   Search *search);

  TagGroupBldr *tag_bldr_;

private:
  DISALLOW_COPY_AND_ASSIGN(DynLoopSrchPred);
};

} // namespace
