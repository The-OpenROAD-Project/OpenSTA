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

#include "WorstSlack.hh"

#include "Debug.hh"
#include "Report.hh"
#include "Mutex.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "Search.hh"
#include "PathAnalysisPt.hh"

namespace sta {

using std::min;

WorstSlacks::WorstSlacks(StaState *sta) :
  worst_slacks_(sta->corners()->pathAnalysisPtCount(), sta),
  sta_(sta)
{
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
    worst_slacks_[path_ap_index].worstSlack(path_ap_index,
					    worst_slack1, worst_vertex1);
    if (delayLess(worst_slack1, worst_slack, sta_)) {
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
  worst_slacks_[path_ap_index].worstSlack(path_ap_index,
					  worst_slack, worst_vertex);
}

void
WorstSlacks::updateWorstSlacks(Vertex *vertex,
			       SlackSeq &slacks)
{
  PathAPIndex path_ap_count = sta_->corners()->pathAnalysisPtCount();
  for (PathAPIndex i = 0; i < path_ap_count; i++)
    worst_slacks_[i].updateWorstSlack(vertex, slacks, i);
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

WorstSlack::WorstSlack(StaState *sta) :
  StaState(sta),
  slack_init_(MinMax::min()->initValue()),
  worst_vertex_(nullptr),
  worst_slack_(slack_init_),
  slack_threshold_(slack_init_),
  queue_(new VertexSet(graph_)),
  min_queue_size_(10),
  max_queue_size_(20)
{
}

WorstSlack::~WorstSlack()
{
  delete queue_;
}

WorstSlack::WorstSlack(const WorstSlack &worst_slack) :
  StaState(worst_slack),
  slack_init_(MinMax::min()->initValue()),
  worst_vertex_(nullptr),
  worst_slack_(slack_init_),
  slack_threshold_(slack_init_),
  queue_(new VertexSet(graph_)),
  min_queue_size_(10),
  max_queue_size_(20)
{
}

void
WorstSlack::deleteVertexBefore(Vertex *vertex)
{
  LockGuard lock(lock_);
  if (vertex == worst_vertex_) {
    worst_vertex_ = nullptr;
    worst_slack_ = slack_init_;
  }
  queue_->erase(vertex);
}

void
WorstSlack::worstSlack(PathAPIndex path_ap_index,
		       // Return values.
		       Slack &worst_slack,
		       Vertex *&worst_vertex)
{
  findWorstSlack(path_ap_index);
  worst_slack = worst_slack_;
  worst_vertex = worst_vertex_;
}

void
WorstSlack::findWorstSlack(PathAPIndex path_ap_index)
{
  if (worst_vertex_ == nullptr) {
    if (queue_->empty())
      initQueue(path_ap_index);
    else
      findWorstInQueue(path_ap_index);
  }
}

void
WorstSlack::initQueue(PathAPIndex path_ap_index)
{
  debugPrint(debug_, "wns", 3, "init queue");

  queue_->clear();
  worst_vertex_ = nullptr;
  worst_slack_ = slack_init_;
  slack_threshold_ = slack_init_;
  for(Vertex *vertex : *search_->endpoints()) {
    Slack slack = search_->wnsSlack(vertex, path_ap_index);
    if (!delayEqual(slack, slack_init_)) {
      if (delayLess(slack, worst_slack_, this))
	setWorstSlack(vertex, slack);
      if (delayLessEqual(slack, slack_threshold_, this))
	queue_->insert(vertex);
      int queue_size = queue_->size();
      if (queue_size >= max_queue_size_)
	sortQueue(path_ap_index);
    }
  }
  debugPrint(debug_, "wns", 3, "threshold %s",
             delayAsString(slack_threshold_, this));
//  checkQueue();
}

void
WorstSlack::sortQueue(PathAPIndex path_ap_index)
{
  if (queue_->size() > 0) {
    debugPrint(debug_, "wns", 3, "sort queue");

    VertexSeq vertices;
    vertices.reserve(queue_->size());
    for (Vertex *vertex : *queue_)
      vertices.push_back(vertex);
    WnsSlackLess slack_less(path_ap_index, this);
    sort(vertices, slack_less);

    int vertex_count = vertices.size();
    int threshold_index = min(min_queue_size_, vertex_count - 1);
    Vertex *threshold_vertex = vertices[threshold_index];
    slack_threshold_ = search_->wnsSlack(threshold_vertex, path_ap_index);
    debugPrint(debug_, "wns", 3, "threshold %s",
               delayAsString(slack_threshold_, this));

    // Reinsert vertices with slack < threshold.
    queue_->clear();
    VertexSeq::Iterator queue_iter2(vertices);
    while (queue_iter2.hasNext()) {
      Vertex *vertex = queue_iter2.next();
      Slack slack = search_->wnsSlack(vertex, path_ap_index);
      if (delayGreater(slack, slack_threshold_, this))
	break;
      queue_->insert(vertex);
    }
    max_queue_size_ = queue_->size() * 2;
    Vertex *worst_slack_vertex = vertices[0];
    Slack worst_slack_slack = search_->wnsSlack(worst_slack_vertex, path_ap_index);
    setWorstSlack(worst_slack_vertex, worst_slack_slack);
  }
}

void
WorstSlack::findWorstInQueue(PathAPIndex path_ap_index)
{
  debugPrint(debug_, "wns", 3, "find worst in queue");

  worst_vertex_ = nullptr;
  worst_slack_ = slack_init_;
  for (Vertex *vertex : *queue_) {
    Slack slack = search_->wnsSlack(vertex, path_ap_index);
    if (delayLess(slack, worst_slack_, this))
      setWorstSlack(vertex, slack);
  }
}

void
WorstSlack::checkQueue(PathAPIndex path_ap_index)
{
  VertexSeq ends;
  for(Vertex *end : *search_->endpoints()) {
    if (delayLessEqual(search_->wnsSlack(end, path_ap_index),
		       slack_threshold_, this))
      ends.push_back(end);
  }
  WnsSlackLess slack_less(path_ap_index, this);
  sort(ends, slack_less);

  VertexSet end_set(graph_);
  for (Vertex *end : ends) {
    end_set.insert(end);
    if (!queue_->hasKey(end)
	&& delayLessEqual(search_->wnsSlack(end, path_ap_index),
			  slack_threshold_, this))
      report_->reportLine("WorstSlack queue missing %s %s < %s",
                          end->name(network_),
                          delayAsString(search_->wnsSlack(end, path_ap_index), this),
                          delayAsString(slack_threshold_, this));
  }

  for (Vertex *end : *queue_) {
    if (!end_set.hasKey(end))
      report_->reportLine("WorstSlack queue extra %s %s > %s",
                          end->name(network_),
                          delayAsString(search_->wnsSlack(end, path_ap_index), this),
                          delayAsString(slack_threshold_, this));
  }
}

void
WorstSlack::updateWorstSlack(Vertex *vertex,
			     SlackSeq &slacks,
			     PathAPIndex path_ap_index)
{
  Slack slack = slacks[path_ap_index];

  // Locking is required because ArrivalVisitor is called by multiple
  // threads.
  LockGuard lock(lock_);
  if (worst_vertex_
      && delayLess(slack, worst_slack_, this))
    setWorstSlack(vertex, slack);
  else if (vertex == worst_vertex_)
    // Mark worst slack as unknown (updated by findWorstSlack().
    worst_vertex_ = nullptr;

  if (!delayEqual(slack, slack_init_)
      && delayLessEqual(slack, slack_threshold_, this)) {
    debugPrint(debug_, "wns", 3, "insert %s %s",
               vertex->name(network_),
               delayAsString(slack, this));
    queue_->insert(vertex);
  }
  else {
    debugPrint(debug_, "wns", 3, "delete %s %s",
               vertex->name(network_),
               delayAsString(slack, this));
    queue_->erase(vertex);
  }
  //  checkQueue();
}

void
WorstSlack::setWorstSlack(Vertex *vertex,
			  Slack slack)
{
  debugPrint(debug_, "wns", 3, "%s %s",
             vertex->name(network_),
             delayAsString(slack, this));
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
  return delayLess(search_->wnsSlack(vertex1, path_ap_index_),
		   search_->wnsSlack(vertex2, path_ap_index_),
		   search_);
}

} // namespace
