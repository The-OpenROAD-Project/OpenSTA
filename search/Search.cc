// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include "Search.hh"

#include <algorithm>
#include <cmath> // abs

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
#include "DcalcAnalysisPt.hh"
#include "SearchPred.hh"
#include "Levelize.hh"
#include "Bfs.hh"
#include "Corner.hh"
#include "Sim.hh"
#include "PathVertex.hh"
#include "PathVertexRep.hh"
#include "PathRef.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "PathEnd.hh"
#include "PathGroup.hh"
#include "PathAnalysisPt.hh"
#include "VisitPathEnds.hh"
#include "GatedClk.hh"
#include "WorstSlack.hh"
#include "Latches.hh"
#include "Crpr.hh"
#include "Genclks.hh"

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
EvalPred::searchThru(Edge *edge)
{
  const Sdc *sdc = sta_->sdc();
  TimingRole *role = edge->role();
  return SearchPred0::searchThru(edge)
    && (sdc->dynamicLoopBreaking()
	|| !edge->isDisabledLoop())
    && !role->isTimingCheck()
    && (search_thru_latches_
	|| role != TimingRole::latchDtoQ()
	|| sta_->latches()->latchDtoQState(edge) == LatchEnableState::open);
}

bool
EvalPred::searchTo(const Vertex *to_vertex)
{
  const Sdc *sdc = sta_->sdc();
  const Pin *pin = to_vertex->pin();
  return SearchPred0::searchTo(to_vertex)
    && !(sdc->isLeafPinClock(pin)
	 && !sdc->isPathDelayInternalEndpoint(pin));
}

////////////////////////////////////////////////////////////////

DynLoopSrchPred::DynLoopSrchPred(TagGroupBldr *tag_bldr) :
  tag_bldr_(tag_bldr)
{
}

bool
DynLoopSrchPred::loopEnabled(Edge *edge,
			     const Sdc *sdc,
			     const Graph *graph,
			     Search *search)
{
  return !edge->isDisabledLoop()
    || (sdc->dynamicLoopBreaking()
	&& hasPendingLoopPaths(edge, graph, search));
}

bool
DynLoopSrchPred::hasPendingLoopPaths(Edge *edge,
				     const Graph *graph,
				     Search *search)
{
  if (tag_bldr_
      && tag_bldr_->hasLoopTag()) {
    Corners *corners = search->corners();
    Vertex *from_vertex = edge->from(graph);
    TagGroup *prev_tag_group = search->tagGroup(from_vertex);
    ArrivalMap::Iterator arrival_iter(tag_bldr_->arrivalMap());
    while (arrival_iter.hasNext()) {
      Tag *from_tag;
      int arrival_index;
      arrival_iter.next(from_tag, arrival_index);
      if (from_tag->isLoop()) {
	// Loop false path exceptions apply to rise/fall edges so to_rf
	// does not matter.
	PathAPIndex path_ap_index = from_tag->pathAPIndex();
	PathAnalysisPt *path_ap = corners->findPathAnalysisPt(path_ap_index);
	Tag *to_tag = search->thruTag(from_tag, edge, RiseFall::rise(),
				      path_ap->pathMinMax(), path_ap);
	if (to_tag
	    && (prev_tag_group == nullptr
		|| !prev_tag_group->hasTag(from_tag)))
	  return true;
      }
    }
  }
  return false;
}

// EvalPred unless
//  latch D->Q edge
class SearchThru : public EvalPred, public DynLoopSrchPred
{
public:
  SearchThru(TagGroupBldr *tag_bldr,
	     const StaState *sta);
  virtual bool searchThru(Edge *edge);
};

SearchThru::SearchThru(TagGroupBldr *tag_bldr,
		       const StaState *sta) :
  EvalPred(sta),
  DynLoopSrchPred(tag_bldr)
{
}

bool
SearchThru::searchThru(Edge *edge)
{
  const Graph *graph = sta_->graph();
  const Sdc *sdc = sta_->sdc();
  Search *search = sta_->search();
  return EvalPred::searchThru(edge)
    // Only search thru latch D->Q if it is always open.
    // Enqueue thru latches is handled explicitly by search.
    && (edge->role() != TimingRole::latchDtoQ()
	|| sta_->latches()->latchDtoQState(edge) == LatchEnableState::open)
    && loopEnabled(edge, sdc, graph, search);
}

ClkArrivalSearchPred::ClkArrivalSearchPred(const StaState *sta) :
  EvalPred(sta)
{
}

bool
ClkArrivalSearchPred::searchThru(Edge *edge)
{
  const TimingRole *role = edge->role();
  return (role->isWire()
	  || role == TimingRole::combinational())
    && EvalPred::searchThru(edge);
}

////////////////////////////////////////////////////////////////

Search::Search(StaState *sta) :
  StaState(sta)
{
  init(sta);
}

void
Search::init(StaState *sta)
{
  initVars();

  search_adj_ = new SearchThru(nullptr, sta);
  eval_pred_ = new EvalPred(sta);
  check_crpr_ = new CheckCrpr(sta);
  genclks_ = new Genclks(sta);
  arrival_visitor_ = new ArrivalVisitor(sta);
  clk_arrivals_valid_ = false;
  arrivals_exist_ = false;
  arrivals_at_endpoints_exist_ = false;
  arrivals_seeded_ = false;
  requireds_exist_ = false;
  requireds_seeded_ = false;
  invalid_arrivals_ = new VertexSet(graph_);
  invalid_requireds_ = new VertexSet(graph_);
  invalid_tns_ = new VertexSet(graph_);
  tns_exists_ = false;
  worst_slacks_ = nullptr;
  arrival_iter_ = new BfsFwdIterator(BfsIndex::arrival, nullptr, sta);
  required_iter_ = new BfsBkwdIterator(BfsIndex::required, search_adj_, sta);
  tag_capacity_ = 127;
  tag_set_ = new TagSet(tag_capacity_);
  clk_info_set_ = new ClkInfoSet(ClkInfoLess(sta));
  tag_next_ = 0;
  tags_ = new Tag*[tag_capacity_];
  tag_group_capacity_ = 127;
  tag_groups_ = new TagGroup*[tag_group_capacity_];
  tag_group_next_ = 0;
  tag_group_set_ = new TagGroupSet(tag_group_capacity_);
  pending_latch_outputs_ = new VertexSet(graph_);
  visit_path_ends_ = new VisitPathEnds(this);
  gated_clk_ = new GatedClk(this);
  path_groups_ = nullptr;
  endpoints_ = nullptr;
  invalid_endpoints_ = nullptr;
  filter_ = nullptr;
  filter_from_ = nullptr;
  filter_to_ = nullptr;
  filtered_arrivals_ = new VertexSet(graph_);
  found_downstream_clk_pins_ = false;
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
  deletePaths();
  deleteTags();
  delete tag_set_;
  delete clk_info_set_;
  delete [] tags_;
  delete [] tag_groups_;
  delete tag_group_set_;
  delete search_adj_;
  delete eval_pred_;
  delete arrival_visitor_;
  delete arrival_iter_;
  delete required_iter_;
  delete endpoints_;
  delete invalid_arrivals_;
  delete invalid_requireds_;
  delete invalid_tns_;
  delete invalid_endpoints_;
  delete pending_latch_outputs_;
  delete visit_path_ends_;
  delete gated_clk_;
  delete worst_slacks_;
  delete check_crpr_;
  delete genclks_;
  delete filtered_arrivals_;
  deleteFilter();
  deletePathGroups();
}

void
Search::clear()
{
  initVars();

  clk_arrivals_valid_ = false;
  arrivals_at_endpoints_exist_ = false;
  arrivals_seeded_ = false;
  requireds_exist_ = false;
  requireds_seeded_ = false;
  tns_exists_ = false;
  clearWorstSlack();
  invalid_arrivals_->clear();
  arrival_iter_->clear();
  invalid_requireds_->clear();
  invalid_tns_->clear();
  required_iter_->clear();
  endpointsInvalid();
  deletePathGroups();
  deletePaths();
  deleteTags();
  clearPendingLatchOutputs();
  deleteFilter();
  genclks_->clear();
  found_downstream_clk_pins_ = false;
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
  tag_set_->deleteContentsClear();
  tag_free_indices_.clear();

  clk_info_set_->deleteContentsClear();
}

void
Search::deleteFilter()
{
  if (filter_) {
    sdc_->deleteException(filter_);
    filter_ = nullptr;
    filter_from_ = nullptr;
  }
  else {
    // Filter owns filter_from_ if it exists.
    delete filter_from_;
    filter_from_ = nullptr;
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
  visit_path_ends_->copyState(sta);
  gated_clk_->copyState(sta);
  check_crpr_->copyState(sta);
  genclks_->copyState(sta);
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
      vertex->deletePaths();
    }
    filtered_arrivals_->clear();
    graph_->clearArrivals();
    graph_->clearPrevPaths();
    arrivals_exist_ = false;
  }
}

void
Search::deletePaths(Vertex *vertex)
{
  tnsNotifyBefore(vertex);
  if (worst_slacks_)
    worst_slacks_->worstSlackNotifyBefore(vertex);
  vertex->deletePaths();
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
		     bool clk_gating_hold)
{
  findFilteredArrivals(from, thrus, to, unconstrained, true);
  if (!sdc_->recoveryRemovalChecksEnabled())
    recovery = removal = false;
  if (!sdc_->gatedClkChecksEnabled())
    clk_gating_setup = clk_gating_hold = false;
  path_groups_ = makePathGroups(group_count, endpoint_count, unique_pins,
				slack_min, slack_max,
				group_names, setup, hold,
				recovery, removal,
				clk_gating_setup, clk_gating_hold);
  ensureDownstreamClkPins();
  PathEndSeq path_ends = path_groups_->makePathEnds(to, unconstrained_paths_,
                                                    corner, min_max,
                                                    sort_by_slack);
  sdc_->reportClkToClkMaxCycleWarnings();
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
  // Delete results from last findPathEnds.
  // Filtered arrivals are deleted by Sta::searchPreamble.
  deletePathGroups();
  checkFromThrusTo(from, thrus, to);
  filter_from_ = from;
  filter_to_ = to;
  if ((from
       && (from->pins()
	   || from->instances()))
      || thrus) {
    filter_ = sdc_->makeFilterPath(from, thrus, nullptr);
    findFilteredArrivals(thru_latches);
  }
  else
    // These cases do not require filtered arrivals.
    //  -from clocks
    //  -to
    findAllArrivals(thru_latches);
}

// From/thrus/to are used to make a filter exception.  If the last
// search used a filter arrival/required times were only found for a
// subset of the paths.  Delete the paths that have a filter
// exception state.
void
Search::deleteFilteredArrivals()
{
  if (filter_) {
    ExceptionFrom *from = filter_->from();
    ExceptionThruSeq *thrus = filter_->thrus();
    if ((from
	 && (from->pins()
	     || from->instances()))
	|| thrus) {
      for (Vertex *vertex : *filtered_arrivals_) {
        deletePaths(vertex);
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
            filtered_arrivals_->erase(vertex);
        }
        if (!filtered_arrivals_->empty()) {
          report_->reportLine("Filtered verticies mismatch");
          for (Vertex *vertex : *filtered_arrivals_)
            report_->reportLine(" %s", vertex->name(network_));
        }
      }
      filtered_arrivals_->clear();
      deleteFilterTagGroups();
      deleteFilterClkInfos();
      deleteFilterTags();
    }
    deleteFilter();
  }
}

void
Search::deleteFilterTagGroups()
{
  for (TagGroupIndex i = 0; i < tag_group_next_; i++) {
    TagGroup *group = tag_groups_[i];
    if (group
	&& group->hasFilterTag()) {
      tag_group_set_->erase(group);
      tag_groups_[group->index()] = nullptr;
      tag_group_free_indices_.push_back(i);
      delete group;
    }
  }
}

void
Search::deleteFilterTags()
{
  for (TagIndex i = 0; i < tag_next_; i++) {
    Tag *tag = tags_[i];
    if (tag
	&& tag->isFilter()) {
      tags_[i] = nullptr;
      tag_set_->erase(tag);
      delete tag;
      tag_free_indices_.push_back(i);
    }
  }
}

void
Search::deleteFilterClkInfos()
{
  ClkInfoSet::Iterator clk_info_iter(clk_info_set_);
  while (clk_info_iter.hasNext()) {
    ClkInfo *clk_info = clk_info_iter.next();
    if (clk_info->refsFilter(this)) {
      clk_info_set_->erase(clk_info);
      delete clk_info;
    }
  }
}

void
Search::findFilteredArrivals(bool thru_latches)
{
  filtered_arrivals_->clear();
  findArrivalsSeed();
  seedFilterStarts();
  Level max_level = levelize_->maxLevel();
  // Search always_to_endpoint to search from exisiting arrivals at
  // fanin startpoints to reach -thru/-to endpoints.
  arrival_visitor_->init(true);
  // Iterate until data arrivals at all latches stop changing.
  for (int pass = 1; pass == 1 || (thru_latches && havePendingLatchOutputs()) ; pass++) {
    if (thru_latches)
      enqueuePendingLatchOutputs();
    debugPrint(debug_, "search", 1, "find arrivals pass %d", pass);
    int arrival_count = arrival_iter_->visitParallel(max_level,
						     arrival_visitor_);
    debugPrint(debug_, "search", 1, "found %d arrivals", arrival_count);
  }
  arrivals_exist_ = true;
}

VertexSeq
Search::filteredEndpoints()
{
  VertexSeq ends;
  for (Vertex *vertex : *filtered_arrivals_) {
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
  search_->seedArrival(vertex);
  if (bidirect_drvr_vertex)
    search_->seedArrival(bidirect_drvr_vertex);
}

void
Search::seedFilterStarts()
{
  ExceptionPt *first_pt = filter_->firstPt();
  if (first_pt) {
    PinSet first_pins(network_);
    first_pt->allPins(network_, &first_pins);
    for (const Pin *pin : first_pins) {
      if (network_->isHierarchical(pin)) {
        SeedFaninsThruHierPin visitor(graph_, this);
        visitDrvrLoadsThruHierPin(pin, network_, &visitor);
      }
      else {
        Vertex *vertex, *bidirect_drvr_vertex;
        graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
        if (vertex)
          seedArrival(vertex);
        if (bidirect_drvr_vertex)
          seedArrival(bidirect_drvr_vertex);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
Search::deleteVertexBefore(Vertex *vertex)
{
  if (arrivals_exist_) {
    deletePaths(vertex);
    arrival_iter_->deleteVertexBefore(vertex);
    invalid_arrivals_->erase(vertex);
    filtered_arrivals_->erase(vertex);
  }
  if (requireds_exist_) {
    required_iter_->deleteVertexBefore(vertex);
    invalid_requireds_->erase(vertex);
    invalid_tns_->erase(vertex);
  }
  if (endpoints_)
    endpoints_->erase(vertex);
  if (invalid_endpoints_)
    invalid_endpoints_->erase(vertex);
}

bool
Search::arrivalsValid()
{
  return arrivals_exist_
    && invalid_arrivals_->empty();
}

void
Search::arrivalsInvalid()
{
  if (arrivals_exist_) {
    debugPrint(debug_, "search", 1, "arrivals invalid");
    // Delete paths to make sure no state is left over.
    // For example, set_disable_timing strands a vertex, which means
    // the search won't revisit it to clear the previous arrival.
    deletePaths();
    deleteTags();
    genclks_->clear();
    deleteFilter();
    arrivals_at_endpoints_exist_ = false;
    arrivals_seeded_ = false;
    requireds_exist_ = false;
    requireds_seeded_ = false;
    clk_arrivals_valid_ = false;
    arrival_iter_->clear();
    required_iter_->clear();
    // No need to keep track of incremental updates any more.
    invalid_arrivals_->clear();
    invalid_requireds_->clear();
    tns_exists_ = false;
    clearWorstSlack();
    invalid_tns_->clear();
  }
}

void
Search::requiredsInvalid()
{
  debugPrint(debug_, "search", 1, "requireds invalid");
  requireds_exist_ = false;
  requireds_seeded_ = false;
  invalid_requireds_->clear();
  tns_exists_ = false;
  clearWorstSlack();
  invalid_tns_->clear();
}

void
Search::arrivalInvalid(Vertex *vertex)
{
  if (arrivals_exist_) {
    debugPrint(debug_, "search", 2, "arrival invalid %s",
               vertex->name(sdc_network_));
    if (!arrival_iter_->inQueue(vertex)) {
      // Lock for StaDelayCalcObserver called by delay calc threads.
      UniqueLock lock(invalid_arrivals_lock_);
      invalid_arrivals_->insert(vertex);
    }
    tnsInvalid(vertex);
  }
}

void
Search::arrivalInvalidDelete(Vertex *vertex)
{
  arrivalInvalid(vertex);
  vertex->deletePaths();
}

void
Search::levelChangedBefore(Vertex *vertex)
{
  if (arrivals_exist_) {
    arrival_iter_->remove(vertex);
    required_iter_->remove(vertex);
    search_->arrivalInvalid(vertex);
    search_->requiredInvalid(vertex);
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
               vertex->name(sdc_network_));
    if (!required_iter_->inQueue(vertex)) {
      // Lock for StaDelayCalcObserver called by delay calc threads.
      UniqueLock lock(invalid_arrivals_lock_);
      invalid_requireds_->insert(vertex);
    }
    tnsInvalid(vertex);
  }
}

////////////////////////////////////////////////////////////////

void
Search::findClkArrivals()
{
  if (!clk_arrivals_valid_) {
    genclks_->ensureInsertionDelays();
    Stats stats(debug_, report_);
    debugPrint(debug_, "search", 1, "find clk arrivals");
    arrival_iter_->clear();
    seedClkVertexArrivals();
    ClkArrivalSearchPred search_clk(this);
    arrival_visitor_->init(false, &search_clk);
    arrival_iter_->visitParallel(levelize_->maxLevel(), arrival_visitor_);
    arrivals_exist_ = true;
    stats.report("Find clk arrivals");
  }
  clk_arrivals_valid_ = true;
}

void
Search::seedClkVertexArrivals()
{
  PinSet clk_pins(network_);
  findClkVertexPins(clk_pins);
  for (const Pin *pin : clk_pins) {
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    seedClkVertexArrivals(pin, vertex);
    if (bidirect_drvr_vertex)
      seedClkVertexArrivals(pin, bidirect_drvr_vertex);
  }
}

void
Search::seedClkVertexArrivals(const Pin *pin,
			      Vertex *vertex)
{
  TagGroupBldr tag_bldr(true, this);
  tag_bldr.init(vertex);
  genclks_->copyGenClkSrcPaths(vertex, &tag_bldr);
  seedClkArrivals(pin, vertex, &tag_bldr);
  setVertexArrivals(vertex, &tag_bldr);
}

Arrival
Search::clockInsertion(const Clock *clk,
		       const Pin *pin,
		       const RiseFall *rf,
		       const MinMax *min_max,
		       const EarlyLate *early_late,
		       const PathAnalysisPt *path_ap) const
{
  float insert;
  bool exists;
  sdc_->clockInsertion(clk, pin, rf, min_max, early_late, insert, exists);
  if (exists)
    return insert;
  else if (clk->isGeneratedWithPropagatedMaster())
    return genclks_->insertionDelay(clk, pin, rf, early_late, path_ap);
  else
    return 0.0;
}

////////////////////////////////////////////////////////////////

void
Search::visitStartpoints(VertexVisitor *visitor)
{
  Instance *top_inst = network_->topInstance();
  InstancePinIterator *pin_iter = network_->pinIterator(top_inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyInput()) {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      visitor->visit(vertex);
    }
  }
  delete pin_iter;

  for (auto iter : sdc_->inputDelayPinMap()) {
    const Pin *pin = iter.first;
    // Already hit these.
    if (!network_->isTopLevelPort(pin)) {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      if (vertex)
	visitor->visit(vertex);
    }
  }

  for (const Clock *clk : sdc_->clks()) {
    for (const Pin *pin : clk->leafPins()) {
      // Already hit these.
      if (!network_->isTopLevelPort(pin)) {
	Vertex *vertex = graph_->pinDrvrVertex(pin);
	visitor->visit(vertex);
      }
    }
  }

  // Register clk pins.
  for (Vertex *vertex : *graph_->regClkVertices())
    visitor->visit(vertex);

  const PinSet &startpoints = sdc_->pathDelayInternalStartpoints();
  if (!startpoints.empty()) {
    for (const Pin *pin : startpoints) {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      visitor->visit(vertex);
    }
  }
}

void
Search::visitEndpoints(VertexVisitor *visitor)
{
  for (Vertex *end : *endpoints()) {
    Pin *pin = end->pin();
    // Filter register clock pins (fails on set_max_delay -from clk_src).
    if (!network_->isRegClkPin(pin)
	|| sdc_->isPathDelayInternalEndpoint(pin))
      visitor->visit(end);
  }
}

////////////////////////////////////////////////////////////////

void
Search::findAllArrivals()
{
  findAllArrivals(true);
}

void
Search::findAllArrivals(bool thru_latches)
{
  arrival_visitor_->init(false);
  // Iterate until data arrivals at all latches stop changing.
  for (int pass = 1; pass == 1 || (thru_latches && havePendingLatchOutputs()); pass++) {
    enqueuePendingLatchOutputs();
    debugPrint(debug_, "search", 1, "find arrivals pass %d", pass);
    findArrivals1(levelize_->maxLevel());
  }
}

bool
Search::havePendingLatchOutputs()
{
  return !pending_latch_outputs_->empty();
}

void
Search::clearPendingLatchOutputs()
{
  pending_latch_outputs_->clear();
}

void
Search::enqueuePendingLatchOutputs()
{
  for (Vertex *latch_vertex : *pending_latch_outputs_)
    arrival_iter_->enqueue(latch_vertex);
  clearPendingLatchOutputs();
}

void
Search::findArrivals()
{
  findArrivals(levelize_->maxLevel());
}

void
Search::findArrivals(Level level)
{
  arrival_visitor_->init(false);
  findArrivals1(level);
}

void
Search::findArrivals1(Level level)
{
  debugPrint(debug_, "search", 1, "find arrivals to level %d", level);
  findArrivalsSeed();
  Stats stats(debug_, report_);
  int arrival_count = arrival_iter_->visitParallel(level, arrival_visitor_);
  stats.report("Find arrivals");
  if (arrival_iter_->empty()
      && invalid_arrivals_->empty()) {
    clk_arrivals_valid_ = true;
    arrivals_at_endpoints_exist_ = true;
  }
  arrivals_exist_ = true;
  debugPrint(debug_, "search", 1, "found %u arrivals", arrival_count);
}

void
Search::findArrivalsSeed()
{
  if (!arrivals_seeded_) {
    genclks_->ensureInsertionDelays();
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
  PathVisitor(nullptr, sta)
{
  init0();
  init(true);
}

// Copy constructor.
ArrivalVisitor::ArrivalVisitor(bool always_to_endpoints,
			       SearchPred *pred,
			       const StaState *sta) :
  PathVisitor(pred, sta)
{
  init0();
  init(always_to_endpoints, pred);
}

void
ArrivalVisitor::init0()
{
  tag_bldr_ = new TagGroupBldr(true, this);
  tag_bldr_no_crpr_ = new TagGroupBldr(false, this);
  adj_pred_ = new SearchThru(tag_bldr_, this);
}

void
ArrivalVisitor::init(bool always_to_endpoints)
{
  init(always_to_endpoints, search_ ? search_->evalPred() : nullptr);
}

void
ArrivalVisitor::init(bool always_to_endpoints,
		     SearchPred *pred)
{
  always_to_endpoints_ = always_to_endpoints;
  pred_ = pred;
  crpr_active_ = sdc_->crprActive();
}


VertexVisitor *
ArrivalVisitor::copy() const
{
  return new ArrivalVisitor(always_to_endpoints_, pred_, this);
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
             vertex->name(sdc_network_));
  Pin *pin = vertex->pin();
  tag_bldr_->init(vertex);
  has_fanin_one_ = graph_->hasFaninOne(vertex);
  if (crpr_active_
      && !has_fanin_one_)
    tag_bldr_no_crpr_->init(vertex);

  visitFaninPaths(vertex);
  if (crpr_active_
      && search_->crprPathPruningEnabled()
      && !vertex->crprPathPruningDisabled()
      && !has_fanin_one_)
    pruneCrprArrivals();

  // Insert paths that originate here.
  if (!network_->isTopLevelPort(pin)
      && sdc_->hasInputDelay(pin))
    // set_input_delay on internal pin.
    search_->seedInputSegmentArrival(pin, vertex, tag_bldr_);
  if (sdc_->isPathDelayInternalStartpoint(pin))
    // set_min/max_delay -from internal pin.
    search_->makeUnclkedPaths(vertex, false, true, tag_bldr_);
  if (sdc_->isLeafPinClock(pin))
    // set_min/max_delay -to internal pin also a clock src. Bizzaroland.
    // Re-seed the clock arrivals on top of the propagated paths.
    search_->seedClkArrivals(pin, vertex, tag_bldr_);
  // Register/latch clock pin that is not connected to a declared clock.
  // Seed with unclocked tag, zero arrival and allow search thru reg
  // clk->q edges.
  // These paths are required to report path delays from unclocked registers
  // For example, "set_max_delay -to" from an unclocked source register.
  bool is_clk = tag_bldr_->hasClkTag();
  if (vertex->isRegClk() && !is_clk) {
    debugPrint(debug_, "search", 2, "arrival seed unclked reg clk %s",
               network_->pathName(pin));
    search_->makeUnclkedPaths(vertex, true, false, tag_bldr_);
  }

  bool arrivals_changed = search_->arrivalsChanged(vertex, tag_bldr_);
  // If vertex is a latch data input arrival that changed from the
  // previous eval pass enqueue the latch outputs to be re-evaled on the
  // next pass.
  if (network_->isLatchData(pin)) {
    if (arrivals_changed
        && network_->isLatchData(pin))
      search_->enqueueLatchDataOutputs(vertex);
  }
  if (!search_->arrivalsAtEndpointsExist()
      || always_to_endpoints_
      || arrivals_changed)
    search_->arrivalIterator()->enqueueAdjacentVertices(vertex, adj_pred_);
  if (arrivals_changed) {
    debugPrint(debug_, "search", 4, "arrival changed");
    // Only update arrivals when delays change by more than
    // fuzzyEqual can distinguish.
    search_->setVertexArrivals(vertex, tag_bldr_);
    search_->tnsInvalid(vertex);
    constrainedRequiredsInvalid(vertex, is_clk);
  }
  enqueueRefPinInputDelays(pin);
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
    DataCheckSet *data_checks = sdc_->dataChecksFrom(pin);
    if (data_checks) {
      for (DataCheck *data_check : *data_checks) {
	Pin *to = data_check->to();
	search_->requiredInvalid(to);
      }
    }
    // Gated clocks.
    if (is_clk && sdc_->gatedClkChecksEnabled()) {
      PinSet enable_pins(network_);
      search_->gatedClk()->gatedClkEnables(vertex, enable_pins);
      for (const Pin *enable : enable_pins)
	search_->requiredInvalid(enable);
    }
  }
}

bool
Search::arrivalsChanged(Vertex *vertex,
			TagGroupBldr *tag_bldr)
{
  Arrival *arrivals1 = graph_->arrivals(vertex);
  if (arrivals1) {
    TagGroup *tag_group = tagGroup(vertex);
    if (tag_group == nullptr
        || tag_group->arrivalMap()->size() != tag_bldr->arrivalMap()->size())
      return true;
    ArrivalMap::Iterator arrival_iter1(tag_group->arrivalMap());
    while (arrival_iter1.hasNext()) {
      Tag *tag1;
      int arrival_index1;
      arrival_iter1.next(tag1, arrival_index1);
      Arrival &arrival1 = arrivals1[arrival_index1];
      Tag *tag2;
      Arrival arrival2;
      int arrival_index2;
      tag_bldr->tagMatchArrival(tag1, tag2, arrival2, arrival_index2);
      if (tag2 != tag1
	  || !delayEqual(arrival1, arrival2))
	return true;
    }
    return false;
  }
  else
    return true;
}

bool
ArrivalVisitor::visitFromToPath(const Pin *,
				Vertex *from_vertex,
				const RiseFall *from_rf,
				Tag *from_tag,
				PathVertex *from_path,
				Edge *,
				TimingArc *,
				ArcDelay arc_delay,
				Vertex *,
				const RiseFall *to_rf,
				Tag *to_tag,
				Arrival &to_arrival,
				const MinMax *min_max,
				const PathAnalysisPt *)
{
  debugPrint(debug_, "search", 3, " %s",
             from_vertex->name(sdc_network_));
  debugPrint(debug_, "search", 3, "  %s -> %s %s",
             from_rf->asString(),
             to_rf->asString(),
             min_max->asString());
  debugPrint(debug_, "search", 3, "  from tag: %s",
             from_tag->asString(this));
  debugPrint(debug_, "search", 3, "  to tag  : %s",
             to_tag->asString(this));
  ClkInfo *to_clk_info = to_tag->clkInfo();
  bool to_is_clk = to_tag->isClock();
  Arrival arrival;
  int arrival_index;
  Tag *tag_match;
  tag_bldr_->tagMatchArrival(to_tag, tag_match, arrival, arrival_index);
  if (tag_match == nullptr
      || delayGreater(to_arrival, arrival, min_max, this)) {
    debugPrint(debug_, "search", 3, "   %s + %s = %s %s %s",
               delayAsString(from_path->arrival(this), this),
               delayAsString(arc_delay, this),
               delayAsString(to_arrival, this),
               min_max == MinMax::max() ? ">" : "<",
               tag_match ? delayAsString(arrival, this) : "MIA");
    PathVertexRep prev_path;
    if (to_tag->isClock() || to_tag->isGenClkSrcPath())
      prev_path.init(from_path, this);
    tag_bldr_->setMatchArrival(to_tag, tag_match,
			       to_arrival, arrival_index,
			       &prev_path);
    if (crpr_active_
	&& !has_fanin_one_
	&& to_clk_info->hasCrprClkPin()
	&& !to_is_clk) {
      tag_bldr_no_crpr_->tagMatchArrival(to_tag, tag_match,
					 arrival, arrival_index);
      if (tag_match == nullptr
	  || delayGreater(to_arrival, arrival, min_max, this)) {
	tag_bldr_no_crpr_->setMatchArrival(to_tag, tag_match,
					   to_arrival, arrival_index,
					   &prev_path);
      }
    }
  }
  return true;
}

void
ArrivalVisitor::pruneCrprArrivals()
{
  ArrivalMap::Iterator arrival_iter(tag_bldr_->arrivalMap());
  CheckCrpr *crpr = search_->checkCrpr();
  while (arrival_iter.hasNext()) {
    Tag *tag;
    int arrival_index;
    arrival_iter.next(tag, arrival_index);
    ClkInfo *clk_info = tag->clkInfo();
    if (!tag->isClock()
	&& clk_info->hasCrprClkPin()) {
      PathAnalysisPt *path_ap = tag->pathAnalysisPt(this);
      const MinMax *min_max = path_ap->pathMinMax();
      Tag *tag_no_crpr;
      Arrival max_arrival;
      int max_arrival_index;
      tag_bldr_no_crpr_->tagMatchArrival(tag, tag_no_crpr,
					 max_arrival, max_arrival_index);
      if (tag_no_crpr) {
	ClkInfo *clk_info_no_crpr = tag_no_crpr->clkInfo();
	Arrival max_crpr = crpr->maxCrpr(clk_info_no_crpr);
	Arrival max_arrival_max_crpr = (min_max == MinMax::max())
	  ? max_arrival - max_crpr
	  : max_arrival + max_crpr;
	debugPrint(debug_, "search", 4, "  cmp %s %s - %s = %s",
                   tag->asString(this),
                   delayAsString(max_arrival, this),
                   delayAsString(max_crpr, this),
                   delayAsString(max_arrival_max_crpr, this));
	Arrival arrival = tag_bldr_->arrival(arrival_index);
	if (delayGreater(max_arrival_max_crpr, arrival, min_max, this)) {
	  debugPrint(debug_, "search", 3, "  pruned %s",
                     tag->asString(this));
	  tag_bldr_->deleteArrival(tag);
	}
      }
    }
  }
}

// Enqueue pins with input delays that use ref_pin as the clock
// reference pin as if there is a timing arc from the reference pin to
// the input delay pin.
void
ArrivalVisitor::enqueueRefPinInputDelays(const Pin *ref_pin)
{
  InputDelaySet *input_delays = sdc_->refPinInputDelays(ref_pin);
  if (input_delays) {
    for (InputDelay *input_delay : *input_delays) {
      const Pin *pin = input_delay->pin();
      Vertex *vertex, *bidirect_drvr_vertex;
      graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
      seedInputDelayArrival(pin, vertex, input_delay);
      if (bidirect_drvr_vertex)
	seedInputDelayArrival(pin, bidirect_drvr_vertex, input_delay);
    }
  }
}

void
ArrivalVisitor::seedInputDelayArrival(const Pin *pin,
				      Vertex *vertex,
				      InputDelay *input_delay)
{
  TagGroupBldr tag_bldr(true, this);
  tag_bldr.init(vertex);
  search_->seedInputDelayArrival(pin, vertex, input_delay,
                                 !network_->isTopLevelPort(pin), &tag_bldr);
  search_->setVertexArrivals(vertex, &tag_bldr);
  search_->arrivalIterator()->enqueueAdjacentVertices(vertex,
                                                      search_->searchAdj());
}

void
Search::enqueueLatchDataOutputs(Vertex *vertex)
{
  VertexOutEdgeIterator out_edge_iter(vertex, graph_);
  while (out_edge_iter.hasNext()) {
    Edge *out_edge = out_edge_iter.next();
    if (latches_->isLatchDtoQ(out_edge)) {
      Vertex *out_vertex = out_edge->to(graph_);
      UniqueLock lock(pending_latch_outputs_lock_);
      pending_latch_outputs_->insert(out_vertex);
    }
  }
}

void
Search::seedArrivals()
{
  VertexSet vertices(graph_);
  findClockVertices(vertices);
  findRootVertices(vertices);
  findInputDrvrVertices(vertices);

  for (Vertex *vertex : vertices)
    seedArrival(vertex);
}

void
Search::findClockVertices(VertexSet &vertices)
{
  for (const Clock *clk : sdc_->clks()) {
    for (const Pin *pin : clk->leafPins()) {
      Vertex *vertex, *bidirect_drvr_vertex;
      graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
      vertices.insert(vertex);
      if (bidirect_drvr_vertex)
	vertices.insert(bidirect_drvr_vertex);
    }
  }
}

void
Search::seedInvalidArrivals()
{
  for (Vertex *vertex : *invalid_arrivals_)
    seedArrival(vertex);
  invalid_arrivals_->clear();
}

void
Search::seedArrival(Vertex *vertex)
{
  const Pin *pin = vertex->pin();
  if (sdc_->isLeafPinClock(pin)) {
    TagGroupBldr tag_bldr(true, this);
    tag_bldr.init(vertex);
    genclks_->copyGenClkSrcPaths(vertex, &tag_bldr);
    seedClkArrivals(pin, vertex, &tag_bldr);
    // Clock pin may also have input arrivals from other clocks.
    seedInputArrival(pin, vertex, &tag_bldr);
    setVertexArrivals(vertex, &tag_bldr);
  }
  else if (isInputArrivalSrchStart(vertex)) {
    TagGroupBldr tag_bldr(true, this);
    tag_bldr.init(vertex);
    seedInputArrival(pin, vertex, &tag_bldr);
    setVertexArrivals(vertex, &tag_bldr);
    if (!tag_bldr.empty())
      // Only search downstream if there were non-false paths from here.
      arrival_iter_->enqueueAdjacentVertices(vertex, search_adj_);
  }
  else if (levelize_->isRoot(vertex)) {
    bool is_reg_clk = vertex->isRegClk();
    if (is_reg_clk
	// Internal roots isolated by disabled pins are seeded with no clock.
	|| (unconstrained_paths_
	    && !network_->isTopLevelPort(pin))) {
      debugPrint(debug_, "search", 2, "arrival seed unclked root %s",
                 network_->pathName(pin));
      TagGroupBldr tag_bldr(true, this);
      tag_bldr.init(vertex);
      if (makeUnclkedPaths(vertex, is_reg_clk, false, &tag_bldr))
	// Only search downstream if there are no false paths from here.
	arrival_iter_->enqueueAdjacentVertices(vertex, search_adj_);
      setVertexArrivals(vertex, &tag_bldr);
    }
    else {
      deletePaths(vertex);
      if (search_adj_->searchFrom(vertex))
	arrival_iter_->enqueueAdjacentVertices(vertex,  search_adj_);
    }
  }
  else {
    debugPrint(debug_, "search", 2, "arrival enqueue %s",
               network_->pathName(pin));
    arrival_iter_->enqueue(vertex);
  }
}

// Find all of the clock leaf pins.
void
Search::findClkVertexPins(PinSet &clk_pins)
{
  for (const Clock *clk : sdc_->clks()) {
    for (const Pin *pin : clk->leafPins()) {
      clk_pins.insert(pin);
    }
  }
}

void
Search::seedClkArrivals(const Pin *pin,
			Vertex *vertex,
			TagGroupBldr *tag_bldr)
{
  for (const Clock *clk : *sdc_->findLeafPinClocks(pin)) {
    debugPrint(debug_, "search", 2, "arrival seed clk %s pin %s",
               clk->name(), network_->pathName(pin));
    for (PathAnalysisPt *path_ap : corners_->pathAnalysisPts()) {
      const MinMax *min_max = path_ap->pathMinMax();
      for (const RiseFall *rf : RiseFall::range()) {
	const ClockEdge *clk_edge = clk->edge(rf);
	const EarlyLate *early_late = min_max;
	if (clk->isGenerated()
	    && clk->masterClk() == nullptr)
	  seedClkDataArrival(pin, rf, clk, clk_edge, min_max, path_ap,
			     0.0, tag_bldr);
	else {
	  Arrival insertion = clockInsertion(clk, pin, rf, min_max,
					     early_late, path_ap);
	  seedClkArrival(pin, rf, clk, clk_edge, min_max, path_ap,
			 insertion, tag_bldr);
	}
      }
    }
    arrival_iter_->enqueueAdjacentVertices(vertex,  search_adj_);
  }
}

void
Search::seedClkArrival(const Pin *pin,
		       const RiseFall *rf,
		       const Clock *clk,
		       const ClockEdge *clk_edge,
		       const MinMax *min_max,
		       const PathAnalysisPt *path_ap,
		       Arrival insertion,
		       TagGroupBldr *tag_bldr)
{
  bool is_propagated = false;
  float latency = 0.0;
  bool latency_exists;
  // Check for clk pin latency.
  sdc_->clockLatency(clk, pin, rf, min_max,
		     latency, latency_exists);
  if (!latency_exists) {
    // Check for clk latency (lower priority).
    sdc_->clockLatency(clk, rf, min_max,
		       latency, latency_exists);
    if (latency_exists) {
      // Propagated pin overrides latency on clk.
      if (sdc_->isPropagatedClock(pin)) {
	latency = 0.0;
	latency_exists = false;
	is_propagated = true;
      }
    }
    else
      is_propagated = sdc_->isPropagatedClock(pin)
	|| clk->isPropagated();
  }

  ClockUncertainties *uncertainties = sdc_->clockUncertainties(pin);
  if (uncertainties == nullptr)
    uncertainties = clk->uncertainties();
  // Propagate liberty "pulse_clock" transition to transitive fanout.
  LibertyPort *port = network_->libertyPort(pin);
  RiseFall *pulse_clk_sense = (port ? port->pulseClkSense() : nullptr);
  ClkInfo *clk_info = findClkInfo(clk_edge, pin, is_propagated, nullptr, false,
				  pulse_clk_sense, insertion, latency,
				  uncertainties, path_ap, nullptr);
  // Only false_paths -from apply to clock tree pins.
  ExceptionStateSet *states = nullptr;
  sdc_->exceptionFromClkStates(pin,rf,clk,rf,min_max,states);
  Tag *tag = findTag(rf, path_ap, clk_info, true, nullptr, false, states, true);
  Arrival arrival(clk_edge->time() + insertion);
  tag_bldr->setArrival(tag, arrival, nullptr);
}

void
Search::seedClkDataArrival(const Pin *pin,
			   const RiseFall *rf,
			   const Clock *clk,
			   const ClockEdge *clk_edge,
			   const MinMax *min_max,
			   const PathAnalysisPt *path_ap,
			   Arrival insertion,
			   TagGroupBldr *tag_bldr)
{	
  Tag *tag = clkDataTag(pin, clk, rf, clk_edge, insertion, min_max, path_ap);
  if (tag) {
    // Data arrivals include insertion delay.
    Arrival arrival(clk_edge->time() + insertion);
    tag_bldr->setArrival(tag, arrival, nullptr);
  }
}

Tag *
Search::clkDataTag(const Pin *pin,
		   const Clock *clk,
		   const RiseFall *rf,
		   const ClockEdge *clk_edge,
		   Arrival insertion,
 		   const MinMax *min_max,
 		   const PathAnalysisPt *path_ap)
{
  ExceptionStateSet *states = nullptr;
  if (sdc_->exceptionFromStates(pin, rf, clk, rf, min_max, states)) {
    bool is_propagated = (clk->isPropagated()
			  || sdc_->isPropagatedClock(pin));
    ClkInfo *clk_info = findClkInfo(clk_edge, pin, is_propagated,
				    insertion, path_ap);
    return findTag(rf, path_ap, clk_info, false, nullptr, false, states, true);
  }
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

bool
Search::makeUnclkedPaths(Vertex *vertex,
			 bool is_segment_start,
			 bool require_exception,
                         TagGroupBldr *tag_bldr)
{
  bool search_from = false;
  const Pin *pin = vertex->pin();
  for (PathAnalysisPt *path_ap : corners_->pathAnalysisPts()) {
    const MinMax *min_max = path_ap->pathMinMax();
    for (RiseFall *rf : RiseFall::range()) {
      Tag *tag = fromUnclkedInputTag(pin, rf, min_max, path_ap,
				     is_segment_start,
                                     require_exception);
      if (tag) {
	tag_bldr->setArrival(tag, delay_zero, nullptr);
	search_from = true;
      }
    }
  }
  return search_from;
}

// Find graph roots and input ports that do NOT have arrivals.
void
Search::findRootVertices(VertexSet &vertices)
{
  for (Vertex *vertex : *levelize_->roots()) {
    const Pin *pin = vertex->pin();
    if (!sdc_->isLeafPinClock(pin)
	&& !sdc_->hasInputDelay(pin)
	&& !vertex->isConstant()) {
      vertices.insert(vertex);
    }
  }
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
Search::isSegmentStart(const Pin *pin)
{
  return sdc_->isInputDelayInternal(pin)
    && !sdc_->isLeafPinClock(pin);
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

// Seed input arrivals clocked by clks.
void
Search::seedInputArrivals(ClockSet *clks)
{
  // Input arrivals can be on internal pins, so iterate over the pins
  // that have input arrivals rather than the top level input pins.
  for (auto iter : sdc_->inputDelayPinMap()) {
    const Pin *pin = iter.first;
    if (!sdc_->isLeafPinClock(pin)) {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      seedInputArrival(pin, vertex, clks);
    }
  }
}

void
Search::seedInputArrival(const Pin *pin,
			 Vertex *vertex,
			 ClockSet *wrt_clks)
{
  bool has_arrival = false;
  // There can be multiple arrivals for a pin with wrt different clocks.
  TagGroupBldr tag_bldr(true, this);
  tag_bldr.init(vertex);
  InputDelaySet *input_delays = sdc_->inputDelaysLeafPin(pin);
  if (input_delays) {
    for (InputDelay *input_delay : *input_delays) {
      Clock *input_clk = input_delay->clock();
      ClockSet *pin_clks = sdc_->findLeafPinClocks(pin);
      if (input_clk && wrt_clks->hasKey(input_clk)
	  // Input arrivals wrt a clock source pin is the insertion
	  // delay (source latency), but arrivals wrt other clocks
	  // propagate.
	  && (pin_clks == nullptr
	      || !pin_clks->hasKey(input_clk))) {
	seedInputDelayArrival(pin, vertex, input_delay, false, &tag_bldr);
	has_arrival = true;
      }
    }
    if (has_arrival)
      setVertexArrivals(vertex, &tag_bldr);
  }
}

void
Search::seedInputArrival(const Pin *pin,
			 Vertex *vertex,
			 TagGroupBldr *tag_bldr)
{
  if (sdc_->hasInputDelay(pin))
    seedInputArrival1(pin, vertex, false, tag_bldr);
  else if (!sdc_->isLeafPinClock(pin))
    // Seed inputs without set_input_delays.
    seedInputDelayArrival(pin, vertex, nullptr, false, tag_bldr);
}

void
Search::seedInputSegmentArrival(const Pin *pin,
				Vertex *vertex,
				TagGroupBldr *tag_bldr)
{
  seedInputArrival1(pin, vertex, true, tag_bldr);
}

void
Search::seedInputArrival1(const Pin *pin,
			  Vertex *vertex,
			  bool is_segment_start,
			  TagGroupBldr *tag_bldr)
{
  // There can be multiple arrivals for a pin with wrt different clocks.
  InputDelaySet *input_delays = sdc_->inputDelaysLeafPin(pin);
  if (input_delays) {
    for (InputDelay *input_delay : *input_delays) {
      Clock *input_clk = input_delay->clock();
      ClockSet *pin_clks = sdc_->findLeafPinClocks(pin);
      // Input arrival wrt a clock source pin is the clock insertion
      // delay (source latency), but arrivals wrt other clocks
      // propagate.
      if (pin_clks == nullptr
	  || !pin_clks->hasKey(input_clk))
	seedInputDelayArrival(pin, vertex, input_delay, is_segment_start,
			      tag_bldr);
    }
  }
}

void
Search::seedInputDelayArrival(const Pin *pin,
			      Vertex *vertex,
			      InputDelay *input_delay,
			      bool is_segment_start,
			      TagGroupBldr *tag_bldr)
{
  debugPrint(debug_, "search", 2,
             input_delay
             ? "arrival seed input arrival %s"
             : "arrival seed input %s",
             vertex->name(sdc_network_));
  const ClockEdge *clk_edge = nullptr;
  const Pin *ref_pin = nullptr;
  if (input_delay) {
    clk_edge = input_delay->clkEdge();
    if (clk_edge == nullptr
	&& sdc_->useDefaultArrivalClock())
      clk_edge = sdc_->defaultArrivalClockEdge();
    ref_pin = input_delay->refPin();
  }
  else if (sdc_->useDefaultArrivalClock())
    clk_edge = sdc_->defaultArrivalClockEdge();
  if (ref_pin) {
    Vertex *ref_vertex = graph_->pinLoadVertex(ref_pin);
    for (PathAnalysisPt *path_ap : corners_->pathAnalysisPts()) {
      const MinMax *min_max = path_ap->pathMinMax();
      const RiseFall *ref_rf = input_delay->refTransition();
      const Clock *clk = input_delay->clock();
      VertexPathIterator ref_path_iter(ref_vertex, ref_rf, path_ap, this);
      while (ref_path_iter.hasNext()) {
	Path *ref_path = ref_path_iter.next();
	if (ref_path->isClock(this)
	    && (clk == nullptr
		|| ref_path->clock(this) == clk)) {
	  float ref_arrival, ref_insertion, ref_latency;
	  inputDelayRefPinArrival(ref_path, ref_path->clkEdge(this), min_max,
				  ref_arrival, ref_insertion, ref_latency);
	  seedInputDelayArrival(pin, input_delay, ref_path->clkEdge(this),
				ref_arrival, ref_insertion, ref_latency,
				is_segment_start, min_max, path_ap, tag_bldr);
	}
      }
    }
  }
  else {
    for (PathAnalysisPt *path_ap : corners_->pathAnalysisPts()) {
      const MinMax *min_max = path_ap->pathMinMax();
      float clk_arrival, clk_insertion, clk_latency;
      inputDelayClkArrival(input_delay, clk_edge, min_max, path_ap,
			   clk_arrival, clk_insertion, clk_latency);
      seedInputDelayArrival(pin, input_delay, clk_edge,
			    clk_arrival, clk_insertion, clk_latency,
			    is_segment_start, min_max, path_ap, tag_bldr);
    }
  }
}

// Input delays with -reference_pin use the clock network latency
// from the clock source to the reference pin.
void
Search::inputDelayRefPinArrival(Path *ref_path,
				const ClockEdge *clk_edge,
				const MinMax *min_max,
				// Return values.
				float &ref_arrival,
				float &ref_insertion,
				float &ref_latency)
{
  Clock *clk = clk_edge->clock();
  if (clk->isPropagated()) {
    ClkInfo *clk_info = ref_path->clkInfo(this);
    ref_arrival = delayAsFloat(ref_path->arrival(this));
    ref_insertion = delayAsFloat(clk_info->insertion());
    ref_latency = clk_info->latency();
  }
  else {
    const RiseFall *clk_rf = clk_edge->transition();
    const EarlyLate *early_late = min_max;
    // Input delays from ideal clk reference pins include clock
    // insertion delay but not latency.
    ref_insertion = sdc_->clockInsertion(clk, clk_rf, min_max, early_late);
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
			      PathAnalysisPt *path_ap,
			      TagGroupBldr *tag_bldr)
{
  for (RiseFall *rf : RiseFall::range()) {
    if (input_delay) {
      float delay;
      bool exists;
      input_delay->delays()->value(rf, min_max, delay, exists);
      if (exists)
	seedInputDelayArrival(pin, rf, clk_arrival + delay,
			      input_delay, clk_edge,
			      clk_insertion,  clk_latency, is_segment_start,
			      min_max, path_ap, tag_bldr);
    }
    else
      seedInputDelayArrival(pin, rf, 0.0,  nullptr, clk_edge,
			    clk_insertion,  clk_latency, is_segment_start,
			    min_max, path_ap, tag_bldr);
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
			      PathAnalysisPt *path_ap,
			      TagGroupBldr *tag_bldr)
{
  Tag *tag = inputDelayTag(pin, rf, clk_edge, clk_insertion, clk_latency,
			   input_delay, is_segment_start, min_max, path_ap);
  if (tag)
    tag_bldr->setArrival(tag, arrival, nullptr);
}

void
Search::inputDelayClkArrival(InputDelay *input_delay,
			     const ClockEdge *clk_edge,
			     const MinMax *min_max,
			     const PathAnalysisPt *path_ap,
			     // Return values.
			     float &clk_arrival, float &clk_insertion,
			     float &clk_latency)
{
  clk_arrival = 0.0;
  clk_insertion = 0.0;
  clk_latency = 0.0;
  if (input_delay && clk_edge) {
    clk_arrival = clk_edge->time();
    Clock *clk = clk_edge->clock();
    RiseFall *clk_rf = clk_edge->transition();
    if (!input_delay->sourceLatencyIncluded()) {
      const EarlyLate *early_late = min_max;
      clk_insertion = delayAsFloat(clockInsertion(clk, clk->defaultPin(),
						  clk_rf, min_max, early_late,
						  path_ap));
      clk_arrival += clk_insertion;
    }
    if (!clk->isPropagated()
	&& !input_delay->networkLatencyIncluded()) {
      clk_latency = sdc_->clockLatency(clk, clk_rf, min_max);
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
		      const PathAnalysisPt *path_ap)
{
  Clock *clk = nullptr;
  const Pin *clk_pin = nullptr;
  RiseFall *clk_rf = nullptr;
  bool is_propagated = false;
  ClockUncertainties *clk_uncertainties = nullptr;
  if (clk_edge) {
    clk = clk_edge->clock();
    clk_rf = clk_edge->transition();
    clk_pin = clk->defaultPin();
    is_propagated = clk->isPropagated();
    clk_uncertainties = clk->uncertainties();
  }

  ExceptionStateSet *states = nullptr;
  Tag *tag = nullptr;
  if (sdc_->exceptionFromStates(pin,rf,clk,clk_rf,min_max,states)) {
    ClkInfo *clk_info = findClkInfo(clk_edge, clk_pin, is_propagated, nullptr,
				    false, nullptr, clk_insertion, clk_latency,
				    clk_uncertainties, path_ap, nullptr);
    tag = findTag(rf, path_ap, clk_info, false, input_delay, is_segment_start,
		  states, true);
  }

  if (tag) {
    ClkInfo *clk_info = tag->clkInfo();
    // Check for state changes on existing tag exceptions (pending -thru pins).
    tag = mutateTag(tag, pin, rf, false, clk_info,
		    pin, rf, false, false, is_segment_start, clk_info,
		    input_delay, min_max, path_ap);
  }
  return tag;
}

////////////////////////////////////////////////////////////////

PathVisitor::PathVisitor(const StaState *sta) :
  StaState(sta),
  pred_(sta->search()->evalPred())
{
}

PathVisitor::PathVisitor(SearchPred *pred,
			 const StaState *sta) :
  StaState(sta),
  pred_(pred)
{
}

void
PathVisitor::visitFaninPaths(Vertex *to_vertex)
{
  if (pred_->searchTo(to_vertex)) {
    VertexInEdgeIterator edge_iter(to_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph_);
      const Pin *from_pin = from_vertex->pin();
      if (pred_->searchFrom(from_vertex)
	  && pred_->searchThru(edge)) {
	const Pin *to_pin = to_vertex->pin();
	if (!visitEdge(from_pin, from_vertex, edge, to_pin, to_vertex))
	  break;
      }
    }
  }
}

void
PathVisitor::visitFanoutPaths(Vertex *from_vertex)
{
  const Pin *from_pin = from_vertex->pin();
  if (pred_->searchFrom(from_vertex)) {
    VertexOutEdgeIterator edge_iter(from_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      const Pin *to_pin = to_vertex->pin();
      if (pred_->searchTo(to_vertex)
	  && pred_->searchThru(edge)) {
	debugPrint(debug_, "search", 3, " %s",
                   to_vertex->name(network_));
	if (!visitEdge(from_pin, from_vertex, edge, to_pin, to_vertex))
	  break;
      }
    }
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
    while (from_iter.hasNext()) {
      PathVertex *from_path = from_iter.next();
      PathAnalysisPt *path_ap = from_path->pathAnalysisPt(this);
      const MinMax *min_max = path_ap->pathMinMax();
      const RiseFall *from_rf = from_path->transition(this);
      TimingArc *arc1, *arc2;
      arc_set->arcsFrom(from_rf, arc1, arc2);
      if (!visitArc(from_pin, from_vertex, from_rf, from_path,
                    edge, arc1, to_pin, to_vertex,
                    min_max, path_ap))
        return false;
      if (!visitArc(from_pin, from_vertex, from_rf, from_path,
                    edge, arc2, to_pin, to_vertex,
                    min_max, path_ap))
        return false;
    }
  }
  return true;
}

bool
PathVisitor::visitArc(const Pin *from_pin,
		      Vertex *from_vertex,
		      const RiseFall *from_rf,
		      PathVertex *from_path,
		      Edge *edge,
		      TimingArc *arc,
		      const Pin *to_pin,
		      Vertex *to_vertex,
		      const MinMax *min_max,
		      PathAnalysisPt *path_ap)
{
  if (arc) {
    RiseFall *to_rf = arc->toEdge()->asRiseFall();
    if (searchThru(from_vertex, from_rf, edge, to_vertex, to_rf))
      return visitFromPath(from_pin, from_vertex, from_rf, from_path,
			   edge, arc, to_pin, to_vertex, to_rf,
			   min_max, path_ap);
  }
  return true;
}

bool
PathVisitor::visitFromPath(const Pin *from_pin,
			   Vertex *from_vertex,
			   const RiseFall *from_rf,
			   PathVertex *from_path,
			   Edge *edge,
			   TimingArc *arc,
			   const Pin *to_pin,
			   Vertex *to_vertex,
			   const RiseFall *to_rf,
			   const MinMax *min_max,
			   const PathAnalysisPt *path_ap)
{
  const TimingRole *role = edge->role();
  Tag *from_tag = from_path->tag(this);
  ClkInfo *from_clk_info = from_tag->clkInfo();
  Tag *to_tag = nullptr;
  const ClockEdge *clk_edge = from_clk_info->clkEdge();
  const Clock *clk = from_clk_info->clock();
  Arrival from_arrival = from_path->arrival(this);
  ArcDelay arc_delay = 0.0;
  Arrival to_arrival;
  if (from_clk_info->isGenClkSrcPath()) {
    if (!sdc_->clkStopPropagation(clk,from_pin,from_rf,to_pin,to_rf)
	&& (sdc_->clkThruTristateEnabled()
	    || !(role == TimingRole::tristateEnable()
		 || role == TimingRole::tristateDisable()))) {
      const Clock *gclk = from_tag->genClkSrcPathClk(this);
      if (gclk) {
	Genclks *genclks = search_->genclks();
	VertexSet *fanins = genclks->fanins(gclk);
	// Note: encountering a latch d->q edge means find the
	// latch feedback edges, but they are referenced for 
	// other edges in the gen clk fanout.
	if (role == TimingRole::latchDtoQ())
	  genclks->findLatchFdbkEdges(gclk);
	EdgeSet *fdbk_edges = genclks->latchFdbkEdges(gclk);
	if ((role == TimingRole::combinational()
	     || role == TimingRole::wire()
	     || !gclk->combinational())
	    && fanins->hasKey(to_vertex)
	    && !(fdbk_edges && fdbk_edges->hasKey(edge))) {
	  to_tag = search_->thruClkTag(from_path, from_tag, true, edge, to_rf,
                                       min_max, path_ap);
	  if (to_tag) {
	    arc_delay = search_->deratedDelay(from_vertex, arc, edge, true,
                                              path_ap);
	    to_arrival = from_arrival + arc_delay;
	  }
	}
      }
    }
  }
  else if (role->genericRole() == TimingRole::regClkToQ()) {
    if (clk == nullptr
	|| !sdc_->clkStopPropagation(from_pin, clk)) {
      arc_delay = search_->deratedDelay(from_vertex, arc, edge, false, path_ap);
      // Propagate from unclocked reg/latch clk pins, which have no
      // clk but are distinguished with a segment_start flag.
      if ((clk_edge == nullptr
	   && from_tag->isSegmentStart())
	  // Do not propagate paths from input ports with default
	  // input arrival clk thru CLK->Q edges.
	  || (clk != sdc_->defaultArrivalClock()
	      // Only propagate paths from clocks that have not
	      // passed thru reg/latch D->Q edges.
	      && from_tag->isClock())) {
	const RiseFall *clk_rf = clk_edge ? clk_edge->transition() : nullptr;
	ClkInfo *to_clk_info = from_clk_info;
	if (network_->direction(to_pin)->isInternal())
	  to_clk_info = search_->clkInfoWithCrprClkPath(from_clk_info,
                                                        from_path, path_ap);
	to_tag = search_->fromRegClkTag(from_pin, from_rf, clk, clk_rf,
                                        to_clk_info, to_pin, to_rf, min_max,
                                        path_ap);
	if (to_tag)
	  to_tag = search_->thruTag(to_tag, edge, to_rf, min_max, path_ap);
	from_arrival = search_->clkPathArrival(from_path, from_clk_info,
                                               clk_edge, min_max, path_ap);
	to_arrival = from_arrival + arc_delay;
      }
      else
	to_tag = nullptr;
    }
  }
  else if (edge->role() == TimingRole::latchDtoQ()) {
    if (min_max == MinMax::max()) {
      arc_delay = search_->deratedDelay(from_vertex, arc, edge, false, path_ap);
      latches_->latchOutArrival(from_path, arc, edge, path_ap,
                                to_tag, arc_delay, to_arrival);
      if (to_tag)
	to_tag = search_->thruTag(to_tag, edge, to_rf, min_max, path_ap);
    }
  }
  else if (from_tag->isClock()) {
    ClockSet *clks = sdc_->findLeafPinClocks(from_pin);
    // Disable edges from hierarchical clock source pins that do
    // not go thru the hierarchical pin and edges from clock source pins
    // that traverse a hierarchical source pin of a different clock.
    // Clock arrivals used as data also need to be disabled.
    if (!(role == TimingRole::wire()
	  && sdc_->clkDisabledByHpinThru(clk, from_pin, to_pin))
        // Generated clock source pins have arrivals for the source clock.
        // Do not propagate them past the generated clock source pin.
        && !(clks
             && !clks->hasKey(const_cast<Clock*>(from_tag->clock())))) {
      // Propagate arrival as non-clock at the end of the clock tree.
      bool to_propagates_clk =
	!sdc_->clkStopPropagation(clk,from_pin,from_rf,to_pin,to_rf)
	&& (sdc_->clkThruTristateEnabled()
	    || !(role == TimingRole::tristateEnable()
		 || role == TimingRole::tristateDisable()));
      arc_delay = search_->deratedDelay(from_vertex, arc, edge,
                                        to_propagates_clk, path_ap);
      to_tag = search_->thruClkTag(from_path, from_tag, to_propagates_clk,
                                   edge, to_rf, min_max, path_ap);
      to_arrival = from_arrival + arc_delay;
    }
  }
  else {
    arc_delay = search_->deratedDelay(from_vertex, arc, edge, false, path_ap);
    if (!delayInf(arc_delay)) {
      to_arrival = from_arrival + arc_delay;
      to_tag = search_->thruTag(from_tag, edge, to_rf, min_max, path_ap);
    }
  }
  if (to_tag)
    return visitFromToPath(from_pin, from_vertex, from_rf, from_tag, from_path,
			   edge, arc, arc_delay,
			   to_vertex, to_rf, to_tag, to_arrival,
			   min_max, path_ap);
  else
    return true;
}

Arrival
Search::clkPathArrival(const Path *clk_path) const
{
  ClkInfo *clk_info = clk_path->clkInfo(this);
  const ClockEdge *clk_edge = clk_info->clkEdge();
  const PathAnalysisPt *path_ap = clk_path->pathAnalysisPt(this);
  const MinMax *min_max = path_ap->pathMinMax();
  return clkPathArrival(clk_path, clk_info, clk_edge, min_max, path_ap);
}

Arrival
Search::clkPathArrival(const Path *clk_path,
		       ClkInfo *clk_info,
		       const ClockEdge *clk_edge,
		       const MinMax *min_max,
		       const PathAnalysisPt *path_ap) const
{
  if (clk_path->vertex(this)->isRegClk()
      && clk_path->isClock(this)
      && clk_edge
      && !clk_info->isPropagated()) {
    // Ideal clock, apply ideal insertion delay and latency.
    const EarlyLate *early_late = min_max;
    return clk_edge->time()
      + clockInsertion(clk_edge->clock(),
		       clk_info->clkSrc(),
		       clk_edge->transition(),
		       min_max, early_late, path_ap)
      + clk_info->latency();
  }
  else
    return clk_path->arrival(this);
}

Arrival
Search::pathClkPathArrival(const Path *path) const
{
  ClkInfo *clk_info = path->clkInfo(this);
  if (clk_info->isPropagated()) {
    PathRef src_clk_path =  pathClkPathArrival1(path);
    if (!src_clk_path.isNull())
      return clkPathArrival(&src_clk_path);
  }
  // Check for input arrival clock.
  const ClockEdge *clk_edge = path->clkEdge(this);
  if (clk_edge)
    return clk_edge->time() + clk_info->latency();
  return 0.0;
}

// PathExpanded::expand() and PathExpanded::clkPath().
PathRef
Search::pathClkPathArrival1(const Path *path) const
{
  PathRef p(path);
  while (!p.isNull()) {
    PathRef prev_path;
    TimingArc *prev_arc;
    p.prevPath(this, prev_path, prev_arc);

    if (p.isClock(this))
      return p;
    if (prev_arc) {
      TimingRole *prev_role = prev_arc->role();
      if (prev_role == TimingRole::regClkToQ()
	  || prev_role == TimingRole::latchEnToQ()) {
	p.prevPath(this, prev_path, prev_arc);
	return prev_path;
      }
      else if (prev_role == TimingRole::latchDtoQ()) {
	Edge *prev_edge = p.prevEdge(prev_arc, this);
	PathVertex enable_path;
	latches_->latchEnablePath(&p, prev_edge, enable_path);
	return enable_path;
      }
    }
    p.init(prev_path);
  }
  return PathRef();
}

////////////////////////////////////////////////////////////////

// Find tag for a path starting with pin/clk_edge.
// Return nullptr if a false path starts at pin/clk_edge.
Tag *
Search::fromUnclkedInputTag(const Pin *pin,
			    const RiseFall *rf,
			    const MinMax *min_max,
			    const PathAnalysisPt *path_ap,
			    bool is_segment_start,
                            bool require_exception)
{
  ExceptionStateSet *states = nullptr;
  if (sdc_->exceptionFromStates(pin, rf, nullptr, nullptr, min_max, states)
      && (!require_exception || states)) {
    ClkInfo *clk_info = findClkInfo(nullptr, nullptr, false, 0.0, path_ap);
    return findTag(rf, path_ap, clk_info, false, nullptr,
                   is_segment_start, states, true);
  }
  return nullptr;
}

Tag *
Search::fromRegClkTag(const Pin *from_pin,
		      const RiseFall *from_rf,
		      const Clock *clk,
		      const RiseFall *clk_rf,
		      ClkInfo *clk_info,
		      const Pin *to_pin,
		      const RiseFall *to_rf,
		      const MinMax *min_max,
		      const PathAnalysisPt *path_ap)
{
  ExceptionStateSet *states = nullptr;
  if (sdc_->exceptionFromStates(from_pin, from_rf, clk, clk_rf,
				min_max, states)) {
    // Hack for filter -from reg/Q.
    sdc_->filterRegQStates(to_pin, to_rf, min_max, states);
    return findTag(to_rf, path_ap, clk_info, false, nullptr, false, states, true);
  }
  else
    return nullptr;
}

// Insert from_path as ClkInfo crpr_clk_path.
ClkInfo *
Search::clkInfoWithCrprClkPath(ClkInfo *from_clk_info,
			       PathVertex *from_path,
			       const PathAnalysisPt *path_ap)
{
  if (sdc_->crprActive())
    return findClkInfo(from_clk_info->clkEdge(),
		       from_clk_info->clkSrc(),
		       from_clk_info->isPropagated(),
		       from_clk_info->genClkSrc(),
		       from_clk_info->isGenClkSrcPath(),
		       from_clk_info->pulseClkSense(),
		       from_clk_info->insertion(),
		       from_clk_info->latency(),
		       from_clk_info->uncertainties(),
		       path_ap, from_path);
  else
    return from_clk_info;
}

// Find tag for a path starting with from_tag going thru edge.
// Return nullptr if the result tag completes a false path.
Tag *
Search::thruTag(Tag *from_tag,
		Edge *edge,
		const RiseFall *to_rf,
		const MinMax *min_max,
		const PathAnalysisPt *path_ap)
{
  const Pin *from_pin = edge->from(graph_)->pin();
  Vertex *to_vertex = edge->to(graph_);
  const Pin *to_pin = to_vertex->pin();
  const RiseFall *from_rf = from_tag->transition();
  ClkInfo *from_clk_info = from_tag->clkInfo();
  bool to_is_reg_clk = to_vertex->isRegClk();
  Tag *to_tag = mutateTag(from_tag, from_pin, from_rf, false, from_clk_info,
			  to_pin, to_rf, false, to_is_reg_clk, false,
			  // input delay is not propagated.
			  from_clk_info, nullptr, min_max, path_ap);
  return to_tag;
}

Tag *
Search::thruClkTag(PathVertex *from_path,
		   Tag *from_tag,
		   bool to_propagates_clk,
		   Edge *edge,
		   const RiseFall *to_rf,
		   const MinMax *min_max,
		   const PathAnalysisPt *path_ap)
{
  const Pin *from_pin = edge->from(graph_)->pin();
  Vertex *to_vertex = edge->to(graph_);
  const Pin *to_pin = to_vertex->pin();
  const RiseFall *from_rf = from_tag->transition();
  ClkInfo *from_clk_info = from_tag->clkInfo();
  bool from_is_clk = from_tag->isClock();
  bool to_is_reg_clk = to_vertex->isRegClk();
  TimingRole *role = edge->role();
  bool to_is_clk = (from_is_clk
		    && to_propagates_clk
		    && (role->isWire()
			|| role == TimingRole::combinational()));
  ClkInfo *to_clk_info = thruClkInfo(from_path, from_clk_info,
				     edge, to_vertex, to_pin, min_max, path_ap);
  Tag *to_tag = mutateTag(from_tag,from_pin,from_rf,from_is_clk,from_clk_info,
			  to_pin, to_rf, to_is_clk, to_is_reg_clk, false,
			  to_clk_info, nullptr, min_max, path_ap);
  return to_tag;
}

// thruTag for clocks.
ClkInfo *
Search::thruClkInfo(PathVertex *from_path,
		    ClkInfo *from_clk_info,
		    Edge *edge,
		    Vertex *to_vertex,
		    const Pin *to_pin,
		    const MinMax *min_max,
		    const PathAnalysisPt *path_ap)
{
  ClkInfo *to_clk_info = from_clk_info;
  bool changed = false;
  const ClockEdge *from_clk_edge = from_clk_info->clkEdge();
  const RiseFall *clk_rf = from_clk_edge->transition();

  bool from_clk_prop = from_clk_info->isPropagated();
  bool to_clk_prop = from_clk_prop;
  if (!from_clk_prop
      && sdc_->isPropagatedClock(to_pin)) {
    to_clk_prop = true;
    changed = true;
  }

  // Distinguish gen clk src path ClkInfo at generated clock roots,
  // so that generated clock crpr info can be (later) safely set on
  // the clkinfo.
  const Pin *gen_clk_src = nullptr;
  if (from_clk_info->isGenClkSrcPath()
      && sdc_->crprActive()
      && sdc_->isClock(to_pin)) {
    // Don't care that it could be a regular clock root.
    gen_clk_src = to_pin;
    changed = true;
  }

  PathVertex *to_crpr_clk_path = nullptr;
  if (sdc_->crprActive()
      && to_vertex->isRegClk()) {
    to_crpr_clk_path = from_path;
    changed = true;
  }

  // Propagate liberty "pulse_clock" transition to transitive fanout.
  RiseFall *from_pulse_sense = from_clk_info->pulseClkSense();
  RiseFall *to_pulse_sense = from_pulse_sense;
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
  sdc_->clockLatency(from_clk, to_pin, clk_rf, min_max,
		     latency, exists);
  if (exists) {
    // Latency on pin has precidence over fanin or hierarchical
    // pin latency.
    to_latency = latency;
    to_clk_prop = false;
    changed = true;
  }
  else {
    // Check for hierarchical pin latency thru edge.
    sdc_->clockLatency(edge, clk_rf, min_max,
		       latency, exists);
    if (exists) {
      to_latency = latency;
      to_clk_prop = false;
      changed = true;
    }
  }

  ClockUncertainties *to_uncertainties = from_clk_info->uncertainties();
  ClockUncertainties *uncertainties = sdc_->clockUncertainties(to_pin);
  if (uncertainties) {
    to_uncertainties = uncertainties;
    changed = true;
  }

  if (changed)
    to_clk_info = findClkInfo(from_clk_edge, from_clk_info->clkSrc(),
			      to_clk_prop, gen_clk_src,
			      from_clk_info->isGenClkSrcPath(),
			      to_pulse_sense, to_insertion, to_latency,
			      to_uncertainties, path_ap, to_crpr_clk_path);
  return to_clk_info;
}

// Find the tag for a path going from from_tag thru edge to to_pin.
Tag *
Search::mutateTag(Tag *from_tag,
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
		  const PathAnalysisPt *path_ap)
{
  ExceptionStateSet *new_states = nullptr;
  ExceptionStateSet *from_states = from_tag->states();
  if (from_states) {
    // Check for state changes in from_tag (but postpone copying state set).
    bool state_change = false;
    for (auto state : *from_states) {
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
           && sdc_->isCompleteTo(state, from_pin, from_rf, min_max))
          // Kill loop tags at register clock pins.
          || (exception->isLoop()
              && to_is_reg_clk)) {
	state_change = true;
        break;
      }
    }

    // Get the set of -thru exceptions starting at to_pin/edge.
    new_states = sdc_->exceptionThruStates(from_pin, to_pin, to_rf, min_max);
    if (new_states || state_change) {
      // Second pass to apply state changes and add updated existing
      // states to new states.
      if (new_states == nullptr)
	new_states = new ExceptionStateSet(network_);
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
               && sdc_->isCompleteTo(state, from_pin, from_rf, min_max))
              // Kill loop tags at register clock pins.
              || (to_is_reg_clk
                  && exception->isLoop())))
	  new_states->insert(state);
      }
    }
  }
  else
    // Get the set of -thru exceptions starting at to_pin/edge.
    new_states = sdc_->exceptionThruStates(from_pin, to_pin, to_rf, min_max);

  if (new_states)
    return findTag(to_rf, path_ap, to_clk_info, to_is_clk,
		   from_tag->inputDelay(), to_is_segment_start,
		   new_states, true);
  else {
    // No state change.
    if (to_clk_info == from_clk_info
	&& to_rf == from_rf
	&& to_is_clk == from_is_clk
	&& from_tag->isSegmentStart() == to_is_segment_start
	&& from_tag->inputDelay() == to_input_delay)
      return from_tag;
    else
      return findTag(to_rf, path_ap, to_clk_info, to_is_clk,
		     to_input_delay, to_is_segment_start,
		     from_states, false);
  }
}

TagGroup *
Search::findTagGroup(TagGroupBldr *tag_bldr)
{
  TagGroup probe(tag_bldr);
  UniqueLock lock(tag_group_lock_);
  TagGroup *tag_group = tag_group_set_->findKey(&probe);
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
    // std::vector doesn't seem to follow this protocol so multi-thread
    // search fails occasionally if a vector is used for tag_groups_.
    if (tag_group_next_ == tag_group_capacity_) {
      TagGroupIndex new_capacity = nextMersenne(tag_group_capacity_);
      TagGroup **new_tag_groups = new TagGroup*[new_capacity];
      memcpy(new_tag_groups, tag_groups_,
             tag_group_capacity_ * sizeof(TagGroup*));
      TagGroup **old_tag_groups = tag_groups_;
      tag_groups_ = new_tag_groups;
      tag_group_capacity_ = new_capacity;
      delete [] old_tag_groups;
      tag_group_set_->reserve(new_capacity);
    }
    if (tag_group_next_ > tag_group_index_max)
      report_->critical(260, "max tag group index exceeded");
  }
  return tag_group;
}

void
Search::setVertexArrivals(Vertex *vertex,
			  TagGroupBldr *tag_bldr)
{
  if (tag_bldr->empty())
    deletePaths(vertex);
  else {
    TagGroup *prev_tag_group = tagGroup(vertex);
    Arrival *prev_arrivals = graph_->arrivals(vertex);
    PathVertexRep *prev_paths = graph_->prevPaths(vertex);

    TagGroup *tag_group = findTagGroup(tag_bldr);
    int arrival_count = tag_group->arrivalCount();
    bool has_requireds = vertex->hasRequireds();
    // Reuse arrival array if it is the same size.
    if (prev_tag_group
	&& arrival_count == prev_tag_group->arrivalCount()) {
      if  (tag_bldr->hasClkTag() || tag_bldr->hasGenClkSrcTag()) {
	if (prev_paths == nullptr)
	  prev_paths = graph_->makePrevPaths(vertex, arrival_count);
      }
      else {
	// Prev paths not required.
	prev_paths = nullptr;
	vertex->setPrevPaths(prev_path_null);
      }
      tag_bldr->copyArrivals(tag_group, prev_arrivals, prev_paths);
      vertex->setTagGroupIndex(tag_group->index());
      if (tag_group->hasFilterTag())
        filtered_arrivals_->insert(vertex);

      if (has_requireds) {
	requiredInvalid(vertex);
        if (tag_group != prev_tag_group)
          // Requireds can only be reused if the tag group is unchanged.
          graph_->deleteRequireds(vertex, prev_tag_group->arrivalCount());
      }
    }
    else {
      if (prev_tag_group) {
        uint32_t prev_arrival_count = prev_tag_group->arrivalCount();
        graph_->deleteArrivals(vertex, prev_arrival_count);
        if (has_requireds) {
          requiredInvalid(vertex);
          graph_->deleteRequireds(vertex, prev_arrival_count);
        }
      }
      Arrival *arrivals = graph_->makeArrivals(vertex, arrival_count);
      prev_paths = nullptr;
      if  (tag_bldr->hasClkTag() || tag_bldr->hasGenClkSrcTag())
	prev_paths = graph_->makePrevPaths(vertex, arrival_count);
      tag_bldr->copyArrivals(tag_group, arrivals, prev_paths);

      vertex->setTagGroupIndex(tag_group->index());
      if (tag_group->hasFilterTag())
        filtered_arrivals_->insert(vertex);
    }
  }
}

void
Search::reportArrivals(Vertex *vertex) const
{
  report_->reportLine("Vertex %s", vertex->name(sdc_network_));
  TagGroup *tag_group = tagGroup(vertex);
  Arrival *arrivals = graph_->arrivals(vertex);
  Required *requireds = graph_->requireds(vertex);
  if (tag_group) {
    report_->reportLine("Group %u", tag_group->index());
    ArrivalMap::Iterator arrival_iter(tag_group->arrivalMap());
    while (arrival_iter.hasNext()) {
      Tag *tag;
      int arrival_index;
      arrival_iter.next(tag, arrival_index);
      PathAnalysisPt *path_ap = tag->pathAnalysisPt(this);
      const RiseFall *rf = tag->transition();
      const char *req = "?";
      if (requireds)
        req = delayAsString(requireds[arrival_index], this);
      bool report_clk_prev = false;
      const char *clk_prev = "";
      if (report_clk_prev
	  && tag_group->hasClkTag()) {
	PathVertex prev = check_crpr_->clkPathPrev(vertex, arrival_index);
        if (!prev.isNull())
          clk_prev = prev.name(this);
      }
      report_->reportLine(" %d %s %s %s / %s %s %s",
                          arrival_index,
                          rf->asString(),
                          path_ap->pathMinMax()->asString(),
                          delayAsString(arrivals[arrival_index], this),
                          req,
                          tag->asString(true, false, this),
                          clk_prev);
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
Search::reportArrivalCountHistogram() const
{
  Vector<int> vertex_counts(10);
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    TagGroup *tag_group = tagGroup(vertex);
    if (tag_group) {
      size_t arrival_count = tag_group->arrivalCount();
      if (arrival_count >= vertex_counts.size())
	vertex_counts.resize(arrival_count * 2);
      vertex_counts[arrival_count]++;
    }
  }

  for (size_t arrival_count = 0; arrival_count < vertex_counts.size(); arrival_count++) {
    int vertex_count = vertex_counts[arrival_count];
    if (vertex_count > 0)
      report_->reportLine("%6lu %6d", arrival_count, vertex_count);
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
Search::findTag(const RiseFall *rf,
		const PathAnalysisPt *path_ap,
		ClkInfo *clk_info,
		bool is_clk,
		InputDelay *input_delay,
		bool is_segment_start,
		ExceptionStateSet *states,
		bool own_states)
{
  Tag probe(0, rf->index(), path_ap->index(), clk_info, is_clk, input_delay,
	    is_segment_start, states, false, this);
  UniqueLock lock(tag_lock_);
  Tag *tag = tag_set_->findKey(&probe);
  if (tag == nullptr) {
    ExceptionStateSet *new_states = !own_states && states
      ? new ExceptionStateSet(*states) : states;
    TagIndex tag_index;
    if (tag_free_indices_.empty())
      tag_index = tag_next_++;
    else {
      tag_index = tag_free_indices_.back();
      tag_free_indices_.pop_back();
    }
    tag = new Tag(tag_index, rf->index(), path_ap->index(),
                  clk_info, is_clk, input_delay, is_segment_start,
                  new_states, true, this);
    own_states = false;
    // Make sure tag can be indexed in tags_ before it is visible to
    // other threads via tag_set_.
    tags_[tag_index] = tag;
    tag_set_->insert(tag);
    // If tags_ needs to grow make the new array and copy the
    // contents into it before updating tags_ so that other threads
    // can use Search::tag(TagIndex) without returning gubbish.
    // std::vector doesn't seem to follow this protocol so multi-thread
    // search fails occasionally if a vector is used for tags_.
    if (tag_next_ == tag_capacity_) {
      TagIndex new_capacity = nextMersenne(tag_capacity_);
      Tag **new_tags = new Tag*[new_capacity];
      memcpy(new_tags, tags_, tag_capacity_ * sizeof(Tag*));
      Tag **old_tags = tags_;
      tags_ = new_tags;
      delete [] old_tags;
      tag_capacity_ = new_capacity;
      tag_set_->reserve(new_capacity);
    }
    if (tag_next_ > tag_index_max)
      report_->critical(261, "max tag index exceeded");
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
      report_->reportLine("%s", tag->asString(this)) ;
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
  Vector<ClkInfo*> clk_infos;
  // set -> vector for sorting.
  for (ClkInfo *clk_info : *clk_info_set_)
    clk_infos.push_back(clk_info);
  sort(clk_infos, ClkInfoLess(this));
  for (ClkInfo *clk_info : clk_infos)
    report_->reportLine("ClkInfo %s", clk_info->asString(this));
  report_->reportLine("%lu clk infos", clk_info_set_->size());
}

ClkInfo *
Search::findClkInfo(const ClockEdge *clk_edge,
		    const Pin *clk_src,
		    bool is_propagated,
                    const Pin *gen_clk_src,
		    bool gen_clk_src_path,
		    const RiseFall *pulse_clk_sense,
		    Arrival insertion,
		    float latency,
		    ClockUncertainties *uncertainties,
                    const PathAnalysisPt *path_ap,
		    PathVertex *crpr_clk_path)
{
  PathVertexRep crpr_clk_path_rep(crpr_clk_path, this);
  ClkInfo probe(clk_edge, clk_src, is_propagated, gen_clk_src, gen_clk_src_path,
		pulse_clk_sense, insertion, latency, uncertainties,
		path_ap->index(), crpr_clk_path_rep, this);
  UniqueLock lock(clk_info_lock_);
  ClkInfo *clk_info = clk_info_set_->findKey(&probe);
  if (clk_info == nullptr) {
    clk_info = new ClkInfo(clk_edge, clk_src,
			   is_propagated, gen_clk_src, gen_clk_src_path,
			   pulse_clk_sense, insertion, latency, uncertainties,
			   path_ap->index(), crpr_clk_path_rep, this);
    clk_info_set_->insert(clk_info);
  }
  return clk_info;
}

ClkInfo *
Search::findClkInfo(const ClockEdge *clk_edge,
		    const Pin *clk_src,
		    bool is_propagated,
		    Arrival insertion,
		    const PathAnalysisPt *path_ap)
{
  return findClkInfo(clk_edge, clk_src, is_propagated, nullptr, false, nullptr,
		     insertion, 0.0, nullptr, path_ap, nullptr);
}

int
Search::clkInfoCount() const
{
  return clk_info_set_->size();
}

ArcDelay
Search::deratedDelay(Vertex *from_vertex,
		     TimingArc *arc,
		     Edge *edge,
		     bool is_clk,
		     const PathAnalysisPt *path_ap)
{
  const DcalcAnalysisPt *dcalc_ap = path_ap->dcalcAnalysisPt();
  DcalcAPIndex ap_index = dcalc_ap->index();
  float derate = timingDerate(from_vertex, arc, edge, is_clk, path_ap);
  ArcDelay delay = graph_->arcDelay(edge, arc, ap_index);
  return delay * derate;
}

float
Search::timingDerate(Vertex *from_vertex,
		     TimingArc *arc,
		     Edge *edge,
		     bool is_clk,
		     const PathAnalysisPt *path_ap)
{
  PathClkOrData derate_clk_data =
    is_clk ? PathClkOrData::clk : PathClkOrData::data;
  TimingRole *role = edge->role();
  const Pin *pin = from_vertex->pin();
  if (role->isWire()) {
    const RiseFall *rf = arc->toEdge()->asRiseFall();
    return sdc_->timingDerateNet(pin, derate_clk_data, rf,
				 path_ap->pathMinMax());
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
    return sdc_->timingDerateInstance(pin, derate_type, derate_clk_data, rf,
				      path_ap->pathMinMax());
  }
}

ClockSet
Search::clockDomains(const Vertex *vertex) const
{
  ClockSet clks;
  clockDomains(vertex, clks);
  return clks;
}

void
Search::clockDomains(const Vertex *vertex,
                     // Return value.
                     ClockSet &clks) const
{
  VertexPathIterator path_iter(const_cast<Vertex*>(vertex), this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    const Clock *clk = path->clock(this);
    if (clk)
      clks.insert(const_cast<Clock *>(clk));
  }
}

ClockSet
Search::clockDomains(const Pin *pin) const
{
  ClockSet clks;
  Vertex *vertex;
  Vertex *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  if (vertex)
    clockDomains(vertex, clks);
  if (bidirect_drvr_vertex)
    clockDomains(bidirect_drvr_vertex, clks);
  return clks;
}

ClockSet
Search::clocks(const Vertex *vertex) const
{
  ClockSet clks;
  clocks(vertex, clks);
  return clks;
}

void
Search::clocks(const Vertex *vertex,
	       // Return value.
	       ClockSet &clks) const
{
  VertexPathIterator path_iter(const_cast<Vertex*>(vertex), this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    if (path->isClock(this))
      clks.insert(const_cast<Clock *>(path->clock(this)));
  }
}

ClockSet
Search::clocks(const Pin *pin) const
{
  ClockSet clks;
  Vertex *vertex;
  Vertex *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  if (vertex)
    clocks(vertex, clks);
  if (bidirect_drvr_vertex)
    clocks(bidirect_drvr_vertex, clks);
  return clks;
}

bool
Search::isClock(const Vertex *vertex) const
{
  TagGroup *tag_group = tagGroup(vertex);
  if (tag_group)
    return tag_group->hasClkTag();
  else
    return false;
}

bool
Search::isGenClkSrc(const Vertex *vertex) const
{
  TagGroup *tag_group = tagGroup(vertex);
  if (tag_group)
    return tag_group->hasGenClkSrcTag();
  else
    return false;
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
  requireds_exist_ = true;
  debugPrint(debug_, "search", 1, "found %d requireds", required_count);
  stats.report("Find requireds");
}

void
Search::seedRequireds()
{
  ensureDownstreamClkPins();
  for (Vertex *vertex : *endpoints())
    seedRequired(vertex);
  requireds_seeded_ = true;
  requireds_exist_ = true;
}

VertexSet *
Search::endpoints()
{
  if (endpoints_ == nullptr) {
    endpoints_ = new VertexSet(graph_);
    invalid_endpoints_ = new VertexSet(graph_);
    VertexIterator vertex_iter(graph_);
    while (vertex_iter.hasNext()) {
      Vertex *vertex = vertex_iter.next();
      if (isEndpoint(vertex)) {
	debugPrint(debug_, "endpoint", 2, "insert %s",
                   vertex->name(sdc_network_));
	endpoints_->insert(vertex);
      }
    }
  }
  if (invalid_endpoints_) {
    for (Vertex *vertex : *invalid_endpoints_) {
      if (isEndpoint(vertex)) {
	debugPrint(debug_, "endpoint", 2, "insert %s",
                   vertex->name(sdc_network_));
	endpoints_->insert(vertex);
      }
      else {
	if (debug_->check("endpoint", 2)
	    && endpoints_->hasKey(vertex))
	  report_->reportLine("endpoint: remove %s",
                              vertex->name(sdc_network_));
	endpoints_->erase(vertex);
      }
    }
    invalid_endpoints_->clear();
  }
  return endpoints_;
}

void
Search::endpointInvalid(Vertex *vertex)
{
  if (invalid_endpoints_) {
    debugPrint(debug_, "endpoint", 2, "invalid %s",
               vertex->name(sdc_network_));
    invalid_endpoints_->insert(vertex);
  }
}

bool
Search::isEndpoint(Vertex *vertex) const
{
  return isEndpoint(vertex, search_adj_);
}

bool
Search::isEndpoint(Vertex *vertex,
		   SearchPred *pred) const
{
  Pin *pin = vertex->pin();
  return hasFanin(vertex, pred, graph_)
    && ((vertex->hasChecks()
	 && hasEnabledChecks(vertex))
	|| (sdc_->gatedClkChecksEnabled()
	    && gated_clk_->isGatedClkEnable(vertex))
	|| vertex->isConstrained()
	|| sdc_->isPathDelayInternalEndpoint(pin)
	|| !hasFanout(vertex, pred, graph_)
	// Unconstrained paths at register clk pins.
	|| (unconstrained_paths_
	    && vertex->isRegClk()));
}

bool
Search::hasEnabledChecks(Vertex *vertex) const
{
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (visit_path_ends_->checkEdgeEnabled(edge))
      return true;
  }
  return false;
}

void
Search::endpointsInvalid()
{
  delete endpoints_;
  delete invalid_endpoints_;
  endpoints_ = nullptr;
  invalid_endpoints_ = nullptr;
}

void
Search::seedInvalidRequireds()
{
  for (Vertex *vertex : *invalid_requireds_)
    required_iter_->enqueue(vertex);
  invalid_requireds_->clear();
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
    PathRef &path = path_end->pathRef();
    const MinMax *req_min = path.minMax(sta_)->opposite();
    int arrival_index;
    bool arrival_exists;
    path.arrivalIndex(arrival_index, arrival_exists);
    Required required = path_end->requiredTime(sta_);
    required_cmp_->requiredSet(arrival_index, required, req_min, sta_);
  }
}

void
Search::seedRequired(Vertex *vertex)
{
  debugPrint(debug_, "search", 2, "required seed %s",
             vertex->name(sdc_network_));
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
    requireds_.resize(tag_group->arrivalCount());
    ArrivalMap *arrival_entries = tag_group->arrivalMap();
    ArrivalMap::Iterator arrival_iter(arrival_entries);
    while (arrival_iter.hasNext()) {
      Tag *tag;
      int arrival_index;
      arrival_iter.next(tag, arrival_index);
      PathAnalysisPt *path_ap = tag->pathAnalysisPt(sta);
      const MinMax *min_max = path_ap->pathMinMax();
      requireds_[arrival_index] = delayInitValue(min_max->opposite());
    }
  }
  else
    requireds_.resize(0);
  have_requireds_ = false;
}

void
RequiredCmp::requiredSet(int arrival_index,
			 Required required,
			 const MinMax *min_max,
			 const StaState *sta)
{
  if (delayGreater(required, requireds_[arrival_index], min_max, sta)) {
    requireds_[arrival_index] = required;
    have_requireds_ = true;
  }
}

bool
RequiredCmp::requiredsSave(Vertex *vertex,
			   const StaState *sta)
{
  bool requireds_changed = false;
  bool prev_reqs = vertex->hasRequireds();
  if (have_requireds_) {
    if (!prev_reqs)
      requireds_changed = true;
    Debug *debug = sta->debug();
    VertexPathIterator path_iter(vertex, sta);
    while (path_iter.hasNext()) {
      PathVertex *path = path_iter.next();
      int arrival_index;
      bool arrival_exists;
      path->arrivalIndex(arrival_index, arrival_exists);
      Required req = requireds_[arrival_index];
      if (prev_reqs) {
	Required prev_req = path->required(sta);
	if (!delayEqual(prev_req, req)) {
	  debugPrint(debug, "search", 3, "required save %s -> %s",
                     delayAsString(prev_req, sta),
                     delayAsString(req, sta));
	  path->setRequired(req, sta);
	  requireds_changed = true;
	}
      }
      else {
	debugPrint(debug, "search", 3, "required save MIA -> %s",
                   delayAsString(req, sta));
	path->setRequired(req, sta);
      }
    }
  }
  else if (prev_reqs) {
    Graph *graph = sta->graph();
    const Search *search = sta->search();
    TagGroup *tag_group = search->tagGroup(vertex);
    if (tag_group == nullptr)
      requireds_changed = true;
    else {
      int arrival_count = tag_group->arrivalCount();
      graph->deleteRequireds(vertex, arrival_count);
      requireds_changed = true;
    }
  }
  return requireds_changed;
}

Required
RequiredCmp::required(int arrival_index)
{
  return requireds_[arrival_index];
}

////////////////////////////////////////////////////////////////

RequiredVisitor::RequiredVisitor(const StaState *sta) :
  PathVisitor(sta),
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
  return new RequiredVisitor(this);
}

void
RequiredVisitor::visit(Vertex *vertex)
{
  debugPrint(debug_, "search", 2, "find required %s",
             vertex->name(network_));
  required_cmp_->requiredsInit(vertex, this);
  vertex->setRequiredsPruned(false);
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
				 Vertex *from_vertex,
				 const RiseFall *from_rf,
				 Tag *from_tag,
				 PathVertex *from_path,
				 Edge *edge,
				 TimingArc *,
				 ArcDelay arc_delay,
				 Vertex *to_vertex,
				 const RiseFall *to_rf,
				 Tag *to_tag,
				 Arrival &,
				 const MinMax *min_max,
				 const PathAnalysisPt *path_ap)
{
  // Don't propagate required times through latch D->Q edges.
  if (edge->role() != TimingRole::latchDtoQ()) {
    debugPrint(debug_, "search", 3, "  %s -> %s %s",
               from_rf->asString(),
               to_rf->asString(),
               min_max->asString());
    debugPrint(debug_, "search", 3, "  from tag %2u: %s",
               from_tag->index(),
               from_tag->asString(this));
    int arrival_index;
    bool arrival_exists;
    from_path->arrivalIndex(arrival_index, arrival_exists);
    const MinMax *req_min = min_max->opposite();
    TagGroup *to_tag_group = search_->tagGroup(to_vertex);
    // Check to see if to_tag was pruned.
    if (to_tag_group && to_tag_group->hasTag(to_tag)) {
      PathVertex to_path(to_vertex, to_tag, this);
      Required to_required = to_path.required(this);
      Required from_required = to_required - arc_delay;
      debugPrint(debug_, "search", 3, "  to tag   %2u: %s",
                 to_tag->index(),
                 to_tag->asString(this));
      debugPrint(debug_, "search", 3, "  %s - %s = %s %s %s",
                 delayAsString(to_required, this),
                 delayAsString(arc_delay, this),
                 delayAsString(from_required, this),
                 min_max == MinMax::max() ? "<" : ">",
                 delayAsString(required_cmp_->required(arrival_index), this));
      required_cmp_->requiredSet(arrival_index, from_required, req_min, this);
    }
    else {
      if (search_->crprApproxMissingRequireds()) {
	// Arrival on to_vertex that differs by crpr_pin was pruned.
	// Find an arrival that matches everything but the crpr_pin
	// as an appromate required.
	VertexPathIterator to_iter(to_vertex, to_rf, path_ap, this);
	while (to_iter.hasNext()) {
	  PathVertex *to_path = to_iter.next();
	  Tag *to_path_tag = to_path->tag(this);
	  if (tagMatchNoCrpr(to_path_tag, to_tag)) {
	    Required to_required = to_path->required(this);
	    Required from_required = to_required - arc_delay;
	    debugPrint(debug_, "search", 3, "  to tag   %2u: %s",
                       to_path_tag->index(),
                       to_path_tag->asString(this));
	    debugPrint(debug_, "search", 3, "  %s - %s = %s %s %s",
                       delayAsString(to_required, this),
                       delayAsString(arc_delay, this),
                       delayAsString(from_required, this),
                       min_max == MinMax::max() ? "<" : ">",
                       delayAsString(required_cmp_->required(arrival_index),
                                     this));
	    required_cmp_->requiredSet(arrival_index, from_required, req_min, this);
	    break;
	  }
	}
      }
      from_vertex->setRequiredsPruned(true);
    }
    // Propagate requireds pruned flag backwards.
    if (to_vertex->requiredsPruned())
      from_vertex->setRequiredsPruned(true);
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
    for (Vertex *vertex : *graph_->regClkVertices())
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
  if (filter_ == nullptr
      && filter_from_ == nullptr
      && filter_to_ == nullptr)
    return true;
  else if (filter_) {
    // -from pins|inst
    // -thru
    // Path has to be tagged by traversing the filter exception points.
    ExceptionStateSet *states = path->tag(this)->states();
    if (states) {
      for (auto state : *states) {
	if (state->exception() == filter_
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
    RiseFall *path_clk_rf =
      path_clk_edge ? path_clk_edge->transition() : nullptr;
    return filter_from_->clks()->hasKey(const_cast<Clock*>(path_clk))
      && filter_from_->transition()->matches(path_clk_rf)
      && matchesFilterTo(path, to_clk_edge);
  }
  else if (filter_from_ == nullptr
	   && filter_to_)
    // -to
    return matchesFilterTo(path, to_clk_edge);
  else {
    report_->critical(262, "unexpected filter path");
    return false;
  }
}

// Similar to Constraints::exceptionMatchesTo.
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
		    bool require_to_pin) const
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
	  && sdc_->isCompleteTo(state, pin, rf, clk_edge, min_max,
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
  sdc_->exceptionTo(type, pin, rf, clk_edge, min_max,
		    match_min_max_exactly,
		    hi_priority_exception, hi_priority);
  return hi_priority_exception;
}

////////////////////////////////////////////////////////////////

Slack
Search::totalNegativeSlack(const MinMax *min_max)
{
  tnsPreamble();
  Slack tns = 0.0;
  for (Corner *corner : *corners_) {
    PathAPIndex path_ap_index = corner->findPathAnalysisPt(min_max)->index();
    Slack tns1 = tns_[path_ap_index];
    if (delayLess(tns1, tns, this))
      tns = tns1;
  }
  return tns;
}

Slack
Search::totalNegativeSlack(const Corner *corner,
			   const MinMax *min_max)
{
  tnsPreamble();
  PathAPIndex path_ap_index = corner->findPathAnalysisPt(min_max)->index();
  return tns_[path_ap_index];
}

void
Search::tnsPreamble()
{
  wnsTnsPreamble();
  PathAPIndex path_ap_count = corners_->pathAnalysisPtCount();
  tns_.resize(path_ap_count);
  tns_slacks_.resize(path_ap_count);
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
               vertex->name(sdc_network_));
    UniqueLock lock(tns_lock_);
    invalid_tns_->insert(vertex);
  }
}

void
Search::updateInvalidTns()
{
  PathAPIndex path_ap_count = corners_->pathAnalysisPtCount();
  for (Vertex *vertex : *invalid_tns_) {
    // Network edits can change endpointedness since tnsInvalid was called.
    if (isEndpoint(vertex)) {
      debugPrint(debug_, "tns", 2, "update tns %s",
                 vertex->name(sdc_network_));
      SlackSeq slacks(path_ap_count);
      wnsSlacks(vertex, slacks);

      if (tns_exists_)
	updateTns(vertex, slacks);
      if (worst_slacks_)
	worst_slacks_->updateWorstSlacks(vertex, slacks);
    }
  }
  invalid_tns_->clear();
}

void
Search::findTotalNegativeSlacks()
{
  PathAPIndex path_ap_count = corners_->pathAnalysisPtCount();
  for (PathAPIndex i = 0; i < path_ap_count; i++) {
    tns_[i] = 0.0;
    tns_slacks_[i].clear();
  }
  for (Vertex *vertex : *endpoints()) {
    // No locking required.
    SlackSeq slacks(path_ap_count);
    wnsSlacks(vertex, slacks);
    for (PathAPIndex i = 0; i < path_ap_count; i++)
      tnsIncr(vertex, slacks[i], i);
  }
  tns_exists_ = true;
}

void
Search::updateTns(Vertex *vertex,
		  SlackSeq &slacks)
{
  PathAPIndex path_ap_count = corners_->pathAnalysisPtCount();
  for (PathAPIndex i = 0; i < path_ap_count; i++) {
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
               vertex->name(sdc_network_));
    tns_[path_ap_index] += slack;
    if (tns_slacks_[path_ap_index].hasKey(vertex))
      report_->critical(263, "tns incr existing vertex");
    tns_slacks_[path_ap_index][vertex] = slack;
  }
}

void
Search::tnsDecr(Vertex *vertex,
		PathAPIndex path_ap_index)
{
  Slack slack;
  bool found;
  tns_slacks_[path_ap_index].findKey(vertex, slack, found);
  if (found
      && delayLess(slack, 0.0, this)) {
    debugPrint(debug_, "tns", 3, "tns- %s %s",
               delayAsString(slack, this),
               vertex->name(sdc_network_));
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
    int ap_count = corners_->pathAnalysisPtCount();
    for (int i = 0; i < ap_count; i++) {
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
Search::worstSlack(const Corner *corner,
		   const MinMax *min_max,
		   // Return values.
		   Slack &worst_slack,
		   Vertex *&worst_vertex)
{
  worstSlackPreamble();
  worst_slacks_->worstSlack(corner, min_max, worst_slack, worst_vertex);
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
    for (auto itr = invalid_requireds_->begin(); itr != invalid_requireds_->end(); ) {
      Vertex *vertex = *itr;
      debugPrint(debug_, "search", 2, "tns update required %s",
                 vertex->name(sdc_network_));
      if (isEndpoint(vertex)) {
	seedRequired(vertex);
	// If the endpoint has fanout it's required time
	// depends on downstream checks, so enqueue it to
	// force required propagation to it's level if
	// the required time is requested later.
	if (hasFanout(vertex, search_adj_, graph_))
	  required_iter_->enqueue(vertex);
        itr = invalid_requireds_->erase(itr);
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
    PathRef &path = path_end->pathRef();
    PathAPIndex path_ap_index = path.pathAnalysisPtIndex(sta_);
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
  PathAPIndex path_ap_count = corners_->pathAnalysisPtCount();
  for (PathAPIndex i = 0; i < path_ap_count; i++)
    slacks[i] = slack_init;
  if (hasFanout(vertex, search_adj_, graph_)) {
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
  PathAPIndex path_ap_count = corners_->pathAnalysisPtCount();
  SlackSeq slacks(path_ap_count);
  wnsSlacks(vertex, slacks);
  return slacks[path_ap_index];
}

////////////////////////////////////////////////////////////////

PathGroups *
Search::makePathGroups(int group_count,
		       int endpoint_count,
		       bool unique_pins,
		       float slack_min,
		       float slack_max,
		       PathGroupNameSet *group_names,
		       bool setup,
		       bool hold,
		       bool recovery,
		       bool removal,
		       bool clk_gating_setup,
		       bool clk_gating_hold)
{
  return new PathGroups(group_count, endpoint_count, unique_pins,
			slack_min, slack_max,
			group_names,
			setup, hold,
			recovery, removal,
			clk_gating_setup, clk_gating_hold,
			unconstrained_paths_,
			this);
}

void
Search::deletePathGroups()
{
  delete path_groups_;
  path_groups_ = nullptr;
}

PathGroup *
Search::pathGroup(const PathEnd *path_end) const
{
  if (path_groups_)
    return path_groups_->pathGroup(path_end);
  else
    return nullptr;
}

bool
Search::havePathGroups() const
{
  return path_groups_ != nullptr;
}

PathGroup *
Search::findPathGroup(const char *name,
		      const MinMax *min_max) const
{
  if (path_groups_)
    return path_groups_->findPathGroup(name, min_max);
  else
    return nullptr;
}

PathGroup *
Search::findPathGroup(const Clock *clk,
		      const MinMax *min_max) const
{
  if (path_groups_)
    return path_groups_->findPathGroup(clk, min_max);
  else
    return nullptr;
}

} // namespace
