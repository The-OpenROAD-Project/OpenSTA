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

#include "Search.hh"

#include <algorithm>
#include <cmath> // abs

#include "ContainerHelpers.hh"
#include "Mutex.hh"
#include "Report.hh" 
#include "Debug.hh"
#include "Stats.hh"
#include "Fuzzy.hh"
#include "TimingRole.hh"
#include "FuncExpr.hh"
#include "TimingArc.hh"
#include "Sequential.hh"
#include "Units.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Graph.hh"
#include "GraphCmp.hh"
#include "PortDelay.hh"
#include "Clock.hh"
#include "CycleAccting.hh"
#include "ExceptionPath.hh"
#include "DataCheck.hh"
#include "Sdc.hh"
#include "Mode.hh"
#include "SearchPred.hh"
#include "Levelize.hh"
#include "Bfs.hh"
#include "Sim.hh"
#include "Path.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "PathEnd.hh"
#include "PathGroup.hh"
#include "VisitPathEnds.hh"
#include "GatedClk.hh"
#include "WorstSlack.hh"
#include "Latches.hh"
#include "Crpr.hh"
#include "Genclks.hh"
#include "Variables.hh"

namespace sta {

using std::min;
using std::max;
using std::abs;

////////////////////////////////////////////////////////////////

EvalPred::EvalPred(const StaState *sta) :
  SearchPred0(sta),
  search_thru_latches_(true)
{
}

void
EvalPred::setSearchThruLatches(bool thru_latches)
{
  search_thru_latches_ = thru_latches;
}

bool
EvalPred::searchThru(Edge *edge,
                     const Mode *mode) const
{
  const TimingRole *role = edge->role();
  return SearchPred0::searchThru(edge, mode)
    && (sta_->variables()->dynamicLoopBreaking()
        || !edge->isDisabledLoop())
    && !role->isTimingCheck()
    && (search_thru_latches_
        || role->isLatchDtoQ()
        || sta_->latches()->latchDtoQState(edge, mode) == LatchEnableState::open);
}

bool
EvalPred::searchTo(const Vertex *to_vertex,
                   const Mode *mode) const
{
  const Pin *to_pin = to_vertex->pin();
  const Sdc *sdc = mode->sdc();
  return SearchPred0::searchTo(to_vertex, mode)
    && !(sdc->isLeafPinClock(to_pin)
         && !sdc->isPathDelayInternalTo(to_pin))
    // Fanin paths are broken by path delay internal pin startpoints.
    && !sdc->isPathDelayInternalFromBreak(to_pin);
}

////////////////////////////////////////////////////////////////

// EvalPred unless
//  latch D->Q edge
class SearchThru : public EvalPred
{
public:
  SearchThru(const StaState *sta);
  bool searchThru(Edge *edge,
                  const Mode *mode) const override;
};

SearchThru::SearchThru(const StaState *sta) :
  EvalPred(sta)
{
}

bool
SearchThru::searchThru(Edge *edge,
                       const Mode *mode) const
{
  return EvalPred::searchThru(edge, mode)
    && !edge->role()->isLatchDtoQ();
}

////////////////////////////////////////////////////////////////

// SearchAdj is mode independent. Search unless
//  disabled to break combinational loop
//  latch D->Q edge
//  timing check edge
//  dynamic loop breaking pending tags
class SearchAdj : public SearchPred
{
public:
  SearchAdj(TagGroupBldr *tag_bldr,
            const StaState *sta);
  bool searchFrom(const Vertex *from_vertex,
                  const Mode *mode) const override;
  bool searchThru(Edge *edge,
                  const Mode *mode) const override;
  bool searchTo(const Vertex *to_vertex,
                const Mode *mode) const override;

protected:
  bool loopEnabled(Edge *edge) const;
  bool hasPendingLoopPaths(Edge *edge) const;

  TagGroupBldr *tag_bldr_;
  const StaState *sta_;

};

SearchAdj::SearchAdj(TagGroupBldr *tag_bldr,
                     const StaState *sta) :
  SearchPred(sta),
  tag_bldr_(tag_bldr),
  sta_(sta)
{
}

bool
SearchAdj::searchFrom(const Vertex * /* from_vertex */,
                      const Mode *) const
{
  return true;
}

bool
SearchAdj::searchThru(Edge *edge,
                      const Mode *) const
{
  const TimingRole *role = edge->role();
  const Variables *variables = sta_->variables();
  return !role->isTimingCheck()
    && !role->isLatchDtoQ()
    // Register/latch preset/clr edges are disabled by default.
    && !(role == TimingRole::regSetClr()
         && !variables->presetClrArcsEnabled())
    && !(edge->isBidirectInstPath()
         && !variables->bidirectInstPathsEnabled())
    && (!edge->isDisabledLoop()
        || (variables->dynamicLoopBreaking()
            && hasPendingLoopPaths(edge)));
}

bool
SearchAdj::loopEnabled(Edge *edge) const
{
  return !edge->isDisabledLoop()
    || (sta_->variables()->dynamicLoopBreaking()
        && hasPendingLoopPaths(edge));
}

bool
SearchAdj::hasPendingLoopPaths(Edge *edge) const
{
  if (tag_bldr_
      && tag_bldr_->hasLoopTag()) {
    const Graph *graph = sta_->graph();
    Search *search = sta_->search();
    Vertex *from_vertex = edge->from(graph);
    TagGroup *prev_tag_group = search->tagGroup(from_vertex);
    for (auto const [from_tag, path_index] : tag_bldr_->pathIndexMap()) {
      if (from_tag->isLoop()) {
        // Loop false path exceptions apply to rise/fall edges so to_rf
        // does not matter.
        Tag *to_tag = search->thruTag(from_tag, edge, RiseFall::rise(), nullptr);
        if (to_tag
            && (prev_tag_group == nullptr
                || !prev_tag_group->hasTag(from_tag)))
          return true;
      }
    }
  }
  return false;
}

bool
SearchAdj::searchTo(const Vertex * /* to_vertex */,
                    const Mode *) const
{
  return true;
}

////////////////////////////////////////////////////////////////

Search::Search(StaState *sta) :
  StaState(sta),
  unconstrained_paths_(false),
  crpr_path_pruning_enabled_(true),
  crpr_approx_missing_requireds_(true),

  search_thru_(new SearchThru(this)),
  search_adj_(new SearchAdj(nullptr, this)),
  eval_pred_(new EvalPred(this)),
  
  arrivals_exist_(false),
  arrivals_seeded_(false),
  invalid_arrivals_(makeVertexSet(this)),
  arrival_iter_(new BfsFwdIterator(BfsIndex::arrival, nullptr, this)),
  arrival_visitor_(new ArrivalVisitor(this)),

  requireds_exist_(false),
  requireds_seeded_(false),
  invalid_requireds_(makeVertexSet(this)),
  required_iter_(new BfsBkwdIterator(BfsIndex::required, search_adj_, this)),

  tns_exists_(false),
  invalid_tns_(makeVertexSet(this)),
  worst_slacks_(nullptr),
  clk_info_set_(new ClkInfoSet(ClkInfoLess(this))),

  tag_capacity_(128),
  tags_(new Tag*[tag_capacity_]),
  tag_set_(new TagSet(tag_capacity_, TagHash(this), TagEqual(this))),
  tag_next_(0),

  tag_group_capacity_(tag_capacity_),
  tag_groups_(new TagGroup*[tag_group_capacity_]),
  tag_group_set_(new TagGroupSet(tag_group_capacity_)),
  tag_group_next_(0),

  pending_latch_outputs_(makeVertexSet(this)),
  pending_clk_endpoints_(makeVertexSet(this)),
  endpoints_(makeVertexSet(this)),
  endpoints_initialized_(false),
  invalid_endpoints_(makeVertexSet(this)),

  have_filter_(false),
  filter_from_(nullptr),
  filter_thrus_(nullptr),
  filter_to_(nullptr),
  filtered_arrivals_(makeVertexSet(this)),

  found_downstream_clk_pins_(false),
  postpone_latch_outputs_(false),

  visit_path_ends_(new VisitPathEnds(this)),
  gated_clk_(new GatedClk(this)),
  check_crpr_(new CheckCrpr(this))
{
  initVars();
}

// Init "options".
void
Search::initVars()
{
  unconstrained_paths_ = false;
  crpr_path_pruning_enabled_ = true;
  crpr_approx_missing_requireds_ = true;
}

Search::~Search()
{
  deletePathGroups();
  deletePaths();
  deleteTags();
  delete tag_set_;
  delete clk_info_set_;
  delete [] tags_;
  delete [] tag_groups_;
  delete tag_group_set_;
  delete search_thru_;
  delete search_adj_;
  delete eval_pred_;
  delete arrival_visitor_;
  delete arrival_iter_;
  delete required_iter_;
  delete visit_path_ends_;
  delete gated_clk_;
  delete worst_slacks_;
  delete check_crpr_;
  deleteFilter();
}

void
Search::clear()
{
  initVars();

  arrivals_seeded_ = false;
  requireds_exist_ = false;
  requireds_seeded_ = false;
  tns_exists_ = false;
  clearWorstSlack();
  invalid_arrivals_.clear();
  arrival_iter_->clear();
  invalid_requireds_.clear();
  invalid_tns_.clear();
  required_iter_->clear();
  endpointsInvalid();
  deletePathGroups();
  deletePaths();
  deleteTags();
  clearPendingLatchOutputs();
  pending_clk_endpoints_.clear();
  deleteFilter();
  found_downstream_clk_pins_ = false;
}

void
Search::deletePathGroups()
{
  for (Mode *mode : modes_)
    mode->deletePathGroups();
}

bool
Search::crprPathPruningEnabled() const
{
  return crpr_path_pruning_enabled_;
}

void
Search::setCrprpathPruningEnabled(bool enabled)
{
  crpr_path_pruning_enabled_ = enabled;
}

bool
Search::crprApproxMissingRequireds() const
{
  return crpr_approx_missing_requireds_;
}

void
Search::setCrprApproxMissingRequireds(bool enabled)
{
  crpr_approx_missing_requireds_ = enabled;
}

void
Search::deleteTags()
{
  for (TagGroupIndex i = 0; i < tag_group_next_; i++) {
    TagGroup *group = tag_groups_[i];
    delete group;
  }
  tag_group_next_ = 0;
  tag_group_set_->clear();
  tag_group_free_indices_.clear();

  tag_next_ = 0;
  deleteContents(tag_set_);

  deleteContents(clk_info_set_);
  deleteTagsPrev();
}

void
Search::deleteFilter()
{
  if (have_filter_) {
    for (const Mode *mode : modes_)
      mode->sdc()->deleteFilter();
    have_filter_ = false;
  }
  delete filter_from_;
  filter_from_ = nullptr;

  if (filter_thrus_) {
    deleteContents(*filter_thrus_);
    delete filter_thrus_;
    filter_thrus_ = nullptr;
  }

  delete filter_to_;
  filter_to_ = nullptr;
}

void
Search::copyState(const StaState *sta)
{
  StaState::copyState(sta);
  // Notify sub-components.
  arrival_iter_->copyState(sta);
  required_iter_->copyState(sta);
  arrival_visitor_->copyState(sta);
  eval_pred_->copyState(sta);
  search_thru_->copyState(sta);
  search_adj_->copyState(sta);
  visit_path_ends_->copyState(sta);
  gated_clk_->copyState(sta);
  check_crpr_->copyState(sta);
}

////////////////////////////////////////////////////////////////

void
Search::deletePaths()
{
  debugPrint(debug_, "search", 1, "delete paths");
  if (arrivals_exist_) {
    VertexIterator vertex_iter(graph_);
    while (vertex_iter.hasNext()) {
      Vertex *vertex = vertex_iter.next();
      deletePaths(vertex);
    }

    deleteContents(enum_paths_);

    filtered_arrivals_.clear();
    arrivals_exist_ = false;
  }
}

void
Search::saveEnumPath(Path *path)
{
  enum_paths_.push_back(path);
}

// Delete with incremental tns/wns update.
void
Search::deletePathsIncr(Vertex *vertex)
{
  tnsNotifyBefore(vertex);
  if (worst_slacks_)
    worst_slacks_->worstSlackNotifyBefore(vertex);
  deletePaths(vertex);
}

void
Search::deletePaths(Vertex *vertex)
{
  debugPrint(debug_, "search", 4, "delete paths %s",
             vertex->to_string(this).c_str());
  TagGroup *tag_group = tagGroup(vertex);
  if (tag_group) {
    vertex->deletePaths();
    tag_group->decrRefCount();
  }
}

////////////////////////////////////////////////////////////////

// from/thrus/to are owned and deleted by Search.
// Returned sequence is owned by the caller.
// PathEnds are owned by Search PathGroups and deleted on next call.
PathEndSeq
Search::findPathEnds(ExceptionFrom *from,
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
                     bool clk_gating_hold)
{
  findFilteredArrivals(from, thrus, to, unconstrained, true);
  if (!variables_->recoveryRemovalChecksEnabled())
    recovery = removal = false;
  if (!variables_->gatedClkChecksEnabled())
    clk_gating_setup = clk_gating_hold = false;
  ensureDownstreamClkPins();
  const ModeSeq modes = Scene::modesSorted(scenes);
  PathEndSeq path_ends;
  for (Mode *mode : modes) {
    PathGroups *path_groups = mode->makePathGroups(group_path_count,
                                                   endpoint_path_count,
                                                   unique_pins, unique_edges,
                                                   slack_min, slack_max,
                                                   group_names,
                                                   setup, hold,
                                                   recovery, removal,
                                                   clk_gating_setup, clk_gating_hold,
                                                   unconstrained_paths_);
    SceneSeq mode_scenes;
    for (Scene *scene : scenes) {
      if (scene->mode() == mode)
        mode_scenes.push_back(scene);
    }
    path_groups->makePathEnds(to, mode_scenes, min_max, sort_by_slack,
                              unconstrained_paths_, path_ends);
  }
  for (const Mode *mode : modes)
    mode->sdc()->reportClkToClkMaxCycleWarnings();
  return path_ends;
}

void
Search::findFilteredArrivals(ExceptionFrom *from,
                             ExceptionThruSeq *thrus,
                             ExceptionTo *to,
                             bool unconstrained,
                             bool thru_latches)
{
  unconstrained_paths_ = unconstrained;
  checkFromThrusTo(from, thrus, to);
  filter_from_ = from;
  filter_thrus_ = thrus;
  filter_to_ = to;
  if ((from
       && (from->pins()
           || from->instances()))
      || thrus) {
    for (const Mode *mode : modes_) {
      Sdc *sdc = mode->sdc();
      sdc->makeFilter(from ? from->clone(network_) : nullptr,
                      thrus ? exceptionThrusClone(thrus, network_) : nullptr);
    }
    have_filter_ = true;
    findFilteredArrivals(thru_latches);
  }
  else
    // These cases do not require filtered arrivals.
    //  -from clocks
    //  -to
    findAllArrivals(thru_latches, false);
}

// From/thrus/to are used to make a filter exception.  If the last
// search used a filter arrival/required times were only found for a
// subset of the paths.  Delete the paths that have a filter
// exception state.
void
Search::deleteFilteredArrivals()
{
  if (have_filter_) {
    ExceptionThruSeq *thrus = filter_thrus_;
    if ((filter_from_
         && (filter_from_->pins()
             || filter_from_->instances()))
        || thrus) {
      for (Vertex *vertex : filtered_arrivals_) {
        deletePathsIncr(vertex);
        arrivalInvalid(vertex);
        requiredInvalid(vertex);
      }
      bool check_filter_arrivals = false;
      if (check_filter_arrivals) {
        VertexIterator vertex_iter(graph_);
        while (vertex_iter.hasNext()) {
          Vertex *vertex = vertex_iter.next();
          TagGroup *tag_group = tagGroup(vertex);
          if (tag_group
              && tag_group->hasFilterTag())
            filtered_arrivals_.erase(vertex);
        }
        if (!filtered_arrivals_.empty()) {
          report_->reportLine("Filtered verticies mismatch");
          for (Vertex *vertex : filtered_arrivals_)
            report_->reportLine(" %s", vertex->to_string(this).c_str());
        }
      }
      filtered_arrivals_.clear();
      deleteFilterTagGroups();
      deleteFilterTags();
      deleteFilterClkInfos();
    }
  }
  // Delete filter_from/thru/to even if there is no filter_.
  deleteFilter();
}

void
Search::deleteFilterTagGroups()
{
  for (TagGroupIndex i = 0; i < tag_group_next_; i++) {
    TagGroup *group = tag_groups_[i];
    if (group
        && group->hasFilterTag())
      deleteTagGroup(group);
  }
}

void
Search::deleteTagGroup(TagGroup *group)
{
  tag_group_set_->erase(group);
  tag_groups_[group->index()] = nullptr;
  tag_group_free_indices_.push_back(group->index());
  delete group;
}

void
Search::deleteFilterTags()
{
  for (TagIndex i = 0; i < tag_next_; i++) {
    Tag *tag = tags_[i];
    if (tag
        && (tag->isFilter()
            || tag->clkInfo()->crprPathRefsFilter())) {
      tags_[i] = nullptr;
      tag_set_->erase(tag);
      delete tag;
    }
  }
}

void
Search::deleteFilterClkInfos()
{
  for (auto itr = clk_info_set_->cbegin(); itr != clk_info_set_->cend(); ) {
    const ClkInfo *clk_info = *itr;
    if (clk_info->crprPathRefsFilter()) {
      itr = clk_info_set_->erase(itr);
      delete clk_info;
    }
    else
      itr++;
  }
}

void
Search::findFilteredArrivals(bool thru_latches)
{
  filtered_arrivals_.clear();
  findArrivalsSeed();
  seedFilterStarts();
  Level max_level = levelize_->maxLevel();
  // Search always_to_endpoint to search from exisiting arrivals at
  // fanin startpoints to reach -thru/-to endpoints.
  arrival_visitor_->init(true, false, eval_pred_);
  // Iterate until data arrivals at all latches stop changing.
  postpone_latch_outputs_ = true;
  enqueuePendingClkFanouts();
  for (int pass = 1; pass == 1 || (thru_latches && havePendingLatchOutputs()) ; pass++) {
    if (thru_latches)
      enqueuePendingLatchOutputs();
    debugPrint(debug_, "search", 1, "find arrivals pass %d", pass);
    int arrival_count = arrival_iter_->visitParallel(max_level, arrival_visitor_);
    deleteTagsPrev();
    debugPrint(debug_, "search", 1, "found %d arrivals", arrival_count);
    postpone_latch_outputs_ = false;
  }
  arrivals_exist_ = true;
}

// Delete stale tag arrarys.
void
Search::deleteTagsPrev()
{
  for (Tag** tags: tags_prev_)
    delete [] tags;
  tags_prev_.clear();

  for (TagGroup** tag_groups: tag_groups_prev_)
    delete [] tag_groups;
  tag_groups_prev_.clear();
}

void
Search::deleteUnusedTagGroups()
{
  for (TagGroupIndex i = 0; i < tag_group_next_; i++) {
    TagGroup *group = tag_groups_[i];
    if (group && group->refCount() == 0)
      deleteTagGroup(group);
  }
}

VertexSeq
Search::filteredEndpoints()
{
  VertexSeq ends;
  for (Vertex *vertex : filtered_arrivals_) {
    if (isEndpoint(vertex))
      ends.push_back(vertex);
  }
  return ends;
}

class SeedFaninsThruHierPin : public HierPinThruVisitor
{
public:
  SeedFaninsThruHierPin(Graph *graph,
                        Search *search);

protected:
  virtual void visit(const Pin *drvr,
                     const Pin *load);

  Graph *graph_;
  Search *search_;
};

SeedFaninsThruHierPin::SeedFaninsThruHierPin(Graph *graph,
                                             Search *search) :
  HierPinThruVisitor(),
  graph_(graph),
  search_(search)
{
}

void
SeedFaninsThruHierPin::visit(const Pin *drvr,
                             const Pin *)
{
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(drvr, vertex, bidirect_drvr_vertex);
  search_->arrivalIterator()->enqueue(vertex);
  if (bidirect_drvr_vertex)
    search_->arrivalIterator()->enqueue(bidirect_drvr_vertex);
}

void
Search::seedFilterStarts()
{
  ExceptionPt *first_pt = nullptr;
  if (filter_from_)
    first_pt = filter_from_;
  else if (filter_thrus_)
    first_pt = (*filter_thrus_)[0];
  if (first_pt) {
    PinSet first_pins = first_pt->allPins(network_);
    for (const Pin *pin : first_pins) {
      if (network_->isHierarchical(pin)) {
        SeedFaninsThruHierPin visitor(graph_, this);
        visitDrvrLoadsThruHierPin(pin, network_, &visitor);
      }
      else {
        Vertex *vertex, *bidirect_drvr_vertex;
        graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
        if (vertex)
          arrival_iter_->enqueue(vertex);
        if (bidirect_drvr_vertex)
          arrival_iter_->enqueue(bidirect_drvr_vertex);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
Search::deleteVertexBefore(Vertex *vertex)
{
  if (arrivals_exist_) {
    deletePathsIncr(vertex);
    arrival_iter_->deleteVertexBefore(vertex);
    invalid_arrivals_.erase(vertex);
    filtered_arrivals_.erase(vertex);
  }
  if (requireds_exist_) {
    required_iter_->deleteVertexBefore(vertex);
    invalid_requireds_.erase(vertex);
    invalid_tns_.erase(vertex);
  }
  if (endpoints_initialized_)
    endpoints_.erase(vertex);
  invalid_endpoints_.erase(vertex);
}

void
Search::deleteEdgeBefore(Edge *edge)
{
  Vertex *from = edge->from(graph_);
  Vertex *to = edge->to(graph_);
  arrivalInvalid(to);
  requiredInvalid(from);
  VertexPathIterator path_iter(to, graph_);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    path->clearPrevPath(this);
  }
}

bool
Search::arrivalsValid()
{
  return arrivals_exist_
    && invalid_arrivals_.empty();
}

void
Search::arrivalsInvalid()
{
  if (arrivals_exist_) {
    debugPrint(debug_, "search", 1, "arrivals invalid");
    // Delete paths to make sure no state is left over.
    // For example, set_disable_timing strands a vertex, which means
    // the search won't revisit it to clear the previous arrival.
    deletePathGroups();
    deletePaths();
    deleteTags();
    for (const Mode *mode : modes_)
      mode->genclks()->clear();
    deleteFilter();
    arrivals_seeded_ = false;
    requireds_exist_ = false;
    requireds_seeded_ = false;
    arrival_iter_->clear();
    required_iter_->clear();
    // No need to keep track of incremental updates any more.
    invalid_arrivals_.clear();
    invalid_requireds_.clear();
    tns_exists_ = false;
    clearWorstSlack();
    invalid_tns_.clear();
  }
}

void
Search::requiredsInvalid()
{
  debugPrint(debug_, "search", 1, "requireds invalid");
  requireds_exist_ = false;
  requireds_seeded_ = false;
  invalid_requireds_.clear();
  tns_exists_ = false;
  clearWorstSlack();
  invalid_tns_.clear();
}

void
Search::arrivalInvalid(Vertex *vertex)
{
  if (arrivals_exist_) {
    debugPrint(debug_, "search", 2, "arrival invalid %s",
               vertex->to_string(this).c_str());
    if (!arrival_iter_->inQueue(vertex)) {
      // Lock for StaDelayCalcObserver called by delay calc threads.
      LockGuard lock(invalid_arrivals_lock_);
      invalid_arrivals_.insert(vertex);
    }
    tnsInvalid(vertex);
  }
}

// Move any pending arrival/requireds to invalid before relevelization.
void
Search::levelsChangedBefore()
{
  if (arrivals_exist_) {
    while (arrival_iter_->hasNext()) {
      Vertex *vertex = arrival_iter_->next();
      arrivalInvalid(vertex);
    }
    while (required_iter_->hasNext()) {
      Vertex *vertex = required_iter_->next();
      requiredInvalid(vertex);
    }
  }
}

void
Search::levelChangedBefore(Vertex *vertex)
{
  if (arrivals_exist_) {
    arrival_iter_->remove(vertex);
    required_iter_->remove(vertex);
    arrivalInvalid(vertex);
    requiredInvalid(vertex);
  }
}

void
Search::arrivalInvalid(const Pin *pin)
{
  if (graph_) {
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    arrivalInvalid(vertex);
    if (bidirect_drvr_vertex)
      arrivalInvalid(bidirect_drvr_vertex);
  }
}

void
Search::requiredInvalid(const Instance *inst)
{
  if (graph_) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      requiredInvalid(pin);
    }
    delete pin_iter;
  }
}

void
Search::requiredInvalid(const Pin *pin)
{
  if (graph_) {
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    requiredInvalid(vertex);
    if (bidirect_drvr_vertex)
      requiredInvalid(bidirect_drvr_vertex);
  }
}

void
Search::requiredInvalid(Vertex *vertex)
{
  if (requireds_exist_) {
    debugPrint(debug_, "search", 2, "required invalid %s",
               vertex->to_string(this).c_str());
    if (!required_iter_->inQueue(vertex)) {
      // Lock for StaDelayCalcObserver called by delay calc threads.
      LockGuard lock(invalid_arrivals_lock_);
      invalid_requireds_.insert(vertex);
    }
    tnsInvalid(vertex);
  }
}

////////////////////////////////////////////////////////////////

void
Search::findClkArrivals()
{
  findAllArrivals(false, true);
}

void
Search::seedClkVertexArrivals()
{
  PinSet clk_pins(network_);
  findClkVertexPins(clk_pins);
  for (const Pin *pin : clk_pins) {
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    arrival_iter_->enqueue(vertex);
    if (bidirect_drvr_vertex)
      arrival_iter_->enqueue(bidirect_drvr_vertex);
  }
}

Arrival
Search::clockInsertion(const Clock *clk,
                       const Pin *pin,
                       const RiseFall *rf,
                       const MinMax *min_max,
                       const EarlyLate *early_late,
                       const Mode *mode) const
{
  float insert;
  bool exists;
  mode->sdc()->clockInsertion(clk, pin, rf, min_max, early_late, insert, exists);
  if (exists)
    return insert;
  else if (clk->isGeneratedWithPropagatedMaster())
    return mode->genclks()->insertionDelay(clk, pin, rf, early_late);
  else
    return 0.0;
}

////////////////////////////////////////////////////////////////

void
Search::findAllArrivals()
{
  findAllArrivals(true, false);
}

void
Search::findAllArrivals(bool thru_latches,
                        bool clks_only)
{
  if (!clks_only)
    enqueuePendingClkFanouts();
  arrival_visitor_->init(false, clks_only, eval_pred_);
  // Iterate until data arrivals at all latches stop changing.
  postpone_latch_outputs_ = true;
  for (int pass = 1; pass == 1 || (thru_latches && havePendingLatchOutputs()); pass++) {
    enqueuePendingLatchOutputs();
    debugPrint(debug_, "search", 1, "find arrivals pass %d", pass);
    findArrivals1(levelize_->maxLevel());
    if (pass > 2)
      postpone_latch_outputs_ = false;
  }
}

bool
Search::havePendingLatchOutputs()
{
  return !pending_latch_outputs_.empty();
}

void
Search::clearPendingLatchOutputs()
{
  pending_latch_outputs_.clear();
}

void
Search::enqueuePendingLatchOutputs()
{
  for (Vertex *latch_vertex : pending_latch_outputs_) {
    debugPrint(debug_, "search", 2, "enqueue latch output %s",
               latch_vertex->to_string(this).c_str());
    arrival_iter_->enqueue(latch_vertex);
  }
  clearPendingLatchOutputs();
}

void
Search::enqueuePendingClkFanouts()
{
  for (Vertex *vertex : pending_clk_endpoints_) {
    debugPrint(debug_, "search", 2, "enqueue clk fanout %s",
               vertex->to_string(this).c_str());
    arrival_iter_->enqueueAdjacentVertices(vertex, search_adj_);
  }
  pending_clk_endpoints_.clear();
}

void
Search::postponeClkFanouts(Vertex *vertex)
{
  LockGuard lock(pending_clk_endpoints_lock_);
  pending_clk_endpoints_.insert(vertex);
}

void
Search::findArrivals()
{
  findArrivals(levelize_->maxLevel());
}

void
Search::findArrivals(Level level)
{
  arrival_visitor_->init(false, false, eval_pred_);
  findArrivals1(level);
}

void
Search::findArrivals1(Level level)
{
  debugPrint(debug_, "search", 1, "find arrivals to level %d", level);
  findArrivalsSeed();
  Stats stats(debug_, report_);
  int arrival_count = arrival_iter_->visitParallel(level, arrival_visitor_);
  deleteTagsPrev();
  if (arrival_count > 0)
    deleteUnusedTagGroups();
  stats.report("Find arrivals");
  arrivals_exist_ = true;
  debugPrint(debug_, "search", 1, "found %d arrivals", arrival_count);
}

void
Search::findArrivalsSeed()
{
  if (!arrivals_seeded_) {
    for (const Mode *mode : modes_)
      mode->genclks()->ensureInsertionDelays();
    arrival_iter_->clear();
    required_iter_->clear();
    seedArrivals();
    arrivals_seeded_ = true;
  }
  else {
    arrival_iter_->ensureSize();
    required_iter_->ensureSize();
  }
  seedInvalidArrivals();
}

////////////////////////////////////////////////////////////////

ArrivalVisitor::ArrivalVisitor(const StaState *sta) :
  PathVisitor(nullptr, false, sta)
{
  init0();
  init(true, false, nullptr);
}

// Copy constructor.
ArrivalVisitor::ArrivalVisitor(bool always_to_endpoints,
                               SearchPred *pred,
                               const StaState *sta) :
  PathVisitor(pred, true, sta)
{
  init0();
  init(always_to_endpoints, false, pred);
}

void
ArrivalVisitor::init0()
{
  tag_bldr_ = new TagGroupBldr(true, this);
  tag_bldr_no_crpr_ = new TagGroupBldr(false, this);
  adj_pred_ = new SearchAdj(tag_bldr_, this);
}

void
ArrivalVisitor::init(bool always_to_endpoints,
                     bool clks_only,
                     SearchPred *pred)
{
  always_to_endpoints_ = always_to_endpoints;
  clks_only_ = clks_only;
  pred_ = pred;
  crpr_active_ = variables_->crprEnabled();
}


VertexVisitor *
ArrivalVisitor::copy() const
{
  return new ArrivalVisitor(always_to_endpoints_, pred_, this);
}

void
ArrivalVisitor::copyState(const StaState *sta)
{
  StaState::copyState(sta);
  adj_pred_->copyState(sta);
}

ArrivalVisitor::~ArrivalVisitor()
{
  delete tag_bldr_;
  delete tag_bldr_no_crpr_;
  delete adj_pred_;
}

void
ArrivalVisitor::setAlwaysToEndpoints(bool to_endpoints)
{
  always_to_endpoints_ = to_endpoints;
}

void
ArrivalVisitor::visit(Vertex *vertex)
{
  debugPrint(debug_, "search", 2, "find arrivals %s",
             vertex->to_string(this).c_str());
  Pin *pin = vertex->pin();
  tag_bldr_->init(vertex);
  has_fanin_one_ = graph_->hasFaninOne(vertex);
  if (crpr_active_
      && !has_fanin_one_)
    tag_bldr_no_crpr_->init(vertex);

  visitFaninPaths(vertex);
  if (crpr_active_
      && search_->crprPathPruningEnabled()
      // No crpr for ideal clocks.
      && tag_bldr_->hasPropagatedClk()
      && !has_fanin_one_)
    pruneCrprArrivals();

  // Insert paths that originate here.
  seedArrivals(vertex);

  bool is_clk = tag_bldr_->hasClkTag();
  bool arrivals_changed = search_->arrivalsChanged(vertex, tag_bldr_);
  // If vertex is a latch data input arrival that changed from the
  // previous eval pass enqueue the latch outputs to be re-evaled on the
  // next pass.
  if (arrivals_changed
      && network_->isLatchData(pin))
    search_->enqueueLatchDataOutputs(vertex);

  if ((always_to_endpoints_
       || arrivals_changed)) {
    if (clks_only_
        && vertex->isRegClk()) {
      debugPrint(debug_, "search", 3, "postponing clk fanout");
      search_->postponeClkFanouts(vertex);
    }
    else
      search_->arrivalIterator()->enqueueAdjacentVertices(vertex, adj_pred_);
  }
  if (arrivals_changed) {
    debugPrint(debug_, "search", 4, "arrivals changed");
    search_->setVertexArrivals(vertex, tag_bldr_);
    search_->tnsInvalid(vertex);
    constrainedRequiredsInvalid(vertex, is_clk);
  }
}

void
ArrivalVisitor::seedArrivals(Vertex *vertex)
{
  const Pin *pin = vertex->pin();
  bool is_clk = tag_bldr_->hasClkTag();
  for (const Mode *mode : modes_) {
    const Sdc *sdc = mode->sdc();
    mode->genclks()->copyGenClkSrcPaths(vertex, tag_bldr_);
    if (sdc->isLeafPinClock(pin))
      search_->seedClkArrivals(pin, mode, tag_bldr_);
    if (search_->isInputArrivalSrchStart(vertex))
      search_->seedInputArrival(pin, vertex, mode, tag_bldr_);
    // Do not apply input delay to bidir load vertices.
    if (!(network_->direction(pin)->isBidirect()
          && !vertex->isBidirectDriver())
        && !network_->isTopLevelPort(pin)
        && sdc->hasInputDelay(pin))
      search_->seedInputSegmentArrival(pin, vertex, mode, tag_bldr_);
    if (sdc->isPathDelayInternalFrom(pin)
        && !sdc->isLeafPinClock(pin))
      // set_min/max_delay -from internal pin.
      search_->makeUnclkedPaths(vertex, false, true, tag_bldr_, mode);
    if (search_->isSrchRoot(vertex, mode)) {
      bool is_reg_clk = vertex->isRegClk();
      if (is_reg_clk
          // Internal roots isolated by disabled pins are seeded with no clock.
          || (search_->unconstrainedPaths()
              && !network_->isTopLevelPort(pin))) {
        debugPrint(debug_, "search", 2, "arrival seed unclked root %s",
                   network_->pathName(pin));
        search_->makeUnclkedPaths(vertex, is_reg_clk, false, tag_bldr_, mode);
      }
    }
    // Register/latch clock pin that is not connected to a declared clock.
    // Seed with unclocked tag, zero arrival and allow search thru reg
    // clk->q edges.
    // These paths are required to report path delays from unclocked registers
    // For example, "set_max_delay -to" from an unclocked source register.
    if (vertex->isRegClk() && !is_clk) {
      debugPrint(debug_, "search", 2, "arrival seed unclked reg clk %s",
                 network_->pathName(pin));
      search_->makeUnclkedPaths(vertex, true, false, tag_bldr_, mode);
    }
    enqueueRefPinInputDelays(pin, sdc);
  }
}

// When a clock arrival changes, the required time changes for any
// timing checks, data checks or gated clock enables constrained
// by the clock pin.
void
ArrivalVisitor::constrainedRequiredsInvalid(Vertex *vertex,
                                            bool is_clk)
{
  Pin *pin = vertex->pin();
  if (network_->isLoad(pin)
      && search_->requiredsExist()) {
    if (is_clk && network_->isCheckClk(pin)) {
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        if (edge->role()->isTimingCheck()) {
          Vertex *to_vertex = edge->to(graph_);
          search_->requiredInvalid(to_vertex);
        }
      }
    }
    // Data checks (vertex does not need to be a clk).
    for (const Mode *mode : modes_) {
      const Sdc *sdc = mode->sdc();
      DataCheckSet *data_checks = sdc->dataChecksFrom(pin);
      if (data_checks) {
        for (DataCheck *data_check : *data_checks) {
          Pin *to = data_check->to();
          search_->requiredInvalid(to);
        }
      }

      // Gated clocks.
      if (is_clk && variables_->gatedClkChecksEnabled()) {
        PinSet enable_pins = search_->gatedClk()->gatedClkEnables(vertex, mode);
        for (const Pin *enable : enable_pins)
          search_->requiredInvalid(enable);
      }
    }
  }
}

bool
Search::arrivalsChanged(Vertex *vertex,
                        TagGroupBldr *tag_bldr)
{
  Path *paths1 = vertex->paths();
  if (paths1) {
    TagGroup *tag_group = tagGroup(vertex);
    if (tag_group == nullptr
        || tag_group->pathCount() != tag_bldr->pathCount())
      return true;
    for (auto const [tag1, path_index1] : *tag_group->pathIndexMap()) {
      Path *path1 = &paths1[path_index1];
      Path *path2 = tag_bldr->tagMatchPath(tag1);
      if (path2 == nullptr
          || path1->tag(this) != path2->tag(this)
          || !delayEqual(path1->arrival(), path2->arrival())
          || path1->prevEdge(this) != path2->prevEdge(this)
          || path1->prevArc(this) != path2->prevArc(this)
          || path1->prevPath() != path2->prevPath())
        return true;
    }
    return false;
  }
  else
    return !tag_bldr->empty();
}

bool
ArrivalVisitor::visitFromToPath(const Pin * /* from_pin */,
                                Vertex *from_vertex,
                                const RiseFall *from_rf,
                                Tag *from_tag,
                                Path *from_path,
                                const Arrival &from_arrival,
                                Edge *edge,
                                TimingArc *arc,
                                ArcDelay arc_delay,
                                Vertex * /* to_vertex */,
                                const RiseFall *to_rf,
                                Tag *to_tag,
                                Arrival &to_arrival,
                                const MinMax *min_max)
{
  debugPrint(debug_, "search", 3, " %s",
             from_vertex->to_string(this).c_str());
  debugPrint(debug_, "search", 3, "  %s -> %s %s",
             from_rf->to_string().c_str(),
             to_rf->to_string().c_str(),
             min_max->to_string().c_str());
  debugPrint(debug_, "search", 3, "  from tag: %s",
             from_tag->to_string(this).c_str());
  debugPrint(debug_, "search", 3, "  to tag  : %s",
             to_tag->to_string(this).c_str());
  const ClkInfo *to_clk_info = to_tag->clkInfo();
  bool to_is_clk = to_tag->isClock();
  Path *match;
  size_t path_index;
  tag_bldr_->tagMatchPath(to_tag, match, path_index);
  if (match == nullptr
      || delayGreater(to_arrival, match->arrival(), min_max, this)) {
    debugPrint(debug_, "search", 3, "   %s + %s = %s %s %s",
               delayAsString(from_arrival, this),
               delayAsString(arc_delay, this),
               delayAsString(to_arrival, this),
               min_max == MinMax::max() ? ">" : "<",
               match ? delayAsString(match->arrival(), this) : "MIA");
    tag_bldr_->setMatchPath(match, path_index, to_tag, to_arrival, from_path, edge, arc);
    if (crpr_active_
        && !has_fanin_one_
        && to_clk_info->hasCrprClkPin()
        && !to_is_clk) {
      tag_bldr_no_crpr_->tagMatchPath(to_tag, match, path_index);
      if (match == nullptr
          || delayGreater(to_arrival, match->arrival(), min_max, this)) {
        tag_bldr_no_crpr_->setMatchPath(match, path_index, to_tag, to_arrival,
                                        from_path, edge, arc);
      }
    }
  }
  return true;
}

void
ArrivalVisitor::pruneCrprArrivals()
{
  CheckCrpr *crpr = search_->checkCrpr();
  PathIndexMap &path_index_map = tag_bldr_->pathIndexMap();
  for (auto path_itr = path_index_map.cbegin(); path_itr != path_index_map.cend(); ) {
    Tag *tag = path_itr->first;
    size_t path_index = path_itr->second;
    const ClkInfo *clk_info = tag->clkInfo();
    bool deleted_tag = false;
    if (!tag->isClock()
        && clk_info->hasCrprClkPin()) {
      const MinMax *min_max = tag->minMax();
      Path *path_no_crpr = tag_bldr_no_crpr_->tagMatchPath(tag);
      if (path_no_crpr) {
        Arrival max_arrival = path_no_crpr->arrival();
        const ClkInfo *clk_info_no_crpr = path_no_crpr->clkInfo(this);
        Arrival max_crpr = crpr->maxCrpr(clk_info_no_crpr);
        Arrival max_arrival_max_crpr = (min_max == MinMax::max())
          ? max_arrival - max_crpr
          : max_arrival + max_crpr;
        debugPrint(debug_, "search", 4, "  cmp %s %s - %s = %s",
                   tag->to_string(this).c_str(),
                   delayAsString(max_arrival, this),
                   delayAsString(max_crpr, this),
                   delayAsString(max_arrival_max_crpr, this));
        Arrival arrival = tag_bldr_->arrival(path_index);
        // Latch D->Q path uses enable min so crpr clk path min/max
        // does not match the path min/max.
        if (delayGreater(max_arrival_max_crpr, arrival, min_max, this)
            && clk_info_no_crpr->crprClkPath(this)->minMax(this)
            == clk_info->crprClkPath(this)->minMax(this)) {
          debugPrint(debug_, "search", 3, "  pruned %s",
                     tag->to_string(this).c_str());
          path_itr = path_index_map.erase(path_itr);
          deleted_tag = true;
        }
      }
    }
    if (!deleted_tag)
      path_itr++;
  }
}

// Enqueue pins with input delays that use ref_pin as the clock
// reference pin as if there is a timing arc from the reference pin to
// the input delay pin.
void
ArrivalVisitor::enqueueRefPinInputDelays(const Pin *ref_pin,
                                         const Sdc *sdc)
{
  InputDelaySet *input_delays = sdc->refPinInputDelays(ref_pin);
  if (input_delays) {
    BfsFwdIterator *arrival_iter = search_->arrivalIterator();
    for (InputDelay *input_delay : *input_delays) {
      const Pin *pin = input_delay->pin();
      Vertex *vertex, *bidirect_drvr_vertex;
      graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
      arrival_iter->enqueue(vertex);
      if (bidirect_drvr_vertex)
        arrival_iter->enqueue(bidirect_drvr_vertex);
    }
  }
}

void
Search::enqueueLatchDataOutputs(Vertex *vertex)
{
  VertexOutEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->role() == TimingRole::latchDtoQ()) {
      Vertex *out_vertex = edge->to(graph_);
      LockGuard lock(pending_latch_outputs_lock_);
      pending_latch_outputs_.insert(out_vertex);
    }
  }
}

void
Search::enqueueLatchOutput(Vertex *vertex)
{
  LockGuard lock(pending_latch_outputs_lock_);
  pending_latch_outputs_.insert(vertex);
}

void
Search::seedArrivals()
{
  VertexSet vertices = makeVertexSet(this);
  findClockVertices(vertices);
  findRootVertices(vertices);
  findInputDrvrVertices(vertices);

  for (Vertex *vertex : vertices)
    arrival_iter_->enqueue(vertex);
}

void
Search::findClockVertices(VertexSet &vertices)
{
  for (const Mode *mode : modes_) {
    const Sdc *sdc = mode->sdc();
    for (const Clock *clk : sdc->clocks()) {
      for (const Pin *pin : clk->leafPins()) {
        Vertex *vertex, *bidirect_drvr_vertex;
        graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
        vertices.insert(vertex);
        if (bidirect_drvr_vertex)
          vertices.insert(bidirect_drvr_vertex);
      }
    }
  }
}

void
Search::seedInvalidArrivals()
{
  for (Vertex *vertex : invalid_arrivals_)
    arrival_iter_->enqueue(vertex);
  invalid_arrivals_.clear();
}

// Find all of the clock leaf pins.
void
Search::findClkVertexPins(PinSet &clk_pins)
{
  for (Scene *scene : scenes_) {
    const Sdc *sdc = scene->sdc();
    for (const Clock *clk : sdc->clocks()) {
      for (const Pin *pin : clk->leafPins()) {
        clk_pins.insert(pin);
      }
    }
  }
}

void
Search::seedClkArrivals(const Pin *pin,
                        const Mode *mode,
                        TagGroupBldr *tag_bldr)
{
  const Sdc *sdc = mode->sdc();
  ClockSet *clks = sdc->findLeafPinClocks(pin);
  if (clks) {
    for (const Clock *clk : *clks) {
      debugPrint(debug_, "search", 2, "arrival seed clk %s/%s pin %s",
                 mode->name().c_str(),
                 clk->name(),
                 network_->pathName(pin));
      for (Scene *scene : mode->scenes()) {
        for (const MinMax *min_max : MinMax::range()) {
          for (const RiseFall *rf : RiseFall::range()) {
            const ClockEdge *clk_edge = clk->edge(rf);
            const EarlyLate *early_late = min_max;
            if (clk->isGenerated()
                && clk->masterClk() == nullptr)
              seedClkDataArrival(pin, rf, clk, clk_edge, min_max,
                                 0.0, scene, tag_bldr);
            else {
              Arrival insertion = clockInsertion(clk, pin, rf, min_max,
                                                 early_late, mode);
              seedClkArrival(pin, rf, clk, clk_edge, min_max,
                             insertion, scene, tag_bldr);
            }
          }
        }
      }
    }
  }
}

void
Search::seedClkArrival(const Pin *pin,
                       const RiseFall *rf,
                       const Clock *clk,
                       const ClockEdge *clk_edge,
                       const MinMax *min_max,
                       Arrival insertion,
                       Scene *scene,
                       TagGroupBldr *tag_bldr)
{
  Sdc *sdc = scene->sdc();
  bool is_propagated = false;
  float latency = 0.0;
  bool latency_exists;
  // Check for clk pin latency.
  sdc->clockLatency(clk, pin, rf, min_max,
                    latency, latency_exists);
  if (!latency_exists) {
    // Check for clk latency (lower priority).
    sdc->clockLatency(clk, rf, min_max,
                      latency, latency_exists);
    if (latency_exists) {
      // Propagated pin overrides latency on clk.
      if (sdc->isPropagatedClock(pin)) {
        latency = 0.0;
        latency_exists = false;
        is_propagated = true;
      }
    }
    else
      is_propagated = sdc->isPropagatedClock(pin)
        || clk->isPropagated();
  }

  const ClockUncertainties *uncertainties = sdc->clockUncertainties(pin);
  if (uncertainties == nullptr)
    uncertainties = clk->uncertainties();
  // Propagate liberty "pulse_clock" transition to transitive fanout.
  LibertyPort *port = network_->libertyPort(pin);
  const RiseFall *pulse_clk_sense = (port ? port->pulseClkSense() : nullptr);
  const ClkInfo *clk_info = findClkInfo(scene, clk_edge, pin, is_propagated,
                                        nullptr, false,
                                        pulse_clk_sense, insertion, latency,
                                        uncertainties, min_max, nullptr);
  // Only false_paths -from apply to clock tree pins.
  ExceptionStateSet *states = nullptr;
  sdc->exceptionFromClkStates(pin,rf,clk,rf,min_max,states);
  Tag *tag = findTag(scene, rf, min_max, clk_info, true, nullptr, false,
                     states, true, nullptr);
  Arrival arrival(clk_edge->time() + insertion);
  tag_bldr->setArrival(tag, arrival);
}

void
Search::seedClkDataArrival(const Pin *pin,
                           const RiseFall *rf,
                           const Clock *clk,
                           const ClockEdge *clk_edge,
                           const MinMax *min_max,
                           Arrival insertion,
                           Scene *scene,
                           TagGroupBldr *tag_bldr)
{       
  Tag *tag = clkDataTag(pin, clk, rf, clk_edge, insertion, min_max, scene);
  if (tag) {
    // Data arrivals include insertion delay.
    Arrival arrival(clk_edge->time() + insertion);
    tag_bldr->setArrival(tag, arrival);
  }
}

Tag *
Search::clkDataTag(const Pin *pin,
                   const Clock *clk,
                   const RiseFall *rf,
                   const ClockEdge *clk_edge,
                   Arrival insertion,
                   const MinMax *min_max,
                   Scene *scene)
{
  Sdc *sdc = scene->sdc();
  ExceptionStateSet *states = nullptr;
  if (sdc->exceptionFromStates(pin, rf, clk, rf, min_max, states)) {
    bool is_propagated = (clk->isPropagated()
                          || sdc->isPropagatedClock(pin));
    const ClkInfo *clk_info = findClkInfo(scene, clk_edge, pin, is_propagated,
                                          insertion, min_max);
    return findTag(scene, rf, min_max, clk_info, false, nullptr, false,
                   states, true, nullptr);
  }
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

bool
Search::makeUnclkedPaths(Vertex *vertex,
                         bool is_segment_start,
                         bool require_exception,
                         TagGroupBldr *tag_bldr,
                         const Mode *mode)
{
  bool search_from = false;
  const Pin *pin = vertex->pin();
  for (Scene *scene : mode->scenes()) {
    for (const MinMax *min_max : MinMax::range()) {
      for (const RiseFall *rf : RiseFall::range()) {
        Tag *tag = fromUnclkedInputTag(pin, rf, min_max, is_segment_start,
                                       require_exception, scene);
        if (tag) {
          tag_bldr->setArrival(tag, delay_zero);
          search_from = true;
        }
      }
    }
  }
  return search_from;
}

void
Search::findRootVertices(VertexSet &vertices)
{
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    for (Mode *mode : modes_) {
      if (isSrchRoot(vertex, mode)) {
        vertices.insert(vertex);
        break;
      }
    }
  }
}

bool
Search::isSrchRoot(Vertex *vertex,
                   const Mode *mode) const
{
  if (!eval_pred_->searchFrom(vertex, mode))
    return false;
  else {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph_);
      if (!edge->role()->isTimingCheck()
          && (eval_pred_->searchFrom(from_vertex, mode)
              && eval_pred_->searchThru(edge, mode)))
        return false;
    }
  }
  return true;
}

void
Search::findInputDrvrVertices(VertexSet &vertices)
{
  Instance *top_inst = network_->topInstance();
  InstancePinIterator *pin_iter = network_->pinIterator(top_inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyInput())
      vertices.insert(graph_->pinDrvrVertex(pin));
  }
  delete pin_iter;
}

bool
Search::isInputArrivalSrchStart(Vertex *vertex)
{
  const Pin *pin = vertex->pin();
  PortDirection *dir = network_->direction(pin);
  bool is_top_level_port = network_->isTopLevelPort(pin);
  return (is_top_level_port
          && (dir->isInput()
              || (dir->isBidirect() && vertex->isBidirectDriver()))) ;
}

void
Search::seedInputArrival(const Pin *pin,
                         Vertex *vertex,
                         const Mode *mode,
                         TagGroupBldr *tag_bldr)
{
  const Sdc *sdc = mode->sdc();
  if (sdc->hasInputDelay(pin))
    seedInputArrival1(pin, vertex, false, mode, tag_bldr);
  else if (!sdc->isLeafPinClock(pin))
    // Seed inputs without set_input_delays.
    seedInputDelayArrival(pin, vertex, nullptr, false, mode, tag_bldr);
}

void
Search::seedInputSegmentArrival(const Pin *pin,
                                Vertex *vertex,
                                const Mode *mode,
                                TagGroupBldr *tag_bldr)
{
  seedInputArrival1(pin, vertex, true, mode, tag_bldr);
}

void
Search::seedInputArrival1(const Pin *pin,
                          Vertex *vertex,
                          bool is_segment_start,
                          const Mode *mode,
                          TagGroupBldr *tag_bldr)
{
  // There can be multiple arrivals for a pin with wrt different clocks.
  const Sdc *sdc = mode->sdc();
  InputDelaySet *input_delays = sdc->inputDelaysLeafPin(pin);
  if (input_delays) {
    for (InputDelay *input_delay : *input_delays) {
      Clock *input_clk = input_delay->clock();
      ClockSet *pin_clks = sdc->findLeafPinClocks(pin);
      // Input arrival wrt a clock source pin is the clock insertion
      // delay (source latency), but arrivals wrt other clocks
      // propagate.
      if (pin_clks == nullptr
          || !pin_clks->contains(input_clk))
        seedInputDelayArrival(pin, vertex, input_delay, is_segment_start,
                              mode, tag_bldr);
    }
  }
}

void
Search::seedInputDelayArrival(const Pin *pin,
                              Vertex *vertex,
                              InputDelay *input_delay,
                              bool is_segment_start,
                              const Mode *mode,
                              TagGroupBldr *tag_bldr)
{
  debugPrint(debug_, "search", 2,
             input_delay
             ? "arrival seed input arrival %s"
             : "arrival seed input %s",
             vertex->to_string(this).c_str());
  const ClockEdge *clk_edge = nullptr;
  const Pin *ref_pin = nullptr;
  const Sdc *sdc = mode->sdc();
  if (input_delay) {
    clk_edge = input_delay->clkEdge();
    if (clk_edge == nullptr
        && variables_->useDefaultArrivalClock())
      clk_edge = sdc->defaultArrivalClockEdge();
    ref_pin = input_delay->refPin();
  }
  else if (variables_->useDefaultArrivalClock())
    clk_edge = sdc->defaultArrivalClockEdge();
  if (ref_pin) {
    Vertex *ref_vertex = graph_->pinLoadVertex(ref_pin);
    for (Scene *scene : mode->scenes()) {
      for (const MinMax *min_max : MinMax::range()) {
        const RiseFall *ref_rf = input_delay->refTransition();
        const Clock *clk = input_delay->clock();
        VertexPathIterator ref_path_iter(ref_vertex, scene, min_max, ref_rf, this);
        while (ref_path_iter.hasNext()) {
          Path *ref_path = ref_path_iter.next();
          if (ref_path->isClock(this)
              && (clk == nullptr
                  || ref_path->clock(this) == clk)) {
            float ref_arrival, ref_insertion, ref_latency;
            inputDelayRefPinArrival(ref_path, ref_path->clkEdge(this), min_max,
                                    sdc, ref_arrival, ref_insertion,
                                    ref_latency);
            seedInputDelayArrival(pin, input_delay, ref_path->clkEdge(this),
                                  ref_arrival, ref_insertion, ref_latency,
                                  is_segment_start, min_max, scene, tag_bldr);
          }
        }
      }
    }
  }
  else {
    for (const MinMax *min_max : MinMax::range()) {
      float clk_arrival, clk_insertion, clk_latency;
      inputDelayClkArrival(input_delay, clk_edge, min_max, mode,
                           clk_arrival, clk_insertion, clk_latency);
      for (Scene *scene : mode->scenes()) {
        seedInputDelayArrival(pin, input_delay, clk_edge,
                              clk_arrival, clk_insertion, clk_latency,
                              is_segment_start, min_max, scene, tag_bldr);
      }
    }
  }
}

// Input delays with -reference_pin use the clock network latency
// from the clock source to the reference pin.
void
Search::inputDelayRefPinArrival(Path *ref_path,
                                const ClockEdge *clk_edge,
                                const MinMax *min_max,
                                const Sdc *sdc,
                                // Return values.
                                float &ref_arrival,
                                float &ref_insertion,
                                float &ref_latency)
{
  Clock *clk = clk_edge->clock();
  if (clk->isPropagated()) {
    const ClkInfo *clk_info = ref_path->clkInfo(this);
    ref_arrival = delayAsFloat(ref_path->arrival());
    ref_insertion = delayAsFloat(clk_info->insertion());
    ref_latency = clk_info->latency();
  }
  else {
    const RiseFall *clk_rf = clk_edge->transition();
    const EarlyLate *early_late = min_max;
    // Input delays from ideal clk reference pins include clock
    // insertion delay but not latency.
    ref_insertion = sdc->clockInsertion(clk, clk_rf, min_max, early_late);
    ref_arrival = clk_edge->time() + ref_insertion;
    ref_latency = 0.0;
  }
}

void
Search::seedInputDelayArrival(const Pin *pin,
                              InputDelay *input_delay,
                              const ClockEdge *clk_edge,
                              float clk_arrival,
                              float clk_insertion,
                              float clk_latency,
                              bool is_segment_start,
                              const MinMax *min_max,
                              Scene *scene,
                              TagGroupBldr *tag_bldr)
{
  for (const RiseFall *rf : RiseFall::range()) {
    if (input_delay) {
      float delay;
      bool exists;
      input_delay->delays()->value(rf, min_max, delay, exists);
      if (exists)
        seedInputDelayArrival(pin, rf, clk_arrival + delay,
                              input_delay, clk_edge,
                              clk_insertion,  clk_latency, is_segment_start,
                              min_max, scene, tag_bldr);
    }
    else
      seedInputDelayArrival(pin, rf, 0.0,  nullptr, clk_edge,
                            clk_insertion,  clk_latency, is_segment_start,
                            min_max, scene, tag_bldr);
  }
}

void
Search::seedInputDelayArrival(const Pin *pin,
                              const RiseFall *rf,
                              float arrival,
                              InputDelay *input_delay,
                              const ClockEdge *clk_edge,
                              float clk_insertion,
                              float clk_latency,
                              bool is_segment_start,
                              const MinMax *min_max,
                              Scene *scene,
                              TagGroupBldr *tag_bldr)
{
  Tag *tag = inputDelayTag(pin, rf, clk_edge, clk_insertion, clk_latency,
                           input_delay, is_segment_start, min_max, scene);
  if (tag)
    tag_bldr->setArrival(tag, arrival);
}

void
Search::inputDelayClkArrival(InputDelay *input_delay,
                             const ClockEdge *clk_edge,
                             const MinMax *min_max,
                             const Mode *mode,
                             // Return values.
                             float &clk_arrival,
                             float &clk_insertion,
                             float &clk_latency)
{
  clk_arrival = 0.0;
  clk_insertion = 0.0;
  clk_latency = 0.0;
  if (input_delay && clk_edge) {
    clk_arrival = clk_edge->time();
    Clock *clk = clk_edge->clock();
    const RiseFall *clk_rf = clk_edge->transition();
    if (!input_delay->sourceLatencyIncluded()) {
      const EarlyLate *early_late = min_max;
      clk_insertion = delayAsFloat(clockInsertion(clk, clk->defaultPin(),
                                                  clk_rf, min_max,
                                                  early_late, mode));
      clk_arrival += clk_insertion;
    }
    if (!clk->isPropagated()
        && !input_delay->networkLatencyIncluded()) {
      clk_latency = mode->sdc()->clockLatency(clk, clk_rf, min_max);
      clk_arrival += clk_latency;
    }
  }
}

Tag *
Search::inputDelayTag(const Pin *pin,
                      const RiseFall *rf,
                      const ClockEdge *clk_edge,
                      float clk_insertion,
                      float clk_latency,
                      InputDelay *input_delay,
                      bool is_segment_start,
                      const MinMax *min_max,
                      Scene *scene)
{
  Clock *clk = nullptr;
  const Pin *clk_pin = nullptr;
  const RiseFall *clk_rf = nullptr;
  bool is_propagated = false;
  ClockUncertainties *clk_uncertainties = nullptr;
  if (clk_edge) {
    clk = clk_edge->clock();
    clk_rf = clk_edge->transition();
    clk_pin = clk->defaultPin();
    is_propagated = clk->isPropagated();
    clk_uncertainties = clk->uncertainties();
  }

  Sdc *sdc = scene->sdc();
  ExceptionStateSet *states = nullptr;
  Tag *tag = nullptr;
  if (sdc->exceptionFromStates(pin,rf,clk,clk_rf,min_max,states)) {
    const ClkInfo *clk_info = findClkInfo(scene, clk_edge, clk_pin,
                                          is_propagated, nullptr,
                                          false, nullptr, clk_insertion, clk_latency,
                                          clk_uncertainties, min_max, nullptr);
    tag = findTag(scene, rf, min_max, clk_info, false,
                  input_delay, is_segment_start, states, true, nullptr);
  }

  if (tag) {
    const ClkInfo *clk_info = tag->clkInfo();
    // Check for state changes on existing tag exceptions (pending -thru pins).
    tag = mutateTag(tag, pin, rf, false, clk_info,
                    pin, rf, false, false, is_segment_start, clk_info,
                    input_delay, nullptr);
  }
  return tag;
}

////////////////////////////////////////////////////////////////

PathVisitor::PathVisitor(const StaState *sta) :

  StaState(sta),
  pred_(sta->search()->evalPred()),
  tag_cache_( nullptr)
{
}

PathVisitor::PathVisitor(SearchPred *pred,
                         bool make_tag_cache,
                         const StaState *sta) :

  StaState(sta),
  pred_(pred),
  tag_cache_(make_tag_cache
             ? new TagSet(128, TagSet::hasher(sta), TagSet::key_equal(sta))
             : nullptr)
{
}

PathVisitor::~PathVisitor()
{
  delete tag_cache_;
}

void
PathVisitor::visitFaninPaths(Vertex *to_vertex)
{
  VertexInEdgeIterator edge_iter(to_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *from_vertex = edge->from(graph_);
    const Pin *from_pin = from_vertex->pin();
    const Pin *to_pin = to_vertex->pin();
    if (!visitEdge(from_pin, from_vertex, edge, to_pin, to_vertex))
      break;
  }
}

void
PathVisitor::visitFanoutPaths(Vertex *from_vertex)
{
  const Pin *from_pin = from_vertex->pin();
  VertexOutEdgeIterator edge_iter(from_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *to_vertex = edge->to(graph_);
    const Pin *to_pin = to_vertex->pin();
    debugPrint(debug_, "search", 3, " %s",
               to_vertex->to_string(this).c_str());
    if (!visitEdge(from_pin, from_vertex, edge, to_pin, to_vertex))
      break;
  }
}

bool
PathVisitor::visitEdge(const Pin *from_pin,
                       Vertex *from_vertex,
                       Edge *edge,
                       const Pin *to_pin,
                       Vertex *to_vertex)
{
  TagGroup *from_tag_group = search_->tagGroup(from_vertex);
  if (from_tag_group) {
    TimingArcSet *arc_set = edge->timingArcSet();
    VertexPathIterator from_iter(from_vertex, search_);
    const Mode *prev_mode = nullptr;
    while (from_iter.hasNext()) {
      Path *from_path = from_iter.next();
      const Mode *mode = from_path->mode(this);
      if (mode == prev_mode
          || (pred_->searchFrom(from_vertex, mode)
              && pred_->searchThru(edge, mode)
              && pred_->searchTo(to_vertex, mode))) {
            prev_mode = mode;
        const MinMax *min_max = from_path->minMax(this);
        const RiseFall *from_rf = from_path->transition(this);
        TimingArc *arc1, *arc2;
        arc_set->arcsFrom(from_rf, arc1, arc2);
        if (!visitArc(from_pin, from_vertex, from_rf, from_path,
                      edge, arc1, to_pin, to_vertex, min_max, mode))
          return false;
        if (!visitArc(from_pin, from_vertex, from_rf, from_path,
                      edge, arc2, to_pin, to_vertex, min_max, mode))
          return false;
      }
    }
  }
  return true;
}

bool
PathVisitor::visitArc(const Pin *from_pin,
                      Vertex *from_vertex,
                      const RiseFall *from_rf,
                      Path *from_path,
                      Edge *edge,
                      TimingArc *arc,
                      const Pin *to_pin,
                      Vertex *to_vertex,
                      const MinMax *min_max,
                      const Mode *mode)
{
  if (arc) {
    const RiseFall *to_rf = arc->toEdge()->asRiseFall();
    if (searchThru(from_vertex, from_rf, edge, to_vertex, to_rf, mode))
      return visitFromPath(from_pin, from_vertex, from_rf, from_path,
                           edge, arc, to_pin, to_vertex, to_rf, min_max);
  }
  return true;
}

bool
PathVisitor::visitFromPath(const Pin *from_pin,
                           Vertex *from_vertex,
                           const RiseFall *from_rf,
                           Path *from_path,
                           Edge *edge,
                           TimingArc *arc,
                           const Pin *to_pin,
                           Vertex *to_vertex,
                           const RiseFall *to_rf,
                           const MinMax *min_max)
{
  const TimingRole *role = edge->role();
  Tag *from_tag = from_path->tag(this);
  Scene *scene = from_tag->scene();
  const Mode *mode = scene->mode();
  const Sdc *sdc = scene->sdc();
  const ClkInfo *from_clk_info = from_tag->clkInfo();
  Tag *to_tag = nullptr;
  const ClockEdge *clk_edge = from_clk_info->clkEdge();
  const Clock *clk = from_clk_info->clock();
  Arrival from_arrival = from_path->arrival();
  ArcDelay arc_delay = 0.0;
  DcalcAPIndex dcalc_ap = from_path->dcalcAnalysisPtIndex(this);
  Arrival to_arrival;
  if (from_clk_info->isGenClkSrcPath()) {
    if (!sdc->clkStopPropagation(clk,from_pin,from_rf,to_pin,to_rf)
        && (variables_->clkThruTristateEnabled()
            || !(role == TimingRole::tristateEnable()
                 || role == TimingRole::tristateDisable()))) {
      const Clock *gclk = from_tag->genClkSrcPathClk();
      if (gclk) {
        Genclks *genclks = mode->genclks();
        VertexSet *fanins = genclks->fanins(gclk);
        // Note: encountering a latch d->q edge means find the
        // latch feedback edges, but they are referenced for 
        // other edges in the gen clk fanout.
        if (role == TimingRole::latchDtoQ())
          genclks->findLatchFdbkEdges(gclk);
        EdgeSet &fdbk_edges = genclks->latchFdbkEdges(gclk);
        if ((role == TimingRole::combinational()
             || role == TimingRole::wire()
             || !gclk->combinational())
            && fanins->contains(to_vertex)
            && !fdbk_edges.contains(edge)) {
          arc_delay = search_->deratedDelay(from_vertex, arc, edge,
                                            true, min_max, dcalc_ap, sdc);
          DcalcAPIndex dcalc_ap = scene->dcalcAnalysisPtIndex(min_max->opposite());
          Delay arc_delay_opp = search_->deratedDelay(from_vertex, arc, edge,
                                                      true, min_max,
                                                      dcalc_ap,
                                                      sdc);
          bool arc_delay_min_max_eq =
            fuzzyEqual(delayAsFloat(arc_delay), delayAsFloat(arc_delay_opp));
          to_tag = search_->thruClkTag(from_path, from_vertex, from_tag, true,
                                       edge, to_rf, arc_delay_min_max_eq,
                                       min_max, scene);
                                       if (to_tag)
          to_arrival = from_arrival + arc_delay;
        }
      }
    }
  }
  else if (role->genericRole() == TimingRole::regClkToQ()) {
    if (clk == nullptr
        || !sdc->clkStopPropagation(from_pin, clk)) {
      arc_delay = search_->deratedDelay(from_vertex, arc, edge, false,
                                        min_max, dcalc_ap, sdc);

      // Remove clock network delay for macros created with propagated
      // clocks when used in a context with ideal clocks.
      if (clk && clk->isIdeal()) {
        const LibertyPort *clk_port = network_->libertyPort(from_pin);
        const LibertyCell *inst_cell = clk_port->libertyCell();
        if (inst_cell->isMacro()) {
          float slew = delayAsFloat(from_path->slew(this));
          arc_delay -= clk_port->clkTreeDelay(slew, from_rf, min_max);
        }
      }

      // Propagate from unclocked reg/latch clk pins, which have no
      // clk but are distinguished with a segment_start flag.
      if ((clk_edge == nullptr
           && from_tag->isSegmentStart())
          // Do not propagate paths from input ports with default
          // input arrival clk thru CLK->Q edges.
          || (clk != sdc->defaultArrivalClock()
              // Only propagate paths from clocks that have not
              // passed thru reg/latch D->Q edges.
              && from_tag->isClock())) {
        const RiseFall *clk_rf = clk_edge ? clk_edge->transition() : nullptr;
        const ClkInfo *to_clk_info = from_clk_info;
        if (from_clk_info->crprClkPath(this) == nullptr
            || network_->direction(to_pin)->isInternal())
          to_clk_info = search_->clkInfoWithCrprClkPath(from_clk_info,
                                                        from_path);
        to_tag = search_->fromRegClkTag(from_pin, from_rf, clk, clk_rf,
                                        to_clk_info, to_pin, to_rf, min_max,
                                        from_tag->scene());
        if (to_tag)
          to_tag = search_->thruTag(to_tag, edge, to_rf, tag_cache_);
        from_arrival = search_->clkPathArrival(from_path, from_clk_info,
                                               clk_edge, min_max);
        to_arrival = from_arrival + arc_delay;
      }
      else
        to_tag = nullptr;
    }
  }
  else if (edge->role() == TimingRole::latchDtoQ()) {
    if (min_max == MinMax::max()
	&& clk) {
      bool postponed = false;
      if (search_->postponeLatchOutputs()) {
        const Path *from_clk_path = from_clk_info->crprClkPath(this);
        if (from_clk_path) {
          Vertex *d_clk_vertex = from_clk_path->vertex(this);
          Level d_clk_level = d_clk_vertex->level();
          Level q_level = to_vertex->level();
          if (d_clk_level >= q_level) {
            // Crpr clk path on latch data input is required to find Q
            // arrival. If the data clk path level is >= Q level the
            // crpr clk path prev_path pointers are not complete.
            debugPrint(debug_, "search", 3, "postponed latch eval %d %s -> %s %d",
                       d_clk_level,
                       d_clk_vertex->to_string(this).c_str(),
                       edge->to_string(this).c_str(),
                       q_level);
            postponed = true;
            search_->enqueueLatchOutput(to_vertex);
          }
        }
      }
      if (!postponed) {
        arc_delay = search_->deratedDelay(from_vertex, arc, edge, false,
                                          min_max, dcalc_ap, sdc);
        latches_->latchOutArrival(from_path, arc, edge, to_tag,
                                  arc_delay, to_arrival);
        if (to_tag)
          to_tag = search_->thruTag(to_tag, edge, to_rf, tag_cache_);
      }
    }
  }
  else if (from_tag->isClock()) {
    ClockSet *clks = sdc->findLeafPinClocks(from_pin);
    // Disable edges from hierarchical clock source pins that do
    // not go thru the hierarchical pin and edges from clock source pins
    // that traverse a hierarchical source pin of a different clock.
    // Clock arrivals used as data also need to be disabled.
    if (!(role == TimingRole::wire()
          && sdc->clkDisabledByHpinThru(clk, from_pin, to_pin))
        // Generated clock source pins have arrivals for the source clock.
        // Do not propagate them past the generated clock source pin.
        && !(clks
             && !clks->contains(const_cast<Clock*>(from_tag->clock())))) {
      // Propagate arrival as non-clock at the end of the clock tree.
      bool to_propagates_clk =
        !sdc->clkStopPropagation(clk,from_pin,from_rf,to_pin,to_rf)
        && (variables_->clkThruTristateEnabled()
            || !(role == TimingRole::tristateEnable()
                 || role == TimingRole::tristateDisable()));
      arc_delay = search_->deratedDelay(from_vertex, arc, edge,
                                        to_propagates_clk, min_max,
                                        dcalc_ap, sdc);
      DcalcAPIndex dcalc_ap_opp = 
        scene->dcalcAnalysisPtIndex(min_max->opposite());
      Delay arc_delay_opp = search_->deratedDelay(from_vertex, arc, edge,
                                                  to_propagates_clk,
                                                  min_max, dcalc_ap_opp, sdc);
      bool arc_delay_min_max_eq =
        fuzzyEqual(delayAsFloat(arc_delay), delayAsFloat(arc_delay_opp));
      to_tag = search_->thruClkTag(from_path, from_vertex, from_tag,
                                   to_propagates_clk, edge, to_rf,
                                   arc_delay_min_max_eq,
                                   min_max, scene);
      to_arrival = from_arrival + arc_delay;
    }
  }
  else {
    if (!(sdc->isPathDelayInternalFromBreak(to_pin)
          || sdc->isPathDelayInternalToBreak(from_pin))) {
      arc_delay = search_->deratedDelay(from_vertex, arc, edge, false,
                                        min_max, dcalc_ap, sdc);
      if (!delayInf(arc_delay)) {
        to_arrival = from_arrival + arc_delay;
        to_tag = search_->thruTag(from_tag, edge, to_rf, tag_cache_);
      }
    }
  }
  if (to_tag)
    return visitFromToPath(from_pin, from_vertex, from_rf,
                           from_tag, from_path, from_arrival,
                           edge, arc, arc_delay,
                           to_vertex, to_rf, to_tag, to_arrival,
                           min_max);
  else
    return true;
}

Arrival
Search::clkPathArrival(const Path *clk_path) const
{
  const ClkInfo *clk_info = clk_path->clkInfo(this);
  const ClockEdge *clk_edge = clk_info->clkEdge();
  const MinMax *min_max = clk_path->minMax(this);
  return clkPathArrival(clk_path, clk_info, clk_edge, min_max);
}

Arrival
Search::clkPathArrival(const Path *clk_path,
                       const ClkInfo *clk_info,
                       const ClockEdge *clk_edge,
                       const MinMax *min_max) const
{
  const Scene *scene = clk_path->scene(this);
  if (clk_path->vertex(this)->isRegClk()
      && clk_path->isClock(this)
      && clk_edge
      && !clk_info->isPropagated()) {
    // Ideal clock, apply ideal insertion delay and latency.
    const EarlyLate *early_late = min_max;
    return clk_edge->time()
      + clockInsertion(clk_edge->clock(), clk_info->clkSrc(),
                       clk_edge->transition(), min_max,
                       early_late, scene->mode())
      + clk_info->latency();
  }
  else
    return clk_path->arrival();
}

Arrival
Search::pathClkPathArrival(const Path *path) const
{
  const ClkInfo *clk_info = path->clkInfo(this);
  if (clk_info->isPropagated()) {
    const Path *src_clk_path = pathClkPathArrival1(path);
    if (src_clk_path)
      return clkPathArrival(src_clk_path);
  }
  // Check for input arrival clock.
  const ClockEdge *clk_edge = path->clkEdge(this);
  if (clk_edge)
    return clk_edge->time() + clk_info->latency();
  return 0.0;
}

// PathExpanded::expand() and PathExpanded::clkPath().
const Path *
Search::pathClkPathArrival1(const Path *path) const
{
  const Path *p = path;
  while (p) {
    Path *prev_path = p->prevPath();
    Edge *prev_edge = p->prevEdge(this);

    if (p->isClock(this))
      return p;
    if (prev_edge) {
      const TimingRole *prev_role = prev_edge->role();
      if (prev_role == TimingRole::regClkToQ()
          || prev_role == TimingRole::latchEnToQ()) {
        return p->prevPath();
      }
      else if (prev_role == TimingRole::latchDtoQ()) {
        Path *enable_path = latches_->latchEnablePath(p, prev_edge);
        return enable_path;
      }
    }
    p = prev_path;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

// Find tag for a path starting with pin/clk_edge.
// Return nullptr if a false path starts at pin/clk_edge.
Tag *
Search::fromUnclkedInputTag(const Pin *pin,
                            const RiseFall *rf,
                            const MinMax *min_max,
                            bool is_segment_start,
                            bool require_exception,
                            Scene *scene)
{
  Sdc *sdc = scene->sdc();
  ExceptionStateSet *states = nullptr;
  if (sdc->exceptionFromStates(pin, rf, nullptr, nullptr, min_max, states)
      && (!require_exception || states)) {
    const ClkInfo *clk_info = findClkInfo(scene, nullptr, nullptr, false,
                                          0.0, min_max);
    return findTag(scene, rf, min_max, clk_info, false, nullptr,
                   is_segment_start, states, true, nullptr);
  }
  return nullptr;
}

Tag *
Search::fromRegClkTag(const Pin *from_pin,
                      const RiseFall *from_rf,
                      const Clock *clk,
                      const RiseFall *clk_rf,
                      const ClkInfo *clk_info,
                      const Pin *to_pin,
                      const RiseFall *to_rf,
                      const MinMax *min_max,
                      Scene *scene)
{
  Sdc *sdc = scene->sdc();
  ExceptionStateSet *states = nullptr;
  if (sdc->exceptionFromStates(from_pin, from_rf, clk, clk_rf,
                                min_max, states)) {
    // Hack for filter -from reg/Q.
    sdc->filterRegQStates(to_pin, to_rf, min_max, states);
    return findTag(scene, to_rf, min_max, clk_info, false, nullptr,
                   false, states, true, nullptr);
  }
  else
    return nullptr;
}

// Insert from_path as ClkInfo crpr_clk_path.
const ClkInfo *
Search::clkInfoWithCrprClkPath(const ClkInfo *from_clk_info,
                               Path *from_path)
{
  Scene *scene = from_clk_info->scene();
  if (crprActive(scene->mode()))
    return findClkInfo(scene,
                       from_clk_info->clkEdge(),
                       from_clk_info->clkSrc(),
                       from_clk_info->isPropagated(),
                       from_clk_info->genClkSrc(),
                       from_clk_info->isGenClkSrcPath(),
                       from_clk_info->pulseClkSense(),
                       from_clk_info->insertion(),
                       from_clk_info->latency(),
                       from_clk_info->uncertainties(),
                       from_clk_info->minMax(),
                       from_path);
  else
    return from_clk_info;
}

// Find tag for a path starting with from_tag going thru edge.
// Return nullptr if the result tag completes a false path.
Tag *
Search::thruTag(Tag *from_tag,
                Edge *edge,
                const RiseFall *to_rf,
                TagSet *tag_cache)
{
  const Pin *from_pin = edge->from(graph_)->pin();
  Vertex *to_vertex = edge->to(graph_);
  const Pin *to_pin = to_vertex->pin();
  const RiseFall *from_rf = from_tag->transition();
  const ClkInfo *from_clk_info = from_tag->clkInfo();
  bool to_is_reg_clk = to_vertex->isRegClk();
  Tag *to_tag = mutateTag(from_tag, from_pin, from_rf, false, from_clk_info,
                          to_pin, to_rf, false, to_is_reg_clk, false,
                          // input delay is not propagated.
                          from_clk_info, nullptr, tag_cache);
  return to_tag;
}

// thruTag for clocks.
Tag *
Search::thruClkTag(Path *from_path,
                   Vertex *from_vertex,
                   Tag *from_tag,
                   bool to_propagates_clk,
                   Edge *edge,
                   const RiseFall *to_rf,
                   bool arc_delay_min_max_eq,
                   const MinMax *min_max,
                   Scene *scene)
{
  const Pin *from_pin = edge->from(graph_)->pin();
  Vertex *to_vertex = edge->to(graph_);
  const Pin *to_pin = to_vertex->pin();
  const RiseFall *from_rf = from_tag->transition();
  const ClkInfo *from_clk_info = from_tag->clkInfo();
  bool from_is_clk = from_tag->isClock();
  bool to_is_reg_clk = to_vertex->isRegClk();
  const TimingRole *role = edge->role();
  bool to_is_clk = (from_is_clk
                    && to_propagates_clk
                    && (role->isWire()
                        || role == TimingRole::combinational()));
  const ClkInfo *to_clk_info = thruClkInfo(from_path, from_vertex,
                                           from_clk_info, from_is_clk,
                                           edge, to_vertex, to_pin, to_is_clk,
                                           arc_delay_min_max_eq, min_max, scene);
  Tag *to_tag = mutateTag(from_tag,from_pin,from_rf,from_is_clk,from_clk_info,
                          to_pin, to_rf, to_is_clk, to_is_reg_clk, false,
                          to_clk_info, nullptr, nullptr);
  return to_tag;
}

const ClkInfo *
Search::thruClkInfo(Path *from_path,
                    Vertex *from_vertex,
                    const ClkInfo *from_clk_info,
                    bool from_is_clk,
                    Edge *edge,
                    Vertex *to_vertex,
                    const Pin *to_pin,
                    bool to_is_clk,
                    bool arc_delay_min_max_eq,
                    const MinMax *min_max,
                    Scene *scene)
{
  const ClkInfo *to_clk_info = from_clk_info;
  const Sdc *sdc = scene->sdc();
  const Mode *mode = scene->mode();
  bool changed = false;
  const ClockEdge *from_clk_edge = from_clk_info->clkEdge();
  const RiseFall *clk_rf = from_clk_edge->transition();

  bool from_clk_prop = from_clk_info->isPropagated();
  bool to_clk_prop = from_clk_prop;
  if (!from_clk_prop
      && sdc->isPropagatedClock(to_pin)) {
    to_clk_prop = true;
    changed = true;
  }

  // Distinguish gen clk src path ClkInfo at generated clock roots,
  // so that generated clock crpr info can be (later) safely set on
  // the clkinfo.
  const Pin *gen_clk_src = nullptr;
  if (from_clk_info->isGenClkSrcPath()
      && crprActive(mode)
      && sdc->isClock(to_pin)) {
    // Don't care that it could be a regular clock root.
    gen_clk_src = to_pin;
    changed = true;
  }

  Path *to_crpr_clk_path = nullptr;
  if (crprActive(mode)
      // Update crpr clk path for combinational paths leaving the clock
      // network (ie, tristate en->out) and buffer driving reg clk.
      && ((from_is_clk
           && !to_is_clk
           && !from_vertex->isRegClk())
          || (to_vertex->isRegClk()
              // If the wire delay to the reg clk pin is zero,
              // leave the crpr_clk_path null to indicate that
              // the reg clk path is the crpr clk path.
              && arc_delay_min_max_eq))) {
    to_crpr_clk_path = from_path;
    changed = true;
  }

  // Propagate liberty "pulse_clock" transition to transitive fanout.
  const RiseFall *from_pulse_sense = from_clk_info->pulseClkSense();
  const RiseFall *to_pulse_sense = from_pulse_sense;
  LibertyPort *port = network_->libertyPort(to_pin);
  if (port && port->pulseClkSense()) {
    to_pulse_sense = port->pulseClkSense();
    changed = true;
  }
  else if (from_pulse_sense &&
           edge->timingArcSet()->sense() == TimingSense::negative_unate) {
    to_pulse_sense = from_pulse_sense->opposite();
    changed = true;
  }

  const Clock *from_clk = from_clk_info->clock();
  Arrival to_insertion = from_clk_info->insertion();
  float to_latency = from_clk_info->latency();
  float latency;
  bool exists;
  sdc->clockLatency(from_clk, to_pin, clk_rf, min_max,
                    latency, exists);
  if (exists) {
    // Latency on pin has precedence over fanin or hierarchical
    // pin latency.
    to_latency = latency;
    to_clk_prop = false;
    changed = true;
  }
  else {
    // Check for hierarchical pin latency thru edge.
    sdc->clockLatency(edge, clk_rf, min_max,
                      latency, exists);
    if (exists) {
      to_latency = latency;
      to_clk_prop = false;
      changed = true;
    }
  }

  const ClockUncertainties *to_uncertainties = from_clk_info->uncertainties();
  const ClockUncertainties *uncertainties = sdc->clockUncertainties(to_pin);
  if (uncertainties) {
    to_uncertainties = uncertainties;
    changed = true;
  }

  if (changed)
    to_clk_info = findClkInfo(scene, from_clk_edge, from_clk_info->clkSrc(),
                              to_clk_prop, gen_clk_src,
                              from_clk_info->isGenClkSrcPath(),
                              to_pulse_sense, to_insertion, to_latency,
                              to_uncertainties, min_max, to_crpr_clk_path);
  return to_clk_info;
}

static size_t
tagsTableRfIndex(size_t tag_index,
                 const RiseFall *rf)
{
  return (tag_index / RiseFall::index_count) * RiseFall::index_count + rf->index();
}

// Find the tag for a path going from from_tag thru edge to to_pin.
Tag *
Search::mutateTag(Tag *from_tag,
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
                  TagSet *tag_cache)
{
  ExceptionStateSet *new_states = nullptr;
  ExceptionStateSet *from_states = from_tag->states();
  Scene *scene = from_tag->scene();
  Sdc *sdc = scene->sdc();
  const MinMax *min_max = from_tag->minMax();
  if (from_states) {
    // Check for state changes in from_tag (but postpone copying state set).
    bool state_change = false;
    for (ExceptionState *state : *from_states) {
      ExceptionPath *exception = state->exception();
      // One edge may traverse multiple hierarchical thru pins.
      while (state->matchesNextThru(from_pin,to_pin,to_rf,min_max,network_)) {
        // Found a -thru that we've been waiting for.
        state = state->nextState();
        state_change = true;
        break;
      }
      if (state_change)
        break;

      // Don't propagate a completed false path -thru unless it is a
      // clock. Clocks carry the completed false path to disable
      // downstream paths that use the clock as data.
      if ((state->isComplete()
           && exception->isFalse()
           && !from_is_clk)
          // to_pin/edge completes a loop path.
          || (exception->isLoop()
              && state->isComplete()))
        return nullptr;

      // Kill path delay tags past the -to pin.
      if ((exception->isPathDelay()
           && sdc->isCompleteTo(state, to_pin, to_rf, min_max))
          // Kill loop tags at register clock pins.
          || (exception->isLoop()
              && to_is_reg_clk)) {
        state_change = true;
        break;
      }
    }

    // Get the set of -thru exceptions starting at to_pin/edge.
    sdc->exceptionThruStates(from_pin, to_pin, to_rf, min_max, new_states);
    if (new_states || state_change) {
      // Second pass to apply state changes and add updated existing
      // states to new states.
      if (new_states == nullptr)
        new_states = new ExceptionStateSet();
      for (auto state : *from_states) {
        ExceptionPath *exception = state->exception();
        // One edge may traverse multiple hierarchical thru pins.
        while (state->matchesNextThru(from_pin,to_pin,to_rf,min_max,network_))
          // Found a -thru that we've been waiting for.
          state = state->nextState();

        // Don't propagate a completed false path -thru unless it is a
        // clock. Clocks carry the completed false path to disable
        // downstream paths that use the clock as data.
        if ((state->isComplete()
             && exception->isFalse()
             && !from_is_clk)
            // to_pin/edge completes a loop path.
            || (exception->isLoop()
                && state->isComplete())) {
          delete new_states;
          return nullptr;
        }

        // Kill path delay tags past the -to pin.
        if (!((exception->isPathDelay()
               && sdc->isCompleteTo(state, from_pin, from_rf, min_max))
              // Kill loop tags at register clock pins.
              || (to_is_reg_clk
                  && exception->isLoop())))
          new_states->insert(state);
      }
    }
  }
  else
    // Get the set of -thru exceptions starting at to_pin/edge.
    sdc->exceptionThruStates(from_pin, to_pin, to_rf, min_max, new_states);

  if (new_states)
    return findTag(scene, to_rf, min_max, to_clk_info, to_is_clk,
                   from_tag->inputDelay(), to_is_segment_start,
                   new_states, true, tag_cache);
  else {
    // No state change.
    if (to_clk_info == from_clk_info
        && to_is_clk == from_is_clk
        && from_tag->isSegmentStart() == to_is_segment_start
        && from_tag->inputDelay() == to_input_delay) {
      return tags_[tagsTableRfIndex(from_tag->index(), to_rf)];
    }
    else
      return findTag(scene, to_rf, min_max, to_clk_info, to_is_clk,
                     to_input_delay, to_is_segment_start,
                     from_states, false, tag_cache);
  }
}

TagGroup *
Search::findTagGroup(TagGroupBldr *tag_bldr)
{
  TagGroup probe(tag_bldr, this);
  LockGuard lock(tag_group_lock_);
  TagGroup *tag_group = findKey(tag_group_set_, &probe);
  if (tag_group == nullptr) {
    TagGroupIndex tag_group_index;
    if (tag_group_free_indices_.empty())
      tag_group_index = tag_group_next_++;
    else {
      tag_group_index = tag_group_free_indices_.back();
      tag_group_free_indices_.pop_back();
    }
    tag_group = tag_bldr->makeTagGroup(tag_group_index, this);
    tag_groups_[tag_group_index] = tag_group;
    tag_group_set_->insert(tag_group);
    // If tag_groups_ needs to grow make the new array and copy the
    // contents into it before updating tags_groups_ so that other threads
    // can use Search::tagGroup(TagGroupIndex) without returning gubbish.
    if (tag_group_next_ == tag_group_capacity_) {
      TagGroupIndex tag_capacity = tag_group_capacity_ * 2;
      TagGroup **tag_groups = new TagGroup*[tag_capacity];
      memcpy(tag_groups, tag_groups_,
             tag_group_capacity_ * sizeof(TagGroup*));
      tag_groups_prev_.push_back(tag_groups_);
      tag_groups_ = tag_groups;
      tag_group_capacity_ = tag_capacity;
      tag_group_set_->reserve(tag_capacity);
    }
    if (tag_group_next_ > tag_group_index_max)
      report_->critical(1510, "max tag group index exceeded");
  }
  return tag_group;
}

void
Search::setVertexArrivals(Vertex *vertex,
                          TagGroupBldr *tag_bldr)
{
  if (tag_bldr->empty())
    deletePathsIncr(vertex);
  else {
    TagGroup *prev_tag_group = tagGroup(vertex);
    Path *prev_paths = vertex->paths();
    TagGroup *tag_group = findTagGroup(tag_bldr);
    if (tag_group == prev_tag_group) {
      tag_bldr->copyPaths(tag_group, prev_paths);
      requiredInvalid(vertex);
    }
    else {
      if (prev_tag_group) {
        vertex->deletePaths();
        prev_tag_group->decrRefCount();
        requiredInvalid(vertex);
      }
      size_t path_count = tag_group->pathCount();
      Path *paths = vertex->makePaths(path_count);
      tag_bldr->copyPaths(tag_group, paths);
      vertex->setTagGroupIndex(tag_group->index());
      tag_group->incrRefCount();
    }
    if (tag_group->hasFilterTag()) {
      LockGuard lock(filtered_arrivals_lock_);
      filtered_arrivals_.insert(vertex);
    }
  }
}

void
Search::checkPrevPaths() const
{
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    VertexPathIterator path_iter(vertex, graph_);
    while (path_iter.hasNext()) {
      Path *path = path_iter.next();
      path->checkPrevPath(this);
    }
  }
}

class ReportPathLess
{
public:
  ReportPathLess(const StaState *sta);
  bool operator()(const Path *path1,
                  const Path *path2) const;

private:
  const StaState *sta_;
};


ReportPathLess::ReportPathLess(const StaState *sta) :
  sta_(sta)
{
}

bool
ReportPathLess::operator()(const Path *path1,
                           const Path *path2) const
{
  return Tag::cmp(path1->tag(sta_), path2->tag(sta_), sta_) < 0;
}

void
Search::reportArrivals(Vertex *vertex,
                       bool report_tag_index) const
{
  report_->reportLine("Vertex %s", vertex->to_string(this).c_str());
  TagGroup *tag_group = tagGroup(vertex);
  if (tag_group) {
    if (report_tag_index)
      report_->reportLine("Group %u", tag_group->index());
    std::vector<const Path*> paths;
    VertexPathIterator path_iter(vertex, this);
    while (path_iter.hasNext()) {
      const Path *path = path_iter.next();
      paths.push_back(path);
    }
    sort(paths, ReportPathLess(this));
    for (const Path *path : paths) {
      const Tag *tag = path->tag(this);
      const RiseFall *rf = tag->transition();
      const char *req = delayAsString(path->required(), this);
      bool report_prev = false;
      std::string prev_str;
      if (report_prev) {
        prev_str = "prev ";
        Path *prev_path = path->prevPath();
        if (prev_path) {
          prev_str += prev_path->to_string(this);
          prev_str += " ";
          const Edge *prev_edge = path->prevEdge(this);
          TimingArc *arc = path->prevArc(this);
          prev_str += prev_edge->from(graph_)->to_string(this);
          prev_str += " ";
          prev_str += arc->fromEdge()->to_string();
          prev_str += " -> ";
          prev_str += prev_edge->to(graph_)->to_string(this);
          prev_str += " ";
          prev_str += arc->toEdge()->to_string();
        }
        else
          prev_str += "NULL";
      }
      report_->reportLine(" %s %s %s / %s %s%s",
                          rf->to_string().c_str(),
                          path->minMax(this)->to_string().c_str(),
                          delayAsString(path->arrival(), this),
                          req,
                          tag->to_string(report_tag_index, false, this).c_str(),
                          prev_str.c_str());
    }
  }
  else
    report_->reportLine(" no arrivals");
}

TagGroup *
Search::tagGroup(TagGroupIndex index) const
{
  return tag_groups_[index];
}

TagGroup *
Search::tagGroup(const Vertex *vertex) const
{
  TagGroupIndex index = vertex->tagGroupIndex();
  if (index == tag_group_index_max)
    return nullptr;
  else
    return tag_groups_[index];
}

TagGroupIndex
Search::tagGroupCount() const
{
  return tag_group_set_->size();
}

void
Search::reportTagGroups() const
{
  for (TagGroupIndex i = 0; i < tag_group_next_; i++) {
    TagGroup *tag_group = tag_groups_[i];
    if (tag_group) {
      report_->reportLine("Group %4u hash = %4lu (%4lu)",
                          i,
                          tag_group->hash(),
                          tag_group->hash() % tag_group_set_->bucket_count());
      tag_group->reportArrivalMap(this);
    }
  }
  size_t long_hash = 0;
  for (size_t i = 0; i < tag_group_set_->bucket_count(); i++) {
    if (tag_group_set_->bucket_size(i) > long_hash)
      long_hash = i;
  }
  report_->reportLine("Longest hash bucket length %zu hash=%zu",
                      tag_group_set_->bucket_size(long_hash),
                      long_hash);
}

void
Search::reportPathCountHistogram() const
{
  std::vector<int> vertex_counts(10);
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    TagGroup *tag_group = tagGroup(vertex);
    if (tag_group) {
      size_t path_count = tag_group->pathCount();
      if (path_count >= vertex_counts.size())
        vertex_counts.resize(path_count * 2);
      vertex_counts[path_count]++;
    }
  }

  for (size_t path_count = 0; path_count < vertex_counts.size(); path_count++) {
    int vertex_count = vertex_counts[path_count];
    if (vertex_count > 0)
      report_->reportLine("%6lu %6d",path_count, vertex_count);
  }
}

////////////////////////////////////////////////////////////////

Tag *
Search::tag(TagIndex index) const
{
  return tags_[index];
}

TagIndex
Search::tagCount() const
{
  return tag_set_->size();
}

Tag *
Search::findTag(Scene *scene,
                const RiseFall *rf,
                const MinMax *min_max,
                const ClkInfo *clk_info,
                bool is_clk,
                InputDelay *input_delay,
                bool is_segment_start,
                ExceptionStateSet *states,
                bool own_states,
                TagSet *tag_cache)
{
  Tag probe(scene, 0, rf, min_max, clk_info, is_clk,
            input_delay, is_segment_start, states, false);
  if (tag_cache) {
    Tag *tag = findKey(tag_cache, &probe);
    if (tag)
      return tag;
  }

  LockGuard lock(tag_lock_);
  Tag *tag = findKey(tag_set_, &probe);
  if (tag == nullptr) {
    // Make rise/fall versions of the tag to avoid tag_set lookups when the
    // only change is the rise/fall edge.
    for (const RiseFall *rf1 : RiseFall::range()) {
      ExceptionStateSet *new_states = !own_states && states
        ? new ExceptionStateSet(*states) : states;
      TagIndex tag_index = tag_next_++;
      Tag *tag1 = new Tag(scene, tag_index, rf1, min_max, clk_info, is_clk,
                          input_delay, is_segment_start, new_states, true);
      own_states = false;
      // Make sure tag can be indexed in tags_ before it is visible to
      // other threads via tag_set_.
      tags_[tagsTableRfIndex(tag_index, rf1)] = tag1;
      tag_set_->insert(tag1);
      if (tag_cache)
        tag_cache->insert(tag1);
      if (rf1 == rf)
        tag = tag1;

      if (tag_next_ == tag_index_max)
        report_->critical(1511, "max tag index exceeded");
    }

    // If tags_ needs to grow make the new array and copy the
    // contents into it before updating tags_ so that other threads
    // can use Search::tag(TagIndex) without returning gubbish.
    if (tag_next_ == tag_capacity_) {
      TagIndex tag_capacity = tag_capacity_ * 2;
      Tag **tags = new Tag*[tag_capacity];
      memcpy(tags, tags_, tag_capacity_ * sizeof(Tag*));
      tags_prev_.push_back(tags_);
      tags_ = tags;
      tag_capacity_ = tag_capacity;
      tag_set_->reserve(tag_capacity);
    }
  }
  if (own_states)
    delete states;
  return tag;
}

void
Search::reportTags() const
{
  for (TagIndex i = 0; i < tag_next_; i++) {
    Tag *tag = tags_[i];
    if (tag)
      report_->reportLine("%s", tag->to_string(this).c_str()) ;
  }
  size_t long_hash = 0;
  for (size_t i = 0; i < tag_set_->bucket_count(); i++) {
    if (tag_set_->bucket_size(i) > long_hash)
      long_hash = i;
  }
  report_->reportLine("Longest hash bucket length %zu hash=%zu",
                      tag_set_->bucket_size(long_hash),
                      long_hash);
}

void
Search::reportClkInfos() const
{
  std::vector<const ClkInfo*> clk_infos;
  // set -> vector for sorting.
  for (const ClkInfo *clk_info : *clk_info_set_)
    clk_infos.push_back(clk_info);
  sort(clk_infos, ClkInfoLess(this));
  for (const ClkInfo *clk_info : clk_infos)
    report_->reportLine("%s", clk_info->to_string(this).c_str());
  report_->reportLine("%zu clk infos", clk_info_set_->size());
}

const ClkInfo *
Search::findClkInfo(Scene *scene,
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
                    Path *crpr_clk_path)
{
  const ClkInfo probe(scene, clk_edge, clk_src, is_propagated, gen_clk_src,
                      gen_clk_src_path, pulse_clk_sense,
                      insertion, latency, uncertainties, min_max,
                      crpr_clk_path, this);
  LockGuard lock(clk_info_lock_);
  const ClkInfo *clk_info = findKey(clk_info_set_, &probe);
  if (clk_info == nullptr) {
    clk_info = new ClkInfo(scene, clk_edge, clk_src,
                           is_propagated, gen_clk_src, gen_clk_src_path,
                           pulse_clk_sense, insertion, latency, uncertainties,
                           min_max, crpr_clk_path, this);
    clk_info_set_->insert(clk_info);
  }
  return clk_info;
}

const ClkInfo *
Search::findClkInfo(Scene *scene,
                    const ClockEdge *clk_edge,
                    const Pin *clk_src,
                    bool is_propagated,
                    Arrival insertion,
                    const MinMax *min_max)
{
  return findClkInfo(scene, clk_edge, clk_src, is_propagated,
                     nullptr, false, nullptr,
                     insertion, 0.0, nullptr, min_max, nullptr);
}

int
Search::clkInfoCount() const
{
  return clk_info_set_->size();
}

ArcDelay
Search::deratedDelay(const Vertex *from_vertex,
                     const TimingArc *arc,
                     const Edge *edge,
                     bool is_clk,
                     const MinMax *min_max,
                     DcalcAPIndex dcalc_ap,
                     const Sdc *sdc)
{
  float derate = timingDerate(from_vertex, arc, edge, is_clk, sdc, min_max);
  ArcDelay delay = graph_->arcDelay(edge, arc, dcalc_ap);
  return delay * derate;
}

float
Search::timingDerate(const Vertex *from_vertex,
                     const TimingArc *arc,
                     const Edge *edge,
                     bool is_clk,
                     const Sdc *sdc,
                     const MinMax *min_max)
{
  PathClkOrData derate_clk_data =
    is_clk ? PathClkOrData::clk : PathClkOrData::data;
  const TimingRole *role = edge->role();
  const Pin *pin = from_vertex->pin();
  if (role->isWire()) {
    const RiseFall *rf = arc->toEdge()->asRiseFall();
    return sdc->timingDerateNet(pin, derate_clk_data, rf, min_max);
  }
  else {
    TimingDerateCellType derate_type;
    const RiseFall *rf;
    if (role->isTimingCheck()) {
      derate_type = TimingDerateCellType::cell_check;
      rf = arc->toEdge()->asRiseFall();
    }
    else {
       derate_type = TimingDerateCellType::cell_delay;
       rf = arc->fromEdge()->asRiseFall();
    }
    return sdc->timingDerateInstance(pin, derate_type, derate_clk_data,
                                     rf, min_max);
  }
}

ClockSet
Search::clockDomains(const Vertex *vertex,
                     const Mode *mode) const

{
  ClockSet clks;
  clockDomains(vertex, mode, clks);
  return clks;
}

void
Search::clockDomains(const Vertex *vertex,
                     const Mode *mode,
                     // Return value.
                     ClockSet &clks) const
{
  VertexPathIterator path_iter(const_cast<Vertex*>(vertex), this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    const Clock *clk = path->clock(this);
    if (clk && path->mode(this) == mode)
      clks.insert(const_cast<Clock *>(clk));
  }
}

ClockSet
Search::clockDomains(const Pin *pin,
                     const Mode *mode) const
{
  ClockSet clks;
  Vertex *vertex;
  Vertex *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  if (vertex)
    clockDomains(vertex, mode, clks);
  if (bidirect_drvr_vertex)
    clockDomains(bidirect_drvr_vertex, mode, clks);
  return clks;
}

ClockSet
Search::clocks(const Pin *pin,
               const Mode *mode) const
{
  ClockSet clks;
  Vertex *vertex;
  Vertex *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  if (vertex)
    clocks(vertex, mode, clks);
  if (bidirect_drvr_vertex)
    clocks(bidirect_drvr_vertex, mode, clks);
  return clks;
}

ClockSet
Search::clocks(const Vertex *vertex,
               const Mode *mode) const
{
  ClockSet clks;
  clocks(vertex, mode, clks);
  return clks;
}

void
Search::clocks(const Vertex *vertex,
               const Mode *mode,
               // Return value.
               ClockSet &clks) const
{
  VertexPathIterator path_iter(const_cast<Vertex*>(vertex), this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    if (path->isClock(this)
        && path->mode(this) == mode)
      clks.insert(const_cast<Clock *>(path->clock(this)));
  }
}

////////////////////////////////////////////////////////////////

void
Search::findRequireds()
{
  findRequireds(0);
}

void
Search::findRequireds(Level level)
{
  Stats stats(debug_, report_);
  debugPrint(debug_, "search", 1, "find requireds to level %d", level);
  RequiredVisitor req_visitor(this);
  if (!requireds_seeded_)
    seedRequireds();
  seedInvalidRequireds();
  int required_count = required_iter_->visitParallel(level, &req_visitor);
  deleteTagsPrev();
  requireds_exist_ = true;
  debugPrint(debug_, "search", 1, "found %d requireds", required_count);
  stats.report("Find requireds");
}

void
Search::seedRequireds()
{
  ensureDownstreamClkPins();
  for (Vertex *vertex : endpoints())
    seedRequired(vertex);
  requireds_seeded_ = true;
  requireds_exist_ = true;
}

VertexSet &
Search::endpoints()
{
  if (!endpoints_initialized_) {
    VertexIterator vertex_iter(graph_);
    while (vertex_iter.hasNext()) {
      Vertex *vertex = vertex_iter.next();
      if (isEndpoint(vertex)) {
        debugPrint(debug_, "endpoint", 2, "insert %s",
                   vertex->to_string(this).c_str());
        endpoints_.insert(vertex);
      }
    }
    endpoints_initialized_ = true;
  }
  if (!invalid_endpoints_.empty()) {
    for (Vertex *vertex : invalid_endpoints_) {
      if (isEndpoint(vertex)) {
        debugPrint(debug_, "endpoint", 2, "insert %s",
                   vertex->to_string(this).c_str());
        endpoints_.insert(vertex);
      }
      else {
        if (debug_->check("endpoint", 2)
            && endpoints_.contains(vertex))
          report_->reportLine("endpoint: remove %s",
                              vertex->to_string(this).c_str());
        endpoints_.erase(vertex);
      }
    }
    invalid_endpoints_.clear();
  }
  return endpoints_;
}

void
Search::endpointInvalid(Vertex *vertex)
{
  debugPrint(debug_, "endpoint", 2, "invalid %s",
             vertex->to_string(this).c_str());
  invalid_endpoints_.insert(vertex);
}

bool
Search::isEndpoint(Vertex *vertex) const
{
  for (const Mode *mode : modes_) {
    if (isEndpoint(vertex, search_thru_, mode))
      return true;
  }
  return false;
}

bool
Search::isEndpoint(Vertex *vertex,
                   const ModeSeq &modes) const
{
  for (const Mode *mode : modes) {
    if (isEndpoint(vertex, search_thru_, mode))
      return true;
  }
  return false;
}

bool
Search::isEndpoint(Vertex *vertex,
                   const Mode *mode) const
{
  return isEndpoint(vertex, search_thru_, mode);
}

bool
Search::isEndpoint(Vertex *vertex,
                   SearchPred *pred,
                   const Mode *mode) const
{
  const Pin *pin = vertex->pin();
  const Sdc *sdc = mode->sdc();
  return hasFanin(vertex, pred, graph_, mode)
    && ((vertex->hasChecks()
         && hasEnabledChecks(vertex, mode))
        || sdc->isConstrainedEnd(pin)
        || !hasFanout(vertex, pred, graph_, mode)
        || sdc->isPathDelayInternalTo(pin)
        // Unconstrained paths at register clk pins.
        || (unconstrained_paths_
            && vertex->isRegClk())
        || (variables_->gatedClkChecksEnabled()
            && gated_clk_->isGatedClkEnable(vertex, mode)));
}

bool
Search::hasEnabledChecks(Vertex *vertex,
                         const Mode *mode) const
{
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (visit_path_ends_->checkEdgeEnabled(edge, mode))
      return true;
  }
  return false;
}

void
Search::endpointsInvalid()
{
  endpoints_.clear();
  endpoints_initialized_ = false;
  invalid_endpoints_.clear();
}

void
Search::seedInvalidRequireds()
{
  for (Vertex *vertex : invalid_requireds_)
    required_iter_->enqueue(vertex);
  invalid_requireds_.clear();
}

////////////////////////////////////////////////////////////////

// Visitor used by visitPathEnds to seed end required time.
class FindEndRequiredVisitor : public PathEndVisitor
{
public:
  FindEndRequiredVisitor(RequiredCmp *required_cmp,
                         const StaState *sta);
  FindEndRequiredVisitor(const StaState *sta);
  virtual ~FindEndRequiredVisitor();
  virtual PathEndVisitor *copy() const;
  virtual void visit(PathEnd *path_end);

protected:
  const StaState *sta_;
  RequiredCmp *required_cmp_;
  bool own_required_cmp_;
};

FindEndRequiredVisitor::FindEndRequiredVisitor(RequiredCmp *required_cmp,
                                               const StaState *sta) :
  sta_(sta),
  required_cmp_(required_cmp),
  own_required_cmp_(false)
{
}

FindEndRequiredVisitor::FindEndRequiredVisitor(const StaState *sta) :
  sta_(sta),
  required_cmp_(new RequiredCmp),
  own_required_cmp_(true)
{
}

FindEndRequiredVisitor::~FindEndRequiredVisitor()
{
  if (own_required_cmp_)
    delete required_cmp_;
}

PathEndVisitor *
FindEndRequiredVisitor::copy() const
{
  return new FindEndRequiredVisitor(sta_);
}

void
FindEndRequiredVisitor::visit(PathEnd *path_end)
{
  if (!path_end->isUnconstrained()) {
    Path *path = path_end->path();
    const MinMax *min_max = path->minMax(sta_)->opposite();
    size_t path_index = path->pathIndex(sta_);
    Required required = path_end->requiredTime(sta_);
    required_cmp_->requiredSet(path_index, required, min_max, sta_);
  }
}

void
Search::seedRequired(Vertex *vertex)
{
  debugPrint(debug_, "search", 2, "required seed %s",
             vertex->to_string(this).c_str());
  RequiredCmp required_cmp;
  FindEndRequiredVisitor seeder(&required_cmp, this);
  required_cmp.requiredsInit(vertex, this);
  visit_path_ends_->visitPathEnds(vertex, &seeder);
  // Enqueue fanin vertices for back-propagating required times.
  if (required_cmp.requiredsSave(vertex, this))
    required_iter_->enqueueAdjacentVertices(vertex);
}

void
Search::seedRequiredEnqueueFanin(Vertex *vertex)
{
  RequiredCmp required_cmp;
  FindEndRequiredVisitor seeder(&required_cmp, this);
  required_cmp.requiredsInit(vertex, this);
  visit_path_ends_->visitPathEnds(vertex, &seeder);
  // Enqueue fanin vertices for back-propagating required times.
  required_cmp.requiredsSave(vertex, this);
  required_iter_->enqueueAdjacentVertices(vertex);
}

////////////////////////////////////////////////////////////////

RequiredCmp::RequiredCmp() :
  have_requireds_(false)
{
  requireds_.reserve(10);
}

void
RequiredCmp::requiredsInit(Vertex *vertex,
                           const StaState *sta)
{
  Search *search = sta->search();
  TagGroup *tag_group = search->tagGroup(vertex);
  if (tag_group) {
    size_t path_count = tag_group->pathCount();
    requireds_.resize(path_count);
    for (auto const [tag, path_index] : *tag_group->pathIndexMap()) {
      const MinMax *min_max = tag->minMax();
      requireds_[path_index] = delayInitValue(min_max->opposite());
    }
  }
  else
    requireds_.clear();
  have_requireds_ = false;
}

void
RequiredCmp::requiredSet(size_t path_index,
                         Required &required,
                         const MinMax *min_max,
                         const StaState *sta)
{
  if (delayGreater(required, requireds_[path_index], min_max, sta)) {
    requireds_[path_index] = required;
    have_requireds_ = true;
  }
}

bool
RequiredCmp::requiredsSave(Vertex *vertex,
                           const StaState *sta)
{
  bool requireds_changed = false;
  Debug *debug = sta->debug();
  VertexPathIterator path_iter(vertex, sta);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    size_t path_index = path->pathIndex(sta);
    Required req = requireds_[path_index];
    Required &prev_req = path->required();
    bool changed = !delayEqual(prev_req, req);
    debugPrint(debug, "search", 3, "required %s save %s -> %s%s",
               path->to_string(sta).c_str(),
               delayAsString(prev_req, sta),
               delayAsString(req, sta),
               changed ? " changed" : "");
    requireds_changed |= changed;
    path->setRequired(req);
  }
  return requireds_changed;
}

Required
RequiredCmp::required(size_t path_index)
{
  return requireds_[path_index];
}

////////////////////////////////////////////////////////////////

RequiredVisitor::RequiredVisitor(const StaState *sta) :
  PathVisitor(sta),
  required_cmp_(new RequiredCmp),
  visit_path_ends_(new VisitPathEnds(sta))
{
}

RequiredVisitor::RequiredVisitor(bool make_tag_cache,
                                 const StaState *sta) :
  PathVisitor(sta->search()->evalPred(), make_tag_cache, sta),
  required_cmp_(new RequiredCmp),
  visit_path_ends_(new VisitPathEnds(sta))
{
}

RequiredVisitor::~RequiredVisitor()
{
  delete required_cmp_;
  delete visit_path_ends_;
}

VertexVisitor *
RequiredVisitor::copy() const
{
  return new RequiredVisitor(true, this);
}

void
RequiredVisitor::visit(Vertex *vertex)
{
  debugPrint(debug_, "search", 2, "find required %s",
             vertex->to_string(this).c_str());
  required_cmp_->requiredsInit(vertex, this);
  // Back propagate requireds from fanout.
  visitFanoutPaths(vertex);
  // Check for constraints at endpoints that set required times.
  if (search_->isEndpoint(vertex)) {
    FindEndRequiredVisitor seeder(required_cmp_, this);
    visit_path_ends_->visitPathEnds(vertex, &seeder);
  }
  bool changed = required_cmp_->requiredsSave(vertex, this);
  search_->tnsInvalid(vertex);

  if (changed)
    search_->requiredIterator()->enqueueAdjacentVertices(vertex);
}

bool
RequiredVisitor::visitFromToPath(const Pin *,
                                 Vertex * /* from_vertex */,
                                 const RiseFall *from_rf,
                                 Tag *from_tag,
                                 Path *from_path,
                                 const Arrival &,
                                 Edge *edge,
                                 TimingArc *,
                                 ArcDelay arc_delay,
                                 Vertex *to_vertex,
                                 const RiseFall *to_rf,
                                 Tag *to_tag,
                                 Arrival &,
                                 const MinMax *min_max)
{
  // Don't propagate required times through latch D->Q edges.
  if (edge->role() != TimingRole::latchDtoQ()) {
    debugPrint(debug_, "search", 3, "  %s -> %s %s",
               from_rf->to_string().c_str(),
               to_rf->to_string().c_str(),
               min_max->to_string().c_str());
    debugPrint(debug_, "search", 3, "  from tag %2u: %s",
               from_tag->index(),
               from_tag->to_string(this).c_str());
    size_t path_index = from_path->pathIndex(this);
    const MinMax *req_min = min_max->opposite();
    TagGroup *to_tag_group = search_->tagGroup(to_vertex);
    // Check to see if to_tag was pruned.
    if (to_tag_group && to_tag_group->hasTag(to_tag)) {
      size_t to_path_index = to_tag_group->pathIndex(to_tag);
      Path &to_path = to_vertex->paths()[to_path_index];
      Required &to_required = to_path.required();
      Required from_required = to_required - arc_delay;
      debugPrint(debug_, "search", 3, "  to tag   %2u: %s",
                 to_tag->index(),
                 to_tag->to_string(this).c_str());
      debugPrint(debug_, "search", 3, "  %s - %s = %s %s %s",
                 delayAsString(to_required, this),
                 delayAsString(arc_delay, this),
                 delayAsString(from_required, this),
                 min_max == MinMax::max() ? "<" : ">",
                 delayAsString(required_cmp_->required(path_index), this));
      required_cmp_->requiredSet(path_index, from_required, req_min, this);
    }
    else {
      if (search_->crprApproxMissingRequireds()) {
        // Arrival on to_vertex that differs by crpr_pin was pruned.
        // Find an arrival that matches everything but the crpr_pin
        // as an appromate required.
        VertexPathIterator to_iter(to_vertex, from_path->scene(this),
                                   from_path->minMax(this), to_rf, this);
        while (to_iter.hasNext()) {
          Path *to_path = to_iter.next();
          Tag *to_path_tag = to_path->tag(this);
          if (Tag::matchNoCrpr(to_path_tag, to_tag)) {
            Required to_required = to_path->required();
            Required from_required = to_required - arc_delay;
            debugPrint(debug_, "search", 3, "  to tag   %2u: %s",
                       to_path_tag->index(),
                       to_path_tag->to_string(this).c_str());
            debugPrint(debug_, "search", 3, "  %s - %s = %s %s %s",
                       delayAsString(to_required, this),
                       delayAsString(arc_delay, this),
                       delayAsString(from_required, this),
                       min_max == MinMax::max() ? "<" : ">",
                       delayAsString(required_cmp_->required(path_index),
                                     this));
            required_cmp_->requiredSet(path_index, from_required, req_min, this);
            break;
          }
        }
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////

void
Search::ensureDownstreamClkPins()
{
  if (!found_downstream_clk_pins_) {
    // Use backward BFS from register clk pins to mark upsteam pins
    // as having downstream clk pins.
    ClkTreeSearchPred pred(this);
    BfsBkwdIterator iter(BfsIndex::other, &pred, this);
    for (Vertex *vertex : graph_->regClkVertices())
      iter.enqueue(vertex);

    while (iter.hasNext()) {
      Vertex *vertex = iter.next();
      vertex->setHasDownstreamClkPin(true);
      iter.enqueueAdjacentVertices(vertex);
    }
  }
  found_downstream_clk_pins_ = true;
}

////////////////////////////////////////////////////////////////

bool
Search::matchesFilter(Path *path,
                      const ClockEdge *to_clk_edge)
{
  if (!have_filter_
      && filter_from_ == nullptr
      && filter_to_ == nullptr)
    return true;
  else if (have_filter_) {
    // -from pins|inst
    // -thru
    // Path has to be tagged by traversing the filter exception points.
    ExceptionStateSet *states = path->tag(this)->states();
    if (states) {
      for (auto state : *states) {
        if (state->exception()->isFilter()
            && state->nextThru() == nullptr
            && matchesFilterTo(path, to_clk_edge))
          return true;
      }
    }
    return false;
  }
  else if (filter_from_
           && filter_from_->pins() == nullptr
           && filter_from_->instances() == nullptr
           && filter_from_->clks()) {
    // -from clks
    const ClockEdge *path_clk_edge = path->clkEdge(this);
    const Clock *path_clk = path_clk_edge ? path_clk_edge->clock() : nullptr;
    const RiseFall *path_clk_rf =
      path_clk_edge ? path_clk_edge->transition() : nullptr;
    return filter_from_->clks()->contains(const_cast<Clock*>(path_clk))
      && filter_from_->transition()->matches(path_clk_rf)
      && matchesFilterTo(path, to_clk_edge);
  }
  else if (filter_from_ == nullptr
           && filter_to_)
    // -to
    return matchesFilterTo(path, to_clk_edge);
  else {
    report_->critical(1512, "unexpected filter path");
    return false;
  }
}

// Similar to Sdf::exceptionMatchesTo.
bool
Search::matchesFilterTo(Path *path,
                        const ClockEdge *to_clk_edge) const
{
  return (filter_to_ == nullptr
          || filter_to_->matchesFilter(path->pin(graph_), to_clk_edge,
                                       path->transition(this), network_));
}

////////////////////////////////////////////////////////////////

// Find the exception that has the highest priority for an end path,
// including exceptions that start at the end pin or target clock.
ExceptionPath *
Search::exceptionTo(ExceptionPathType type,
                    const Path *path,
                    const Pin *pin,
                    const RiseFall *rf,
                    const ClockEdge *clk_edge,
                    const MinMax *min_max,
                    bool match_min_max_exactly,
                    bool require_to_pin,
                    Sdc *sdc) const
{
  // Find the highest priority exception carried by the path's tag.
  int hi_priority = -1;
  ExceptionPath *hi_priority_exception = nullptr;
  const ExceptionStateSet *states = path->tag(this)->states();
  if (states) {
    for (auto state : *states) {
      ExceptionPath *exception = state->exception();
      int priority = exception->priority(min_max);
      if ((type == ExceptionPathType::any
           || exception->type() == type)
          && sdc->isCompleteTo(state, pin, rf, clk_edge, min_max,
                               match_min_max_exactly, require_to_pin)
          && (hi_priority_exception == nullptr
              || priority > hi_priority
              || (priority == hi_priority
                  && exception->tighterThan(hi_priority_exception)))) {
        hi_priority = priority;
        hi_priority_exception = exception;
      }
    }
  }
  // Check for -to exceptions originating at the end pin or target clock.
  sdc->exceptionTo(type, pin, rf, clk_edge, min_max,
                   match_min_max_exactly,
                   hi_priority_exception, hi_priority);
  return hi_priority_exception;
}

////////////////////////////////////////////////////////////////

// Find group paths that end at the path.
ExceptionPathSeq
Search::groupPathsTo(const PathEnd *path_end) const
{
  ExceptionPathSeq group_paths;
  const Path *path = path_end->path();
  const Pin *pin = path->pin(this);
  const Tag *tag = path->tag(this);
  Sdc *sdc = tag->scene()->sdc();
  const RiseFall *rf = tag->transition();
  const ClockEdge *clk_edge = path_end->targetClkEdge(this);
  const MinMax *min_max = tag->minMax();
  const ExceptionStateSet *states = path->tag(this)->states();
  if (states) {
    for (auto state : *states) {
      ExceptionPath *exception = state->exception();
      if (exception->isGroupPath()
          && sdc->exceptionMatchesTo(exception, pin, rf, clk_edge, min_max,
                                     false, false))
        group_paths.push_back(exception);
    }
  }
  // Check for group_path -to exceptions originating at the end pin or target clock.
  sdc->groupPathsTo(pin, rf, clk_edge, min_max, group_paths);
  return group_paths;
}

////////////////////////////////////////////////////////////////

Slack
Search::totalNegativeSlack(const MinMax *min_max)
{
  tnsPreamble();
  Slack tns = 0.0;
  for (Scene *scene : scenes_) {
    size_t path_index = scene->pathIndex(min_max);
    Slack tns1 = tns_[path_index];
    if (delayLess(tns1, tns, this))
      tns = tns1;
  }
  return tns;
}

Slack
Search::totalNegativeSlack(const Scene *scene,
                           const MinMax *min_max)
{
  tnsPreamble();
  PathAPIndex path_ap_index = scene->pathIndex(min_max);
  return tns_[path_ap_index];
}

void
Search::tnsPreamble()
{
  wnsTnsPreamble();
  size_t path_count = scenePathCount();
  tns_.resize(path_count);
  tns_slacks_.resize(path_count);
  if (tns_exists_)
    updateInvalidTns();
  else
    findTotalNegativeSlacks();
}

void
Search::tnsInvalid(Vertex *vertex)
{
  if ((tns_exists_ || worst_slacks_)
      && isEndpoint(vertex)) {
    debugPrint(debug_, "tns", 2, "tns invalid %s",
               vertex->to_string(this).c_str());
    LockGuard lock(tns_lock_);
    invalid_tns_.insert(vertex);
  }
}

void
Search::updateInvalidTns()
{
  size_t path_count = scenePathCount();
  for (Vertex *vertex : invalid_tns_) {
    // Network edits can change endpointedness since tnsInvalid was called.
    if (isEndpoint(vertex)) {
      debugPrint(debug_, "tns", 2, "update tns %s",
                 vertex->to_string(this).c_str());
      SlackSeq slacks(path_count);
      wnsSlacks(vertex, slacks);

      if (tns_exists_)
        updateTns(vertex, slacks);
      if (worst_slacks_)
        worst_slacks_->updateWorstSlacks(vertex, slacks);
    }
  }
  invalid_tns_.clear();
}

void
Search::findTotalNegativeSlacks()
{
  size_t path_count = scenePathCount();
  for (size_t i = 0; i < path_count; i++) {
    tns_[i] = 0.0;
    tns_slacks_[i].clear();
  }
  for (Vertex *vertex : endpoints()) {
    // No locking required.
    SlackSeq slacks(path_count);
    wnsSlacks(vertex, slacks);
    for (size_t i = 0; i < path_count; i++)
      tnsIncr(vertex, slacks[i], i);
  }
  tns_exists_ = true;
}

void
Search::updateTns(Vertex *vertex,
                  SlackSeq &slacks)
{
  size_t path_count = scenePathCount();
  for (size_t i = 0; i < path_count; i++) {
    tnsDecr(vertex, i);
    tnsIncr(vertex, slacks[i], i);
  }
}

void
Search::tnsIncr(Vertex *vertex,
                Slack slack,
                PathAPIndex path_ap_index)
{
  if (delayLess(slack, 0.0, this)) {
    debugPrint(debug_, "tns", 3, "tns+ %s %s",
               delayAsString(slack, this),
               vertex->to_string(this).c_str());
    tns_[path_ap_index] += slack;
    if (tns_slacks_[path_ap_index].contains(vertex))
      report_->critical(1513, "tns incr existing vertex");
    tns_slacks_[path_ap_index][vertex] = slack;
  }
}

void
Search::tnsDecr(Vertex *vertex,
                PathAPIndex path_ap_index)
{
  Slack slack;
  bool found;
  findKeyValue(tns_slacks_[path_ap_index], vertex, slack, found);
  if (found
      && delayLess(slack, 0.0, this)) {
    debugPrint(debug_, "tns", 3, "tns- %s %s",
               delayAsString(slack, this),
               vertex->to_string(this).c_str());
    tns_[path_ap_index] -= slack;
    tns_slacks_[path_ap_index].erase(vertex);
  }
}

// Notify tns before updating/deleting slack (arrival/required).
void
Search::tnsNotifyBefore(Vertex *vertex)
{
  if (tns_exists_
      && isEndpoint(vertex)) {
    size_t path_count = scenePathCount();
    for (size_t i = 0; i < path_count; i++) {
      tnsDecr(vertex, i);
    }
  }
}

////////////////////////////////////////////////////////////////

void
Search::worstSlack(const MinMax *min_max,
                   // Return values.
                   Slack &worst_slack,
                   Vertex *&worst_vertex)
{
  worstSlackPreamble();
  worst_slacks_->worstSlack(min_max, worst_slack, worst_vertex);
}

void
Search::worstSlack(const Scene *scene,
                   const MinMax *min_max,
                   // Return values.
                   Slack &worst_slack,
                   Vertex *&worst_vertex)
{
  worstSlackPreamble();
  worst_slacks_->worstSlack(scene, min_max, worst_slack, worst_vertex);
}

void
Search::worstSlackPreamble()
{
  wnsTnsPreamble();
  if (worst_slacks_)
    updateInvalidTns();
  else
    worst_slacks_ = new WorstSlacks(this);
}

void
Search::wnsTnsPreamble()
{
  findAllArrivals();
  // Required times are only needed at endpoints.
  if (requireds_seeded_) {
    for (auto itr = invalid_requireds_.begin(); itr != invalid_requireds_.end(); ) {
      Vertex *vertex = *itr;
      debugPrint(debug_, "search", 2, "tns update required %s",
                 vertex->to_string(this).c_str());
      if (isEndpoint(vertex)) {
        seedRequired(vertex);
        // If the endpoint has fanout it's required time
        // depends on downstream checks, so enqueue it to
        // force required propagation to it's level if
        // the required time is requested later.
        if (vertex->hasFanout())
          required_iter_->enqueue(vertex);
        itr = invalid_requireds_.erase(itr);
      }
      else
        itr++;
    }
  }
  else
    seedRequireds();
}

void
Search::clearWorstSlack()
{
  if (worst_slacks_) {
    // Don't maintain incremental worst slacks until there is a request.
    delete worst_slacks_;
    worst_slacks_ = nullptr;
  }
}

////////////////////////////////////////////////////////////////

class FindEndSlackVisitor : public PathEndVisitor
{
public:
  FindEndSlackVisitor(SlackSeq &slacks,
                      const StaState *sta);
  FindEndSlackVisitor(const FindEndSlackVisitor &) = default;
  virtual PathEndVisitor *copy() const;
  virtual void visit(PathEnd *path_end);

protected:
  SlackSeq &slacks_;
  const StaState *sta_;
};

FindEndSlackVisitor::FindEndSlackVisitor(SlackSeq &slacks,
                                         const StaState *sta) :
  slacks_(slacks),
  sta_(sta)
{
}

PathEndVisitor *
FindEndSlackVisitor::copy() const
{
  return new FindEndSlackVisitor(*this);
}

void
FindEndSlackVisitor::visit(PathEnd *path_end)
{
  if (!path_end->isUnconstrained()) {
    Path *path = path_end->path();
    PathAPIndex path_ap_index = path->pathAnalysisPtIndex(sta_);
    Slack slack = path_end->slack(sta_);
    if (delayLess(slack, slacks_[path_ap_index], sta_))
      slacks_[path_ap_index] = slack;
  }
}

void
Search::wnsSlacks(Vertex *vertex,
                  // Return values.
                  SlackSeq &slacks)
{
  Slack slack_init = MinMax::min()->initValue();
  size_t path_count = scenePathCount();
  for (size_t i = 0; i < path_count; i++)
    slacks[i] = slack_init;
  if (vertex->hasFanout()) {
    // If the vertex has fanout the path slacks include downstream
    // PathEnd slacks so find the endpoint slack directly.
    FindEndSlackVisitor end_visitor(slacks, this);
    visit_path_ends_->visitPathEnds(vertex, &end_visitor);
  }
  else {
    VertexPathIterator path_iter(vertex, this);
    while (path_iter.hasNext()) {
      Path *path = path_iter.next();
      PathAPIndex path_ap_index = path->pathAnalysisPtIndex(this);
      const Slack path_slack = path->slack(this);
      if (!path->tag(this)->isFilter()
          && delayLess(path_slack, slacks[path_ap_index], this))
        slacks[path_ap_index] = path_slack;
    }
  }
}

Slack
Search::wnsSlack(Vertex *vertex,
                 PathAPIndex path_ap_index)
{
  size_t path_count = scenePathCount();
  SlackSeq slacks(path_count);
  wnsSlacks(vertex, slacks);
  return slacks[path_ap_index];
}

} // namespace
