// opensta, Static Timing Analyzer
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

#include <mutex>
#include <atomic>
#include <unordered_set>

#include "MinMax.hh"
#include "Transition.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "Delay.hh"
#include "SdcClass.hh"
#include "StaState.hh"
#include "SearchClass.hh"
#include "SearchPred.hh"
#include "VertexVisitor.hh"
#include "Path.hh"

namespace sta {

class BfsFwdIterator;
class BfsBkwdIterator;
class SearchPred;
class SearchThru;
class SearchAdj;
class ClkInfoLess;
class PathEndVisitor;
class ArrivalVisitor;
class RequiredVisitor;
class ClkPathIterator;
class EvalPred;
class TagGroup;
class TagGroupBldr;
class WorstSlacks;
class VisitPathEnds;
class GatedClk;
class CheckCrpr;
class Scene;

using ClkInfoSet = std::set<const ClkInfo*, ClkInfoLess>;
using TagSet = std::unordered_set<Tag*, TagHash, TagEqual>;
using TagGroupSet = std::unordered_set<TagGroup*, TagGroupHash, TagGroupEqual>;
using VertexSlackMap = std::map<Vertex*, Slack>;
using VertexSlackMapSeq = std::vector<VertexSlackMap>;
using WorstSlacksSeq = std::vector<WorstSlacks>;
using DelayDblSeq = std::vector<DelayDbl>;
using ExceptionPathSeq = std::vector<ExceptionPath*>;
using StdStringSeq = std::vector<std::string>;

class Search : public StaState
{
public:
  Search(StaState *sta);
  virtual ~Search();
  virtual void copyState(const StaState *sta);
  // Reset to virgin state.
  void clear();
  // When enabled, non-critical path arrivals are pruned to improve
  // run time and reduce memory.
  [[nodiscard]] bool crprPathPruningEnabled() const;
  void setCrprpathPruningEnabled(bool enabled);
  // When path pruning is enabled required times for non-critical paths
  // that have been pruned require additional search. This option
  // disables additional search to returns approximate required times.
  [[nodiscard]] bool crprApproxMissingRequireds() const;
  void setCrprApproxMissingRequireds(bool enabled);

  [[nodiscard]] bool unconstrainedPaths() const { return unconstrained_paths_; }
  // from/thrus/to are owned and deleted by Search.
  // Use scene nullptr to report timing for all scenes.
  // PathEnds are owned by Mode PathGroups and deleted on next call.
  PathEndSeq findPathEnds(ExceptionFrom *from,
                          ExceptionThruSeq *thrus,
                          ExceptionTo *to,
                          bool unconstrained,
                          const SceneSeq &scenes,
                          const MinMaxAll *min_max,
                          size_t group_path_count,
                          size_t endpoint_path_count,
                          bool unique_pins,
                          bool unique_edges,
                          float slack_min,
                          float slack_max,
                          bool sort_by_slack,
                          StdStringSeq &group_names,
                          bool setup,
                          bool hold,
                          bool recovery,
                          bool removal,
                          bool clk_gating_setup,
                          bool clk_gating_hold);
  [[nodiscard]] bool arrivalsValid();
  // Invalidate all arrival and required times.
  void arrivalsInvalid();
  // Invalidate vertex arrival time.
  void arrivalInvalid(Vertex *vertex);
  void arrivalInvalid(const Pin *pin);
  // Invalidate all required times.
  void requiredsInvalid();
  // Invalidate vertex required time.
  void requiredInvalid(Vertex *vertex);
  void requiredInvalid(const Instance *inst);
  void requiredInvalid(const Pin *pin);
  // Vertex will be deleted.
  void deleteVertexBefore(Vertex *vertex);
  void deleteEdgeBefore(Edge *edge);
  // Find all arrival times (propatating thru latches).
  void findAllArrivals();
  // Find all arrivals (without latch propagation).
  void findArrivals();
  // Find arrival times up thru level.
  void findArrivals(Level level);
  void findRequireds();
  // Find required times down thru level.
  void findRequireds(Level level);
  [[nodiscard]] bool requiredsSeeded() const { return requireds_seeded_; }
  [[nodiscard]] bool requiredsExist() const { return requireds_exist_; }
  // The sum of all negative endpoints slacks.
  // Incrementally updated.
  Slack totalNegativeSlack(const MinMax *min_max);
  Slack totalNegativeSlack(const Scene *scene,
                           const MinMax *min_max);
  // Worst endpoint slack and vertex.
  // Incrementally updated.
  void worstSlack(const MinMax *min_max,
                  // Return values.
                  Slack &worst_slack,
                  Vertex *&worst_vertex);
  void worstSlack(const Scene *scene,
                  const MinMax *min_max,
                  // Return values.
                  Slack &worst_slack,
                  Vertex *&worst_vertex);
  // Clock arrival respecting ideal clock insertion delay and latency.
  Arrival clkPathArrival(const Path *clk_path) const;
  Arrival clkPathArrival(const Path *clk_path,
                         const ClkInfo *clk_info,
                         const ClockEdge *clk_edge,
                         const MinMax *min_max) const;
  // Clock arrival at the path source/launch point.
  Arrival pathClkPathArrival(const Path *path) const;

  void deletePathGroups();
  ExceptionPath *exceptionTo(ExceptionPathType type,
                             const Path *path,
                             const Pin *pin,
                             const RiseFall *rf,
                             const ClockEdge *clk_edge,
                             const MinMax *min_max,
                             bool match_min_max_exactly,
                             bool require_to_pin,
                             Sdc *sdc) const;
  ExceptionPathSeq groupPathsTo(const PathEnd *path_end) const;
  void deleteFilter();
  void deleteFilteredArrivals();

  VertexSet &endpoints();
  void endpointsInvalid();

  // The set of clocks that arrive at vertex in the clock network.
  ClockSet clocks(const Pin *pin,
                  const Mode *mode) const;
  ClockSet clocks(const Vertex *vertex,
                  const Mode *mode) const;
  // Clock domains for a vertex.
  ClockSet clockDomains(const Vertex *vertex,
                        const Mode *mode) const;
  ClockSet clockDomains(const Pin *pin,
                        const Mode *mode) const;

  ////////////////////////////////////////////////////////////////
  //
  // Somewhat protected functions.
  //
  ////////////////////////////////////////////////////////////////

  // Find arrivals for the clock tree.
  void findClkArrivals();
  EvalPred *evalPred() const { return eval_pred_; }
  SearchPred *searchAdj() const { return search_thru_; }
  Tag *tag(TagIndex index) const;
  TagIndex tagCount() const;
  TagGroupIndex tagGroupCount() const;
  void reportTagGroups() const;
  void reportPathCountHistogram() const;
  int clkInfoCount() const;
  // Endpoint for any mode.
  [[nodiscard]] bool isEndpoint(Vertex *vertex) const;
  // Endpoint for one mode.
  [[nodiscard]] bool isEndpoint(Vertex *vertex,
                                 const Mode *mode) const;
  [[nodiscard]] bool isEndpoint(Vertex *vertex,
                                 const ModeSeq &modes) const;
  [[nodiscard]] bool isEndpoint(Vertex *vertex,
                                 SearchPred *pred,
                  const Mode *mode) const;
  void endpointInvalid(Vertex *vertex);
  Tag *fromUnclkedInputTag(const Pin *pin,
                           const RiseFall *rf,
                           const MinMax *min_max,
                           bool is_segment_start,
                           bool require_exception,
                           Scene *scene);
  Tag *fromRegClkTag(const Pin *from_pin,
                     const RiseFall *from_rf,
                     const Clock *clk,
                     const RiseFall *clk_rf,
                     const ClkInfo *clk_info,
                     const Pin *to_pin,
                     const RiseFall *to_rf,
                     const MinMax *min_max,
                     Scene *scene);
  Tag *thruTag(Tag *from_tag,
               Edge *edge,
               const RiseFall *to_rf,
               TagSet *tag_cache);
  Tag *thruClkTag(Path *from_path,
                  Vertex *from_vertex,
                  Tag *from_tag,
                  bool to_propagates_clk,
                  Edge *edge,
                  const RiseFall *to_rf,
                  bool arc_delay_min_max_eq,
                  const MinMax *min_max,
                  Scene *scene);
  const ClkInfo *thruClkInfo(Path *from_path,
                             Vertex *from_vertex,
                             const ClkInfo *from_clk_info,
                             bool from_is_clk,
                             Edge *edge,
                             Vertex *to_vertex,
                             const Pin *to_pin,
                             bool to_is_clk,
                             bool arc_delay_min_max_eq,
                             const MinMax *min_max,
                             Scene *scene);
  const ClkInfo *clkInfoWithCrprClkPath(const ClkInfo *from_clk_info,
                                        Path *from_path);
  void seedClkArrivals(const Pin *pin,
                       const Mode *mode,
                       TagGroupBldr *tag_bldr);
  void setVertexArrivals(Vertex *vertex,
                         TagGroupBldr *group_bldr);
  void tnsInvalid(Vertex *vertex);
  [[nodiscard]] bool arrivalsChanged(Vertex *vertex,
                                     TagGroupBldr *tag_bldr);
  BfsFwdIterator *arrivalIterator() const { return arrival_iter_; }
  BfsBkwdIterator *requiredIterator() const { return required_iter_; }
  // Used by OpenROAD.
  bool makeUnclkedPaths(Vertex *vertex,
                        bool is_segment_start,
                        bool require_exception,
                        TagGroupBldr *tag_bldr,
                        const Mode *mode);
  bool makeUnclkedPaths2(Vertex *vertex,
                         TagGroupBldr *tag_bldr);
  [[nodiscard]] bool isInputArrivalSrchStart(Vertex *vertex);
  void seedInputSegmentArrival(const Pin *pin,
                               Vertex *vertex,
                               const Mode *mode,
                               TagGroupBldr *tag_bldr);
  void enqueueLatchDataOutputs(Vertex *vertex);
  void enqueueLatchOutput(Vertex *vertex);
  void enqueuePendingClkFanouts();
  void postponeClkFanouts(Vertex *vertex);
  void seedRequired(Vertex *vertex);
  void seedRequiredEnqueueFanin(Vertex *vertex);
  void seedInputDelayArrival(const Pin *pin,
                             Vertex *vertex,
                             InputDelay *input_delay,
                             const Mode *mode);
  void seedInputDelayArrival(const Pin *pin,
                             Vertex *vertex,
                             InputDelay *input_delay,
                             bool is_segment_start,
                             const Mode *mode,
                             TagGroupBldr *tag_bldr);
  // Insertion delay for regular or generated clock.
  Arrival clockInsertion(const Clock *clk,
                         const Pin *pin,
                         const RiseFall *rf,
                         const MinMax *min_max,
                         const EarlyLate *early_late,
                         const Mode *mode) const;
  [[nodiscard]] bool propagateClkSense(const Pin *from_pin,
                                        Path *from_path,
                                        const RiseFall *to_rf);

  Tag *findTag(Scene *scene,
               const RiseFall *rf,
               const MinMax *min_max,
               const ClkInfo *tag_clk,
               bool is_clk,
               InputDelay *input_delay,
               bool is_segment_start,
               ExceptionStateSet *states,
               bool own_states,
               TagSet *tag_cache);
  void reportTags() const;
  void reportClkInfos() const;
  const ClkInfo *findClkInfo(Scene *scene,
                             const ClockEdge *clk_edge,
                             const Pin *clk_src,
                             bool is_propagated,
                             const Pin *gen_clk_src,
                             bool gen_clk_src_path,
                             const RiseFall *pulse_clk_sense,
                             Arrival insertion,
                             float latency,
                             const ClockUncertainties *uncertainties,
                             const MinMax *min_max,
                             Path *crpr_clk_path);
  const ClkInfo *findClkInfo(Scene *scene,
                             const ClockEdge *clk_edge,
                             const Pin *clk_src,
                             bool is_propagated,
                             Arrival insertion,
                             const MinMax *min_max);
  // Timing derated arc delay for a path analysis point.
  ArcDelay deratedDelay(const Vertex *from_vertex,
                        const TimingArc *arc,
                        const Edge *edge,
                        bool is_clk,
                        const MinMax *min_max,
                        DcalcAPIndex dcalc_ap,
                        const Sdc *sdc);

  TagGroup *tagGroup(const Vertex *vertex) const;
  TagGroup *tagGroup(TagGroupIndex index) const;
  void reportArrivals(Vertex *vertex,
                      bool report_tag_index) const;
  Slack wnsSlack(Vertex *vertex,
                 PathAPIndex path_ap_index);
  void levelsChangedBefore();
  void levelChangedBefore(Vertex *vertex);
  void seedInputArrival(const Pin *pin,
                        Vertex *vertex,
                        const Mode *mode,
                        TagGroupBldr *tag_bldr);
  void ensureDownstreamClkPins();
  [[nodiscard]] bool matchesFilter(Path *path,
                                   const ClockEdge *to_clk_edge);
  CheckCrpr *checkCrpr() { return check_crpr_; }
  VisitPathEnds *visitPathEnds() { return visit_path_ends_; }
  GatedClk *gatedClk() { return gated_clk_; }
  void findClkVertexPins(PinSet &clk_pins);
  void findFilteredArrivals(ExceptionFrom *from,
                            ExceptionThruSeq *thrus,
                            ExceptionTo *to,
                            bool unconstrained,
                            bool thru_latches);
  VertexSeq filteredEndpoints();

  Arrival *arrivals(const Vertex *vertex) const;
  Arrival *makeArrivals(const Vertex *vertex,
                        uint32_t count);
  void deleteArrivals(const Vertex *vertex);
  Required *requireds(const Vertex *vertex) const;
  [[nodiscard]] bool hasRequireds(const Vertex *vertex) const;
  Required *makeRequireds(const Vertex *vertex,
                          uint32_t count);
  void deleteRequireds(const Vertex *vertex);
  size_t arrivalCount() const;
  size_t requiredCount() const;
  Path *prevPaths(const Vertex *vertex) const;
  Path *makePrevPaths(const Vertex *vertex,
                      uint32_t count);
  void deletePrevPaths(Vertex *vertex);
  [[nodiscard]] bool crprPathPruningDisabled(const Vertex *vertex) const;
  void setCrprPathPruningDisabled(const Vertex *vertex,
                                  bool disabled);
  [[nodiscard]] bool bfsInQueue(const Vertex *vertex,
                                 BfsIndex index) const;
  void setBfsInQueue(const Vertex *vertex,
                     BfsIndex index,
                     bool value);
  TagGroupIndex tagGroupIndex(const Vertex *vertex) const;
  void setTagGroupIndex(const Vertex *vertex,
                        TagGroupIndex tag_index);
  void checkPrevPaths() const;
  void deletePaths(Vertex *vertex);
  void deleteTagGroup(TagGroup *group);
  bool postponeLatchOutputs() const { return postpone_latch_outputs_; }
  void saveEnumPath(Path *path);
  bool isSrchRoot(Vertex *vertex,
                  const Mode *mode) const;

protected:
  void initVars();
  void deleteTags();
  void deleteTagsPrev();
  void deleteUnusedTagGroups();
  void seedInvalidArrivals();
  void seedArrivals();
  void findClockVertices(VertexSet &vertices);
  void seedClkDataArrival(const Pin *pin,
                          const RiseFall *rf,
                          const Clock *clk,
                          const ClockEdge *clk_edge,
                          const MinMax *min_max,
                          Arrival insertion,
                          Scene *scene,
                          TagGroupBldr *tag_bldr);
  void seedClkArrival(const Pin *pin,
                      const RiseFall *rf,
                      const Clock *clk,
                      const ClockEdge *clk_edge,
                      const MinMax *min_max,
                      Arrival insertion,
                      Scene *scene,
                      TagGroupBldr *tag_bldr);
  Tag *clkDataTag(const Pin *pin,
                  const Clock *clk,
                  const RiseFall *rf,
                  const ClockEdge *clk_edge,
                  Arrival insertion,
                  const MinMax *min_max,
                  Scene *scene);
  void findInputArrivalVertices(VertexSet &vertices);
  void findRootVertices(VertexSet &vertices);
  void findInputDrvrVertices(VertexSet &vertices);
  void seedInputArrival1(const Pin *pin,
                         Vertex *vertex,
                         bool is_segment_start,
                         const Mode *mode,
                         TagGroupBldr *tag_bldr);
  void seedInputArrival(const Pin *pin,
                        Vertex *vertex,
                        ClockSet *wrt_clks);
  void seedInputDelayArrival(const Pin *pin,
                             InputDelay *input_delay,
                             const ClockEdge *clk_edge,
                             float clk_arrival,
                             float clk_insertion,
                             float clk_latency,
                             bool is_segment_start,
                             const MinMax *min_max,
                             Scene *scene,
                             TagGroupBldr *tag_bldr);
  void seedInputDelayArrival(const Pin *pin,
                             const RiseFall *rf,
                             float arrival,
                             InputDelay *input_delay,
                             const ClockEdge *clk_edge,
                             float clk_insertion,
                             float clk_latency,
                             bool is_segment_start,
                             const MinMax *min_max,
                             Scene *scene,
                             TagGroupBldr *tag_bldr);
  void inputDelayClkArrival(InputDelay *input_delay,
                            const ClockEdge *clk_edge,
                            const MinMax *min_max,
                            const Mode *mode,
                            // Return values.
                            float &clk_arrival,
                            float &clk_insertion,
                            float &clk_latency);
  void inputDelayRefPinArrival(Path *ref_path,
                               const ClockEdge *clk_edge,
                               const MinMax *min_max,
                               const Sdc *sdc,
                               // Return values.
                               float &ref_arrival,
                               float &ref_insertion,
                               float &ref_latency);
  Tag *inputDelayTag(const Pin *pin,
                     const RiseFall *rf,
                     const ClockEdge *clk_edge,
                     float clk_insertion,
                     float clk_latency,
                     InputDelay *input_delay,
                     bool is_segment_start,
                     const MinMax *min_max,
                     Scene *scene);
  void seedClkVertexArrivals();
  void findClkArrivals1();

  void findAllArrivals(bool thru_latches,
                       bool clks_only);
  void findArrivals1(Level level);
  Tag *mutateTag(Tag *from_tag,
                 const Pin *from_pin,
                 const RiseFall *from_rf,
                 bool from_is_clk,
                 const ClkInfo *from_clk_info,
                 const Pin *to_pin,
                 const RiseFall *to_rf,
                 bool to_is_clk,
                 bool to_is_reg_clk,
                 bool to_is_segment_start,
                 const ClkInfo *to_clk_info,
                 InputDelay *to_input_delay,
                 TagSet *tag_cache);
  ExceptionPath *exceptionTo(const Path *path,
                             const Pin *pin,
                             const RiseFall *rf,
                             const ClockEdge *clk_edge,
                             const MinMax *min_max) const;
  void seedRequireds();
  void seedInvalidRequireds();
  [[nodiscard]] bool havePendingLatchOutputs();
  void clearPendingLatchOutputs();
  void enqueuePendingLatchOutputs();
  void findFilteredArrivals(bool thru_latches);
  void findArrivalsSeed();
  void seedFilterStarts();
  [[nodiscard]] bool hasEnabledChecks(Vertex *vertex,
                                      const Mode *mode) const;
  float timingDerate(const Vertex *from_vertex,
                     const TimingArc *arc,
                     const Edge *edge,
                     bool is_clk,
                     const Sdc *sdc,
                     const MinMax *min_max);
  void deletePaths();
  // Delete with incremental tns/wns update.
  void deletePathsIncr(Vertex *vertex);
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
  [[nodiscard]] bool matchesFilterTo(Path *path,
                                     const ClockEdge *to_clk_edge) const;
  const Path *pathClkPathArrival1(const Path *path) const;
  void deletePathsState(const Vertex *vertex) const;
  void clocks(const Vertex *vertex,
              const Mode *mode,
              // Return value.
              ClockSet &clks) const;
  void clockDomains(const Vertex *vertex,
                    const Mode *mode,
                    // Return value.
                    ClockSet &clks) const;

  ////////////////////////////////////////////////////////////////

  // findPathEnds arg.
  bool unconstrained_paths_;
  bool crpr_path_pruning_enabled_;
  bool crpr_approx_missing_requireds_;

  // Search predicates.
  SearchPred *search_thru_;
  SearchAdj *search_adj_;
  EvalPred *eval_pred_;

  // Some arrivals exist.
  bool arrivals_exist_;
  // Arrivals at start points have been initialized.
  bool arrivals_seeded_;
  // Vertices with invalid arrival times to update and search from.
  VertexSet invalid_arrivals_;
  std::mutex invalid_arrivals_lock_;
  BfsFwdIterator *arrival_iter_;
  ArrivalVisitor *arrival_visitor_;

  // Some requireds exist.
  bool requireds_exist_;
  // Requireds have been seeded by searching arrivals to all endpoints.
  bool requireds_seeded_;
  // Vertices with invalid required times to update and search from.
  VertexSet invalid_requireds_;
  BfsBkwdIterator *required_iter_;

  bool tns_exists_;
  // Endpoint vertices with slacks that have changed since tns was found.
  VertexSet invalid_tns_;
  // Indexed by path_ap->index().
  DelayDblSeq tns_;
  // Indexed by path_ap->index().
  VertexSlackMapSeq tns_slacks_;
  std::mutex tns_lock_;

  // Indexed by path_ap->index().
  WorstSlacks *worst_slacks_;

  // Use pointer to clk_info set so Tag.hh does not need to be included.
  ClkInfoSet *clk_info_set_;
  std::mutex clk_info_lock_;

  // Entries in tags_ may be missing where previous filter tags were deleted.
  TagIndex tag_capacity_;
  std::atomic<Tag **> tags_;
  // Use pointer to tag set so Tag.hh does not need to be included.
  TagSet *tag_set_;
  std::vector<Tag **> tags_prev_;
  TagIndex tag_next_;
  std::mutex tag_lock_;

  // Capacity of tag_groups_.
  TagGroupIndex tag_group_capacity_;
  std::atomic<TagGroup **> tag_groups_;
  TagGroupSet *tag_group_set_;
  std::vector<TagGroup **> tag_groups_prev_;
  TagGroupIndex tag_group_next_;
  // Holes in tag_groups_ left by deleting filter tag groups.
  std::vector<TagIndex> tag_group_free_indices_;
  std::mutex tag_group_lock_;

  // Latches data outputs to queue on the next search pass.
  VertexSet pending_latch_outputs_;
  std::mutex pending_latch_outputs_lock_;
  // Clock network endpoints where arrival search was suppended by findClkArrivals().
  VertexSet pending_clk_endpoints_;
  std::mutex pending_clk_endpoints_lock_;

  VertexSet endpoints_;
  bool endpoints_initialized_;
  VertexSet invalid_endpoints_;

  bool have_filter_;
  ExceptionFrom *filter_from_;
  ExceptionThruSeq *filter_thrus_;
  ExceptionTo *filter_to_;
  VertexSet filtered_arrivals_;
  std::mutex filtered_arrivals_lock_;

  bool found_downstream_clk_pins_;
  bool postpone_latch_outputs_;
  std::vector<Path*> enum_paths_;

  VisitPathEnds *visit_path_ends_;
  GatedClk *gated_clk_;
  CheckCrpr *check_crpr_;
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
  EvalPred(const StaState *sta);
  bool searchThru(Edge *edge,
                  const Mode *mode) const override;
  void setSearchThruLatches(bool thru_latches);
  bool searchTo(const Vertex *to_vertex,
                const Mode *mode) const override;

  using SearchPred::searchFrom;
  using SearchPred::searchThru;
  using SearchPred::searchTo;

protected:
  bool search_thru_latches_;
};

// Class for visiting fanin/fanout paths of a vertex.
// This used by forward/backward search to find arrival/required path times.
class PathVisitor : public VertexVisitor, public StaState
{
public:
  // Uses search->evalPred() for search predicate.
  PathVisitor(const StaState *sta);
  PathVisitor(SearchPred *pred,
              bool make_tag_cache,
              const StaState *sta);
  virtual ~PathVisitor();
  virtual void visitFaninPaths(Vertex *to_vertex);
  virtual void visitFanoutPaths(Vertex *from_vertex);

protected:
  // Return false to stop visiting.
  virtual bool visitEdge(const Pin *from_pin,
                         Vertex *from_vertex,
                         Edge *edge,
                         const Pin *to_pin,
                         Vertex *to_vertex);
  // Return false to stop visiting.
  [[nodiscard]] bool visitArc(const Pin *from_pin,
                               Vertex *from_vertex,
                               const RiseFall *from_rf,
                               Path *from_path,
                               Edge *edge,
                               TimingArc *arc,
                const Pin *to_pin,
                Vertex *to_vertex,
                const MinMax *min_max,
                const Mode *mode);
  // This calls visit below with everything required to make to_path.
  // Return false to stop visiting.
  virtual bool visitFromPath(const Pin *from_pin,
                             Vertex *from_vertex,
                             const RiseFall *from_rf,
                             Path *from_path,
                             Edge *edge,
                             TimingArc *arc,
                             const Pin *to_pin,
                             Vertex *to_vertex,
                             const RiseFall *to_rf,
                             const MinMax *min_max);
  // Return false to stop visiting.
  virtual bool visitFromToPath(const Pin *from_pin,
                               Vertex *from_vertex,
                               const RiseFall *from_rf,
                               Tag *from_tag,
                               Path *from_path,
                               const Arrival &from_arrival,
                               Edge *edge,
                               TimingArc *arc,
                               ArcDelay arc_delay,
                               Vertex *to_vertex,
                               const RiseFall *to_rf,
                               Tag *to_tag,
                               Arrival &to_arrival,
                               const MinMax *min_max) = 0;

  SearchPred *pred_;
  TagSet *tag_cache_;
};

// Visitor called during forward search to record an
// arrival at an path.
class ArrivalVisitor : public PathVisitor
{
public:
  ArrivalVisitor(const StaState *sta);
  virtual ~ArrivalVisitor();
  // Initialize the visitor.
  void init(bool always_to_endpoints,
            bool clks_only,
            SearchPred *pred);
  void copyState(const StaState *sta);
  virtual void visit(Vertex *vertex);
  virtual VertexVisitor *copy() const;
  // Return false to stop visiting.
  virtual bool visitFromToPath(const Pin *from_pin,
                               Vertex *from_vertex,
                               const RiseFall *from_rf,
                               Tag *from_tag,
                               Path *from_path,
                               const Arrival &from_arrival,
                               Edge *edge,
                               TimingArc *arc,
                               ArcDelay arc_delay,
                               Vertex *to_vertex,
                               const RiseFall *to_rf,
                               Tag *to_tag,
                               Arrival &to_arrival,
                               const MinMax *min_max);
  void setAlwaysToEndpoints(bool to_endpoints);
  TagGroupBldr *tagBldr() const { return tag_bldr_; }

protected:
  ArrivalVisitor(bool always_to_endpoints,
                 SearchPred *pred,
                 const StaState *sta);
  void init0();
  void enqueueRefPinInputDelays(const Pin *ref_pin,
                                const Sdc *sdc);
  void seedArrivals(Vertex *vertex);
  void pruneCrprArrivals();
  void constrainedRequiredsInvalid(Vertex *vertex,
                                   bool is_clk);
  bool always_to_endpoints_;
  bool always_save_prev_paths_;
  bool clks_only_;
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
  void requiredSet(size_t path_index,
                   Required &required,
                   const MinMax *min_max,
                   const StaState *sta);
  // Return true if the requireds changed.
  bool requiredsSave(Vertex *vertex,
                     const StaState *sta);
  Required required(size_t path_index);

protected:
  ArrivalSeq requireds_;
  bool have_requireds_;
};

// Visitor called during backward search to record a
// required time at an path.
class RequiredVisitor : public PathVisitor
{
public:
  RequiredVisitor(const StaState *sta);
  virtual ~RequiredVisitor();
  virtual VertexVisitor *copy() const;
  virtual void visit(Vertex *vertex);

protected:
  RequiredVisitor(bool make_tag_cache,
                  const StaState *sta);
  // Return false to stop visiting.
  virtual bool visitFromToPath(const Pin *from_pin,
                               Vertex *from_vertex,
                               const RiseFall *from_rf,
                               Tag *from_tag,
                               Path *from_path,
                               const Arrival &from_arrival,
                               Edge *edge,
                               TimingArc *arc,
                               ArcDelay arc_delay,
                               Vertex *to_vertex,
                               const RiseFall *to_rf,
                               Tag *to_tag,
                               Arrival &to_arrival,
                               const MinMax *min_max);

  RequiredCmp *required_cmp_;
  VisitPathEnds *visit_path_ends_;
};

} // namespace
