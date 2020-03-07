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

#include "Machine.hh"
#include "Mutex.hh"
#include "Debug.hh"
#include "Report.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "Search.hh"
#include "PathAnalysisPt.hh"
#include "WorstSlack.hh"

namespace sta {

using std::min;

WorstSlacks::WorstSlacks(StaState *sta) :
  sta_(sta)
{
  worst_slacks_.resize(sta->corners()->pathAnalysisPtCount());
}

void
WorstSlacks::worstSlack(const MinMax *min_max,
			// Return values.
			Slack &worst_slack,
			Vertex *&worst_vertex)
{
  worst_slack = MinMax::min()->initValue();
  worst_vertex = nullptr;
  for (auto corner : *sta_->corners()) {
    PathAPIndex path_ap_index = corner->findPathAnalysisPt(min_max)->index();
    Slack worst_slack1;
    Vertex *worst_vertex1;
    worst_slacks_[path_ap_index].worstSlack(path_ap_index, sta_,
					    worst_slack1, worst_vertex1);
    if (fuzzyLess(worst_slack1, worst_slack)) {
      worst_slack = worst_slack1;
      worst_vertex = worst_vertex1;
    }
  }
}

void
WorstSlacks::worstSlack(const Corner *corner,
			const MinMax *min_max,
			// Return values.
			Slack &worst_slack,
			Vertex *&worst_vertex)
{
  PathAPIndex path_ap_index = corner->findPathAnalysisPt(min_max)->index();
  worst_slacks_[path_ap_index].worstSlack(path_ap_index, sta_,
					  worst_slack, worst_vertex);
}

void
WorstSlacks::updateWorstSlacks(Vertex *vertex,
			       SlackSeq &slacks)
{
  PathAPIndex path_ap_count = sta_->corners()->pathAnalysisPtCount();
  for (PathAPIndex i = 0; i < path_ap_count; i++)
    worst_slacks_[i].updateWorstSlack(vertex, slacks, i, sta_);
}

void
WorstSlacks::worstSlackNotifyBefore(Vertex *vertex)
{
  WorstSlackSeq::Iterator worst_iter(worst_slacks_);
  while (worst_iter.hasNext()) {
    WorstSlack &worst_slack = worst_iter.next();
    worst_slack.deleteVertexBefore(vertex);
  }
}

////////////////////////////////////////////////////////////////

WorstSlack::WorstSlack() :
  slack_init_(MinMax::min()->initValue()),
  worst_vertex_(nullptr),
  worst_slack_(slack_init_),
  slack_threshold_(slack_init_),
  min_queue_size_(10),
  max_queue_size_(20)
{
}

WorstSlack::WorstSlack(const WorstSlack &) :
  slack_init_(MinMax::min()->initValue()),
  worst_vertex_(nullptr),
  worst_slack_(slack_init_),
  slack_threshold_(slack_init_),
  min_queue_size_(10),
  max_queue_size_(20)
{
}

void
WorstSlack::deleteVertexBefore(Vertex *vertex)
{
  UniqueLock lock(lock_);
  if (vertex == worst_vertex_) {
    worst_vertex_ = nullptr;
    worst_slack_ = slack_init_;
  }
  queue_.erase(vertex);
}

void
WorstSlack::worstSlack(PathAPIndex path_ap_index,
		       const StaState *sta,
		       // Return values.
		       Slack &worst_slack,
		       Vertex *&worst_vertex)
{
  findWorstSlack(path_ap_index, sta);
  worst_slack = worst_slack_;
  worst_vertex = worst_vertex_;
}

void
WorstSlack::findWorstSlack(PathAPIndex path_ap_index,
			   const StaState *sta)
{
  if (worst_vertex_ == nullptr) {
    if (queue_.empty())
      initQueue(path_ap_index, sta);
    else
      findWorstInQueue(path_ap_index, sta);
  }
}

void
WorstSlack::initQueue(PathAPIndex path_ap_index,
		      const StaState *sta)
{
  Search *search = sta->search();
  const Debug *debug = sta->debug();
  debugPrint0(debug, "wns", 3, "init queue\n");

  queue_.clear();
  worst_vertex_ = nullptr;
  worst_slack_ = slack_init_;
  slack_threshold_ = slack_init_;
  VertexSet::Iterator end_iter(search->endpoints());
  while (end_iter.hasNext()) {
    Vertex *vertex = end_iter.next();
    Slack slack = search->wnsSlack(vertex, path_ap_index);
    if (!fuzzyEqual(slack, slack_init_)) {
      if (fuzzyLess(slack, worst_slack_))
	setWorstSlack(vertex, slack, sta);
      if (fuzzyLessEqual(slack, slack_threshold_))
	queue_.insert(vertex);
      int queue_size = queue_.size();
      if (queue_size >= max_queue_size_)
	sortQueue(path_ap_index, sta);
    }
  }
  debugPrint1(debug, "wns", 3, "threshold %s\n",
	      delayAsString(slack_threshold_, sta));
//  checkQueue();
}

void
WorstSlack::sortQueue(PathAPIndex path_ap_index,
		      const StaState *sta)
{
  Search *search = sta->search();
  const Debug *debug = sta->debug();
  debugPrint0(debug, "wns", 3, "sort queue\n");

  VertexSeq vertices;
  vertices.reserve(queue_.size());
  VertexSet::Iterator queue_iter(queue_);
  while (queue_iter.hasNext()) {
    Vertex *vertex = queue_iter.next();
    vertices.push_back(vertex);
  }
  WnsSlackLess slack_less(path_ap_index, sta);
  sort(vertices, slack_less);

  int vertex_count = vertices.size();
  int threshold_index = min(min_queue_size_, vertex_count - 1);
  Vertex *threshold_vertex = vertices[threshold_index];
  slack_threshold_ = search->wnsSlack(threshold_vertex, path_ap_index);
  debugPrint1(debug, "wns", 3, "threshold %s\n",
	      delayAsString(slack_threshold_, sta));

  // Reinsert vertices with slack < threshold.
  queue_.clear();
  VertexSeq::Iterator queue_iter2(vertices);
  while (queue_iter2.hasNext()) {
    Vertex *vertex = queue_iter2.next();
    Slack slack = search->wnsSlack(vertex, path_ap_index);
    if (fuzzyGreater(slack, slack_threshold_))
      break;
    queue_.insert(vertex);
  }
  max_queue_size_ = queue_.size() * 2;
  Vertex *worst_slack_vertex = vertices[0];
  Slack worst_slack_slack = search->wnsSlack(worst_slack_vertex, path_ap_index);
  setWorstSlack(worst_slack_vertex, worst_slack_slack, sta);
}

void
WorstSlack::findWorstInQueue(PathAPIndex path_ap_index,
			     const StaState *sta)
{
  Search *search = sta->search();
  const Debug *debug = sta->debug();
  debugPrint0(debug, "wns", 3, "find worst in queue\n");

  worst_vertex_ = nullptr;
  worst_slack_ = slack_init_;
  VertexSet::Iterator queue_iter(queue_);
  while (queue_iter.hasNext()) {
    Vertex *vertex = queue_iter.next();
    Slack slack = search->wnsSlack(vertex, path_ap_index);
    if (slack < worst_slack_)
      setWorstSlack(vertex, slack, sta);
  }
}

void
WorstSlack::checkQueue(PathAPIndex path_ap_index,
		       const StaState *sta)
{
  Search *search = sta->search();
  Report *report = sta->report();
  const Network *network = sta->network();

  VertexSeq ends;
  VertexSet::Iterator end_iter(search->endpoints());
  while (end_iter.hasNext()) {
    Vertex *end = end_iter.next();
    if (fuzzyLessEqual(search->wnsSlack(end, path_ap_index),
			    slack_threshold_))
      ends.push_back(end);
  }
  WnsSlackLess slack_less(path_ap_index, sta);
  sort(ends, slack_less);

  VertexSet end_set;
  VertexSeq::Iterator end_iter2(ends);
  while (end_iter2.hasNext()) {
    Vertex *end = end_iter2.next();
    end_set.insert(end);
    if (!queue_.hasKey(end)
	&& fuzzyLessEqual(search->wnsSlack(end, path_ap_index),
			       slack_threshold_))
      report->print("WorstSlack queue missing %s %s < %s\n",
		    end->name(network),
		    delayAsString(search->wnsSlack(end, path_ap_index), sta),
		    delayAsString(slack_threshold_, sta));
  }

  VertexSet::Iterator queue_iter(queue_);
  while (queue_iter.hasNext()) {
    Vertex *end = queue_iter.next();
    if (!end_set.hasKey(end))
      report->print("WorstSlack queue extra %s %s > %s\n",
		    end->name(network),
		    delayAsString(search->wnsSlack(end, path_ap_index), sta),
		    delayAsString(slack_threshold_, sta));
  }
}

void
WorstSlack::updateWorstSlack(Vertex *vertex,
			     SlackSeq &slacks,
			     PathAPIndex path_ap_index,
			     const StaState *sta)
{
  const Debug *debug = sta->debug();
  const Network *network = sta->network();
  Slack slack = slacks[path_ap_index];

  // Locking is required because ArrivalVisitor is called by multiple
  // threads.
  UniqueLock lock(lock_);
  if (worst_vertex_
      && fuzzyLess(slack, worst_slack_))
    setWorstSlack(vertex, slack, sta);
  else if (vertex == worst_vertex_)
    // Mark worst slack as unknown (updated by findWorstSlack().
    worst_vertex_ = nullptr;

  if (!fuzzyEqual(slack, slack_init_)
      && fuzzyLessEqual(slack, slack_threshold_)) {
    debugPrint2(debug, "wns", 3, "insert %s %s\n",
		vertex->name(network),
		delayAsString(slack, sta));
    queue_.insert(vertex);
  }
  else {
    debugPrint2(debug, "wns", 3, "delete %s %s\n",
		vertex->name(network),
		delayAsString(slack, sta));
    queue_.erase(vertex);
  }
  //  checkQueue();
}

void
WorstSlack::setWorstSlack(Vertex *vertex,
			  Slack slack,
			  const StaState *sta)
{
  debugPrint2(sta->debug(), "wns", 3, "%s %s\n",
	      vertex->name(sta->network()),
	      delayAsString(slack, sta));
  worst_vertex_ = vertex;
  worst_slack_ = slack;
}

////////////////////////////////////////////////////////////////

WnsSlackLess::WnsSlackLess(PathAPIndex path_ap_index,
			   const StaState *sta) :
  path_ap_index_(path_ap_index),
  search_(sta->search())
{
}

bool
WnsSlackLess::operator()(Vertex *vertex1,
			 Vertex *vertex2)
{
  return fuzzyLess(search_->wnsSlack(vertex1, path_ap_index_),
			search_->wnsSlack(vertex2, path_ap_index_));
}

} // namespace
